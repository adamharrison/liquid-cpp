#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"

namespace Liquid {

    struct NodeType;
    struct Variable;

    struct Parser {
        const Context& context;


        // Represents everything that can be addressed by textual liquid.
        struct Variant {

            enum class Type {
                NIL,
                BOOL,
                FLOAT,
                INT,
                STRING,
                ARRAY,
                VARIABLE,
                POINTER
            };


            union {
                bool b;
                double f;
                long long i;
                string s;
                void* p;
                Variable* v;
                vector<Variant> a;
            };
            Type type;

            Variant() : type(Type::NIL) { }

            Variant(const Variant& v) : type(v.type) {
                switch (type) {
                    case Type::STRING:
                        new(&s) string;
                        s = v.s;
                    break;
                    case Type::ARRAY:
                        new(&a) vector<Variant>();
                        a = v.a;
                    break;
                    default:
                        f = v.f;
                    break;
                }
            }
            Variant(Variant&& v) : type(v.type) {
                switch (type) {
                    case Type::STRING:
                        new(&s) string;
                        s = std::move(v.s);
                    break;
                    case Type::ARRAY:
                        new(&a) vector<Variant>();
                        a = std::move(v.a);
                    break;
                    default:
                        f = v.f;
                    break;
                }
            }
            Variant(bool b) : b(b), type(Type::BOOL) {  }
            Variant(double f) : f(f), type(Type::FLOAT) {  }
            Variant(long long i) : i(i), type(Type::INT) { }
            Variant(const string& s) : s(s), type(Type::STRING) { }
            Variant(const char* s) : s(s), type(Type::STRING) { }
            Variant(Variable* p) : p(p), type(Type::VARIABLE) { }
            Variant(void* p) : p(p), type(Type::POINTER) { }
            Variant(const vector<Variant>& a) : a(a), type(Type::ARRAY) { }
            ~Variant() {
                switch (type) {
                    case Type::STRING:
                        s.~string();
                    break;
                    case Type::ARRAY:
                        a.~vector<Variant>();
                    break;
                    default:
                    break;
                }
            }

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
                switch (v.type) {
                    case Type::STRING:
                        if (type != Type::STRING) {
                            if (v.type == Type::ARRAY)
                                a.~vector<Variant>();
                            else
                                i = 0;
                            new(&s) string;
                        }
                        s = v.s;
                    break;
                    case Type::ARRAY:
                        if (type != Type::ARRAY) {
                            if (v.type == Type::STRING)
                                s.~string();
                            else
                                i = 0;
                            new(&a) vector<Variant>();
                        }
                        a = v.a;
                    break;
                    default:
                        f = v.f;
                    break;
                }
                return *this;
            }
            bool operator == (const Variant& v) const {
                if (type != v.type)
                    return false;
                switch (type) {
                    case Type::STRING:
                        return s == v.s;
                    case Type::INT:
                        return i == v.i;
                    case Type::ARRAY:
                        return a == v.a;
                    case Type::FLOAT:
                        return f == v.f;
                    case Type::NIL:
                        return true;
                    case Type::BOOL:
                        return b == v.b;
                    default:
                        return p == v.p;
                }
            }

            void inject(Variable& variable) {
                switch (type) {
                    case Type::STRING:
                        variable.assign(s);
                    break;
                    case Type::INT:
                        variable.assign(i);
                    break;
                    case Type::ARRAY: {
                        for (size_t i = 0; i < a.size(); ++i) {
                            Variable* target;
                            if (!variable.getArrayVariable(target, i, true))
                                break;
                            a[i].inject(*target);
                        }
                    } break;
                    case Type::FLOAT:
                        variable.assign(f);
                    break;
                    case Type::NIL:
                        variable.clear();
                    break;
                    case Type::BOOL:
                        variable.assign(b);
                    break;
                    case Type::VARIABLE:
                        variable.assign(*v);
                    break;
                    default:
                        variable.assign(p);
                    break;
                }
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
            Error(Type type) : type(type), column(0), row(0) {

            }
            Error(Type type, const std::string& message) : type(type), column(0), row(0), message(message) {

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
            Node(Node&& node) :type(node.type) {
                if (type) {
                    new(&children) vector<unique_ptr<Node>>(std::move(node.children));
                } else {
                    new(&variant) Variant(std::move(node.variant));
                }
            }
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

        enum class EBlockType {
            NONE,
            INTERMEDIATE,
            END
        };

        EBlockType blockType = EBlockType::NONE;

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
