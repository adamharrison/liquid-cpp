#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"
#include "context.h"

namespace Liquid {

    struct NodeType;
    struct Variable;

    struct Parser {
        const Context& context;


        struct Variant {
            union {
                bool b;
                double f;
                long long i;
                string s;
                void* p;

                struct {
                    int start;
                    int end;
                };
            };
            enum class Type {
                NIL,
                BOOL,
                FLOAT,
                INT,
                STRING,
                VARIABLE,
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
            Variant(bool b) : b(b), type(Type::BOOL) {  }
            Variant(double f) : f(f), type(Type::FLOAT) {  }
            Variant(long long i) : i(i), type(Type::INT) { }
            Variant(const string& s) : s(s), type(Type::STRING) { }
            Variant(const char* s) : s(s), type(Type::STRING) { }
            Variant(Variable* p) : p(p), type(Type::VARIABLE) { }
            Variant(void* p) : p(p), type(Type::POINTER) { }
            ~Variant() { if (type == Type::STRING) s.~string(); }

            bool isTruthy() const {
                return !(
                    (type == Parser::Variant::Type::BOOL && !b) ||
                    (type == Parser::Variant::Type::INT && !i) ||
                    (type == Parser::Variant::Type::FLOAT && !f) ||
                    (type == Parser::Variant::Type::POINTER && !p) ||
                    (type == Parser::Variant::Type::NIL)
                );
            }

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

        struct Error {
            enum Type {
                NONE,
                // Self-explamatory.
                UNKNOWN_TAG,
                UNKNOWN_OPERATOR,
                UNKNOWN_FILTER,
                // Weird symbol in weird place.
                INVALID_SYMBOL,
                // Was expecting somthing else, i.e. {{ i + }}; was expecting a number there.
                UNEXPECTED_END,
                UNBALANCED_GROUP
            };

            Type type;
            size_t column;
            size_t row;
            std::string message;

            Error() : type(Type::NONE) { }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            template <class T>
            Error(T& lexer, Type type) : type(type), column(lexer.column), row(lexer.row) {

            }
            template <class T>
            Error(T& lexer, Type type, const std::string& message) : type(type), column(lexer.column), row(lexer.row), message(message) {

            }
        };

        struct Node {
            const NodeType* type;
            size_t line;
            size_t column;

            union {
                Variant variant;
                vector<unique_ptr<Node>> children;
            };

            Node() : type(nullptr), variant() { }
            Node(const NodeType* type) : type(type), children() { }
            Node(const Node& node) :type(node.type) {
                if (type) {
                    new(&children) vector<unique_ptr<Node>>();
                    children.reserve(node.children.size());
                    for (auto it = node.children.begin(); it != node.children.end(); ++it)
                        children.push_back(make_unique<Node>(*it->get()));
                } else {
                    new(&variant) Variant(node.variant);
                }
            }
            Node(const Variant& v) : type(nullptr), variant(v) { }
            Node(Node&& node) :type(node.type), children(std::move(node.children)) { }
            ~Node() {
                if (type)
                    children.~vector<unique_ptr<Node>>();
            }

            string getString();

            Error validate(const Context& context) const;


            Node& operator = (const Node& n) {
                if (type)
                    children.~vector<unique_ptr<Node>>();
                if (n.type) {
                    new(&children) vector<unique_ptr<Node>>();
                    children.reserve(n.children.size());
                    for (auto it = n.children.begin(); it != n.children.end(); ++it)
                        children.push_back(make_unique<Node>(*it->get()));
                } else {
                    new(&variant) Variant();
                }
                type = n.type;
                return *this;
            }

            Node& operator = (Node&& n) {
                if (type)
                    children.~vector<unique_ptr<Node>>();
                if (n.type) {
                    new(&children) vector<unique_ptr<Node>>();
                    children = move(n.children);
                } else {
                    new(&variant) Variant();
                }
                type = n.type;
                return *this;
            }
        };

        struct ParserException : std::exception {
            Error error;
            std::string message;
            ParserException(const Error& error) : error(error) {
                char buffer[512];
                switch (error.type) {
                    case Error::Type::NONE: break;
                    case Error::Type::UNKNOWN_TAG:
                        sprintf(buffer, "Unknown tag '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Error::Type::UNKNOWN_OPERATOR:
                        sprintf(buffer, "Unknown operator '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Error::Type::UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Error::Type::INVALID_SYMBOL:
                        sprintf(buffer, "Invalid symbol '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Error::Type::UNEXPECTED_END:
                        sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.row, error.column);
                    break;
                    case Error::Type::UNBALANCED_GROUP:
                        sprintf(buffer, "Unbalanced end to group on line %lu, column %lu.", error.row, error.column);
                    break;
                }
                message = buffer;
            }

            const char* what() const noexcept {
               return message.data();
            }
        };



        enum class State {
            NODE,
            ARGUMENT
        };
        State state = State::NODE;
        enum class EFilterState {
            UNSET,
            COLON,
            NAME,
            ARGUMENTS
        };

        EFilterState filterState = EFilterState::UNSET;
        bool endBlock = false;

        vector<unique_ptr<Node>> nodes;
        vector<Error> errors;

        void pushError(const Error& error) {
            errors.push_back(error);
        }

        struct Lexer : Liquid::Lexer<Lexer> {
            Parser& parser;
            typedef Liquid::Lexer<Lexer> SUPER;

            bool literal(const char* str, size_t len);
            bool dot();
            bool colon();
            bool comma();

            bool startOutputBlock(bool suppress);
            bool endOutputBlock(bool suppress);
            bool endControlBlock(bool suppress);

            bool startVariableDereference();
            bool endVariableDereference();
            bool string(const char* str, size_t len);
            bool integer(long long i);
            bool floating(double f);

            bool openParenthesis();
            bool closeParenthesis();

            Lexer(const Context& context, Parser& parser) : Liquid::Lexer<Lexer>(context), parser(parser) { }
        };

        Lexer lexer;

        Parser(const Context& context) : context(context), lexer(context, *this) { }

        // Pops the last node in the stack, and then applies it as the last child of the node prior to it.
        bool popNode();
        // Pops nodes until it reaches a node of the given type.
        bool popNodeUntil(int type);

        Node parse(const char* buffer, size_t len);
        Node parse(const string& str) {
            return parse(str.data(), str.size());
        }
    };
}

#endif
