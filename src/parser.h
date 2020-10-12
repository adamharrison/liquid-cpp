#ifndef PARSER_H
#define PARSER_H

#include "context.h"
#include "lexer.h"

namespace Liquid {

    template <class Context>
    struct Parser {
        const Context& context;

        struct Variable;

        struct Node {
            const NodeType* type;
            vector<Variable> arguments;
            vector<unique_ptr<Node>> children;

            Node() : type(nullptr) { }
            Node(const NodeType* type) : type(type) { }
            Node(const Node& node) :type(node.type), arguments(node.arguments), children(node.children) { }
            Node(Node&& node) :type(node.type), arguments(std::move(node.arguments)), children(std::move(node.children)) { }
            ~Node() { }
        };
        struct Error {
            enum Type {
                UNKNOWN_TAG,
                INVALID_SYMBOL
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
            };
            enum class Type {
                NIL,
                FLOAT,
                INT,
                STRING
            };
            Type type;

            Variant() : s(), type(Type::NIL) { }

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
            ~Variant() { if (type == Type::STRING) s.~string(); }
        };
        // If a named variable, the first variant in the chain is a string literal.
        struct Variable {
            enum class Type {
                // Unitialized
                UNDEF,
                // Pass-through to the underlying first element of the chain.
                STATIC,
                // A list of literals and whatnot to dereference from.
                REFERENCE,
                // A list of literals and whatnot to dereference from, but the first is a literal that is the named argument.
                NAMED
            };
            Type type;
            vector<Variable> chain;

            Variable() : type(Type::UNDEF) { }
            Variable(Variable&& v) : type(v.type), chain(std::move(v.chain)) { }
            Variable(const Variable& v) : type(v.type), chain(v.chain) { type = v.type; }
            Variable(const Variant& v) : type(Type::STATIC) { chain.push_back(v); }
        };

        enum class State {
            NODE,
            ARGUMENT,
            VARIABLE,
            VARIABLE_FINISHED
        };

        State state = State::NODE;
        int argumentIdx = 0;


        vector<unique_ptr<Node>> nodes;
        vector<Variable> variables;
        vector<Error> errors;

        void pushError(const Error& error) {
            errors.push_back(error);
        }

        struct Lexer : Liquid::Lexer<Context, Lexer> {
            Parser& parser;
            typedef Liquid::Lexer<Context, Lexer> SUPER;

            bool literal(const char* str, size_t len) {
                switch (parser.state) {
                    case Parser::State::NODE: {
                        switch (this->state) {
                            case SUPER::State::CONTROL: {
                                if (parser.argumentIdx == 0) {
                                    if (len > 3 && strncmp(str, "end", 3) == 0) {
                                        std::string typeName = std::string(&str[3], len - 3);
                                        const NodeType* type = SUPER::context.getType(typeName);
                                        if (!type) {
                                            parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, std::string(str, len)));
                                            return false;
                                        }
                                        parser.nodes.pop_back();
                                    } else {v
                                        std::string typeName = std::string(str, len);
                                        const NodeType* type = SUPER::context.getType(typeName);
                                        if (!type) {
                                            parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, typeName));
                                            return false;
                                        }
                                        parser.nodes.push_back(std::make_unique<Node>(type));
                                    }
                                } else {
                                }
                            } break;
                            case SUPER::State::OUTPUT:
                            case SUPER::State::RAW:
                            case SUPER::State::INITIAL:
                                assert(parser.nodes.back()->type == SUPER::context.getConcatenationNodeType());
                                parser.nodes.back()->children.push_back(std::make_unique<Node>(SUPER::context.getStaticNodeType()));
                                parser.nodes.back()->children.back()->arguments.push_back(Variable(Variant(std::string(str, len))));
                            break;
                        }
                    } break;
                    case Parser::State::ARGUMENT:
                        parser.state = Parser::State::VARIABLE;
                        parser.variables.push_back(Variable());
                        parser.variables.back().chain.push_back(Variant(std::string(str, len)));
                    break;
                    case Parser::State::VARIABLE:
                        parser.variables.back().chain.push_back(Variant(std::string(str, len)));
                        parser.state = Parser::State::VARIABLE_FINISHED;
                    break;
                    case Parser::State::VARIABLE_FINISHED:
                    break;
                }
                return true;
            }

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
            bool colon() {
                if (parser.variables.size() == 0 || parser.variables.back().chain.size() != 1 || parser.variables.back().type != Variable::Type::REFERENCE) {
                    parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                    return false;
                }
                parser.variables.back().type = Variable::Type::NAMED;
                return true;
            }
            bool comma() {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE_FINISHED:
                        parser.state = Parser::State::ARGUMENT;
                        assert(parser.variables.size() == 1);
                        parser.nodes.back().get()->arguments.push_back(parser.variables.front());
                        parser.variables.pop_back();
                    break;
                }
                return true;
            }

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

            bool endVariableDereference() {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE_FINISHED:
                        parser.state = Parser::State::VARIABLE;
                        assert(parser.variables.size() >= 2);
                        parser.variables[parser.variables.size()-2].chain.push_back(parser.variables[parser.variables.size()-1]);
                        parser.variables.pop_back();
                    break;
                }
                return true;
            }

            bool string(const char* str, size_t len) {
                 switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE_FINISHED:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE:
                        parser.variables.back().chain.push_back(Variant(std::string(str, len)));
                    break;
                }
                return true;
            }
            bool integer(long long i) {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE_FINISHED:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE:
                        parser.variables.back().chain.push_back(Variant(i));
                    break;
                }
                return true;
            }
            bool floating(double f) {
                switch (parser.state) {
                    case Parser::State::NODE:
                    case Parser::State::ARGUMENT:
                    case Parser::State::VARIABLE_FINISHED:
                        parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                        return false;
                    break;
                    case Parser::State::VARIABLE:
                        parser.variables.back().chain.push_back(Variant(f));
                    break;
                }
                return true;
            }

            Lexer(const Context& context, Parser& parser) : Liquid::Lexer<Context, Lexer>(context), parser(parser) { }
        };

        Lexer lexer;

        Parser(const Context& context) : context(context), lexer(context, *this) {

        }

        Node parse(const char* buffer, size_t len) {
            nodes.push_back(std::make_unique<Node>(context.getConcatenationNodeType()));
            lexer.parse(buffer, len);
            assert(nodes.size() > 0);
            Node node = std::move(*nodes[nodes.size()-1].get());
            nodes.clear();
            return node;
        }
        Node parse(const string& str) {
            return parse(str.data(), str.size());
        }
    };
}

#endif
