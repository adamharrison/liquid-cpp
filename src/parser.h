#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"
#include "context.h"

namespace Liquid {

    struct NodeType;

    struct Parser {
        const Context& context;

        struct Variable;
        struct Variant;

        struct Node {
            const NodeType* type;
            vector<Variable> arguments;
            vector<unique_ptr<Node>> children;

            Node() : type(nullptr) { }
            Node(const NodeType* type) : type(type) { }
            Node(const Node& node) :type(node.type), arguments(node.arguments) {
                children.reserve(node.children.size());
                for (auto it = node.children.begin(); it != node.children.end(); ++it)
                    children.push_back(make_unique<Node>(*it->get()));
            }
            Node(const Variable& v) : type(nullptr) { arguments.push_back(v); }
            Node(const Variant& v) : type(nullptr) { arguments.push_back(Variable(v)); }
            Node(Node&& node) :type(node.type), arguments(std::move(node.arguments)), children(std::move(node.children)) { }
            ~Node() { }

            string getString();

            Variable& variable() { assert(!type && arguments.size() == 1); return arguments[0]; }

            Node& operator = (const Node& n) {
                type = n.type;
                arguments = n.arguments;
                children.clear();
                children.reserve(n.children.size());
                for (auto it = n.children.begin(); it != n.children.end(); ++it)
                    children.push_back(make_unique<Node>(*it->get()));
                return *this;
            }
        };
        struct Error {
            enum Type {
                // Self-explamatory.
                UNKNOWN_TAG,
                UNKNOWN_OPERATOR,
                // Weird symbol in weird place.
                INVALID_SYMBOL,
                // Was expecting somthing else, i.e. {{ i + }}; was expecting a number there.
                UNEXPECTED_END
            };

            Type type;
            size_t column;
            size_t row;
            std::string message;

            template <class T>
            Error(T& lexer, Type type) : type(type), column(lexer.column), row(lexer.row) {

            }
            template <class T>
            Error(T& lexer, Type type, const std::string& message) : type(type), column(lexer.column), row(lexer.row), message(message) {

            }
        };


        struct Variant {
            union {
                double f;
                long long i;
                string s;
                void* p;
            };
            enum class Type {
                NIL,
                FLOAT,
                INT,
                STRING,
                POINTER
            };
            Type type;

            Variant() : type(Type::NIL) { }

            Variant(const Variant& v) : s(), type(v.type) {
                if (type == Type::STRING)
                    s = v.s;
                else
                    f = v.f;
            }
            Variant(Variant&& v) : s(), type(v.type) {
                if (type == Type::STRING)
                    s = std::move(v.s);
                else
                    f = v.f;
            }
            Variant(double f) : f(f), type(Type::FLOAT) {  }
            Variant(long long i) : i(i), type(Type::INT) { }
            Variant(const string& s) : s(s), type(Type::STRING) { }
            Variant(const char* s) : s(s), type(Type::STRING) { }
            Variant(void* p) : p(p), type(Type::POINTER) { }
            ~Variant() { if (type == Type::STRING) s.~string(); }

            Variant& operator = (const Variant& v) {
                if (v.type == Type::STRING) {
                    i = 0;
                    s = v.s;
                } else {
                    if (type == Type::STRING)
                        s.~string();
                    i = v.i;
                }
                return *this;
            }
        };
        // If a named variable, the first variant in the chain is a string literal.
        struct Variable {
            enum class Type {
                // Unitialized
                UNDEF,
                // Represented by a variant.
                STATIC,
                // A list of literals and whatnot to dereference from in the chain.
                REFERENCE,
                // A list of literals and whatnot to dereference from, but the first is a literal that is the named argument.
                NAMED,
                // A pointer to the dynamic variable.
                DYNAMIC
            };
            Type type;
            Variant variant;
            vector<Variable> chain;

            Variable() : type(Type::UNDEF) { }
            Variable(Type type) : type(type) { }
            Variable(Variable&& v) : type(v.type), variant(std::move(v.variant)), chain(std::move(v.chain)) { }
            Variable(const Variable& v) : type(v.type), variant(v.variant), chain(v.chain) { type = v.type; }
            Variable(const Variant& v) : type(v.type == Variant::Type::POINTER ? Type::DYNAMIC : Type::STATIC), variant(v) { }
            Variable(Variant&& v) : type(Type::STATIC), variant(std::move(v)) { }

            Variable& operator = (const Variable& v) {
                type = v.type;
                variant = v.variant;
                chain = v.chain;
                return *this;
            }
        };

        enum class State {
            NODE,
            ARGUMENT,
            VARIABLE,
            VARIABLE_FINISHED
        };

        State state = State::NODE;
        int argumentIdx = 0;
        int argumentNodeIdx = 0;


        vector<unique_ptr<Node>> nodes;
        vector<Error> errors;

        void pushError(const Error& error) {
            errors.push_back(error);
        }

        struct Lexer : Liquid::Lexer<Lexer> {
            Parser& parser;
            typedef Liquid::Lexer<Lexer> SUPER;

            bool literal(const char* str, size_t len);
            bool dot() {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE_FINISHED:
                        parser.state = Parser::State::VARIABLE;
                    break;
                }
                return true;
            }

            bool colon();
            bool comma();

            bool startOutputBlock(bool suppress);
            bool endOutputBlock(bool suppress);

            bool startVariableDereference() {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE_FINISHED:
                        parser.state = Parser::State::ARGUMENT;
                    break;
                }
                return true;
            }

            bool endVariableDereference();
            bool string(const char* str, size_t len);
            bool integer(long long i);
            bool floating(double f);

            Lexer(const Context& context, Parser& parser) : Liquid::Lexer<Lexer>(context), parser(parser) { }
        };

        Lexer lexer;

        Parser(const Context& context) : context(context), lexer(context, *this) {

        }

        Variable& activeVariable() {
            return nodes.back()->arguments.back();
        }

        // Called at the end of a control tag, output tag, or comma. Closes up an argument and appends it to the apppropraite node.
        void closeVariable();

        Node parse(const char* buffer, size_t len);
        Node parse(const string& str) {
            return parse(str.data(), str.size());
        }
    };
}

#endif
