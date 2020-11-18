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
            Variant(string&& s) : s(std::move(s)), type(Type::STRING) { }
            Variant(const char* s) : s(s), type(Type::STRING) { }
            Variant(Variable* p) : p(p), type(Type::VARIABLE) { }
            Variant(void* p) : p(p), type(Type::POINTER) { }
            Variant(const vector<Variant>& a) : a(a), type(Type::ARRAY) { }
            Variant(vector<Variant>&& a) : a(std::move(a)), type(Type::ARRAY) { }
            Variant(std::initializer_list<Variant> list) : a(list), type(Type::ARRAY) { }
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
            bool isNumeric() const {
                return type == Type::INT || type == Type::FLOAT;
            }

            string getString() const {
                switch (type) {
                    case Type::STRING:
                        return s;
                    case Type::FLOAT:
                        return std::to_string(f);
                    case Type::INT:
                        return std::to_string(i);
                    default:
                        return string();
                }
            }

            long long getInt() const {
                switch (type) {
                    case Type::INT:
                        return i;
                    case Type::FLOAT:
                        return (long long)f;
                    default:
                        return 0;
                }
            }

            double getFloat() const {
                switch (type) {
                    case Type::INT:
                        return (double)i;
                    case Type::FLOAT:
                        return f;
                    default:
                        return 0.0;
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
                        variable.clear();
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

            static Variant parse(Variable& variable) {
                switch (variable.getType()) {
                    case Variable::Type::OTHER:
                    case Variable::Type::DICTIONARY:
                    case Variable::Type::ARRAY:
                        return Variant(&variable);
                    break;
                    case Variable::Type::BOOL: {
                        bool b;
                        if (variable.getBool(b))
                            return Variant(b);
                    } break;
                    case Variable::Type::INT: {
                        long long i;
                        if (variable.getInteger(i))
                            return Variant(i);
                    } break;
                    case Variable::Type::FLOAT: {
                        double f;
                        if (variable.getFloat(f))
                            return Variant(f);
                    } break;
                    case Variable::Type::STRING: {
                        string s;
                        if (variable.getString(s))
                            return Variant(move(s));
                    } break;
                    case Variable::Type::NIL:
                        return Variant();
                }
                return Variant();
            }

            size_t hash() const {
                switch (type) {
                    case Type::STRING:
                        return std::hash<string>{}(s);
                    case Type::INT:
                        return std::hash<long long>{}(i);
                    case Type::ARRAY:
                        return 0;
                    case Type::FLOAT:
                        return std::hash<double>{}(f);
                    case Type::NIL:
                        return 0;
                    case Type::BOOL:
                        return std::hash<bool>{}(b);
                    case Type::VARIABLE:
                        return std::hash<Variable*>{}(v);
                    default:
                        return std::hash<void*>{}(p);
                }
            }

            bool operator < (const Variant& v) const {
                switch (type) {
                    case Type::INT:
                        return i < v.getInt();
                    break;
                    case Type::FLOAT:
                        return f < v.getFloat();
                    break;
                    case Type::STRING:
                        if (v.type == Type::STRING)
                            return s < v.s;
                        return s < v.getString();
                    break;
                    default:
                        return p < v.p;
                    break;
                }
                return false;
            }
        };

        struct Error {
            enum Type {
                NONE,
                // Self-explamatory.
                UNKNOWN_TAG,
                UNKNOWN_OPERATOR,
                UNKNOWN_OPERATOR_OR_QUALIFIER,
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
            Node(Variant&& v) : type(nullptr), variant(std::move(v)) { }
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

            string getString() const {
                assert(!type);
                return variant.getString();
            }

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



        enum class State {
            NODE,
            ARGUMENT
        };
        enum class EFilterState {
            UNSET,
            COLON,
            NAME,
            ARGUMENTS
        };


        enum class EBlockType {
            NONE,
            INTERMEDIATE,
            END
        };

        State state;
        EFilterState filterState;
        EBlockType blockType;
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


        struct Exception : std::exception {
            Parser::Error parserError;
            Lexer::Error lexerError;
            std::string message;
            Exception(const Parser::Error& error) : parserError(error) {
                char buffer[512];
                switch (parserError.type) {
                    case Parser::Error::Type::NONE: break;
                    case Parser::Error::Type::UNKNOWN_TAG:
                        sprintf(buffer, "Unknown tag '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::UNKNOWN_OPERATOR:
                        sprintf(buffer, "Unknown operator '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::UNKNOWN_OPERATOR_OR_QUALIFIER:
                        sprintf(buffer, "Unknown operator, or qualifier '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::INVALID_SYMBOL:
                        sprintf(buffer, "Invalid symbol '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::UNEXPECTED_END: {
                        if (!error.message.empty())
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.row, error.column);
                    } break;
                    case Parser::Error::Type::UNBALANCED_GROUP:
                        sprintf(buffer, "Unbalanced end to group on line %lu, column %lu.", error.row, error.column);
                    break;
                }
                message = buffer;
            }

            Exception(const Lexer::Error& error) : lexerError(error) {
                char buffer[512];
                switch (lexerError.type) {
                    case Lexer::Error::Type::NONE: break;
                    case Lexer::Error::Type::UNEXPECTED_END:
                        if (!error.message.empty())
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.row, error.column);
                    break;
                }
                message = buffer;
            }

            const char* what() const noexcept {
               return message.data();
            }
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
