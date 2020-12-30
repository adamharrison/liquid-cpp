#ifndef COMMON_H
#define COMMON_H

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdarg>
#include <chrono>

#include "interface.h"

namespace Liquid {
    template<typename T>
    using vector = std::vector<T>;
    using string = std::string;
    template<typename S, typename T>
    using unordered_map = std::unordered_map<S,T>;
    template<typename T>
    using unique_ptr = std::unique_ptr<T>;
    template<typename T>
    using shared_ptr = std::shared_ptr<T>;
    using std::make_unique;
    using std::move;


    struct Exception : public std::exception {
        std::string internal;

        Exception() { }
        Exception(const char* format, ...) {
            char buffer[512];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            internal = buffer;
        }
        const char * what () const throw () {
            return internal.c_str();
        }
    };

    // Represents the underlying variable implementation that is passed in to liquid.
    struct Variable {
        void* pointer;

        Variable() :pointer(nullptr) { }
        Variable(void* pointer) : pointer(pointer) { }

        operator bool() const { return pointer != nullptr; }
        operator void*() { return pointer; }
        operator const void*() const { return pointer; }
        operator void** () { return &pointer; }

        Variable& operator = (void* pointer) { this->pointer = pointer; return *this; }

        bool exists() const { return pointer; }
    };

    enum EFalsiness {
        FALSY_FALSE             = 0,
        FALSY_0                 = 1,
        FALSY_EMPTY_STRING      = 2,
        FALSY_NIL               = 4
    };
    // Represents everything that can be addressed by textual liquid. A built-in type.
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
            Variable v;
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
        Variant(Variable v) : v(v), type(Type::VARIABLE) { }
        Variant(void* p) : p(p), type(p ? Type::POINTER : Type::NIL) { }
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

        bool isTruthy(EFalsiness falsiness) const {
            return !(
                (type == Variant::Type::BOOL && !b) ||
                ((falsiness & FALSY_0) && type == Variant::Type::INT && !i) ||
                ((falsiness & FALSY_0) && type == Variant::Type::FLOAT && !f) ||
                ((falsiness & FALSY_NIL) && type == Variant::Type::POINTER && !p) ||
                ((falsiness & FALSY_NIL) && type == Variant::Type::NIL) ||
                ((falsiness & FALSY_EMPTY_STRING) && type == Variant::Type::STRING && s.size() == 0)
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

        Variant& operator = (Variant&& v) {
            switch (v.type) {
                case Type::STRING:
                    if (type != Type::STRING) {
                        if (v.type == Type::ARRAY)
                            a.~vector<Variant>();
                        else
                            i = 0;
                        new(&s) string;
                    }
                    s = move(v.s);
                break;
                case Type::ARRAY:
                    if (type != Type::ARRAY) {
                        if (v.type == Type::STRING)
                            s.~string();
                        else
                            i = 0;
                        new(&a) vector<Variant>();
                    }
                    a = move(v.a);
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
                case Type::BOOL:
                    return b ? "true" : "false";
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
                case Type::STRING:
                    return atoll(s.c_str());
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
                case Type::STRING:
                    return atof(s.c_str());
                default:
                    return 0.0;
            }
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

    struct NodeType;

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
            else
                variant.~Variant();
        }

        string getString() const {
            assert(!type);
            return variant.getString();
        }

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
                new(&variant) Variant(move(n.variant));
            }
            type = n.type;
            return *this;
        }

        template <class T>
        void walk(T callback) const {
            callback(*this);
            if (type) {
                for (auto it = children.begin(); it != children.end(); ++it)
                    (*it)->walk(callback);
            }
        }
    };
}

#endif
