
#ifndef CPPVARAIBLE_H
#define CPPVARIABLE_H

#include "common.h"

namespace Liquid {
    struct CPPVariable : Variable {
        union {
            bool b;
            string s;
            double f;
            long long i;
            vector<unique_ptr<CPPVariable>> a;
            unordered_map<string, unique_ptr<CPPVariable>> d;
        };
        Type type;

        CPPVariable() :type(Type::NIL) { }
        CPPVariable(bool b) : b(b), type(Type::BOOL) { }
        CPPVariable(long long i) : i(i), type(Type::INT) { }
        CPPVariable(int i) : CPPVariable((long long)i) { }
        CPPVariable(double f) : f(f), type(Type::FLOAT) { }
        CPPVariable(const string& s) : s(s), type(Type::STRING) { }
        CPPVariable(const char* s) : CPPVariable(string(s)) { }
        CPPVariable(void* nil) : type(Type::NIL) { }
        CPPVariable(const CPPVariable& v) :type(Type::NIL) { assign(v); }
        CPPVariable(CPPVariable&& v) : type(Type::NIL) { this->move(std::move(v)); }
        CPPVariable(std::initializer_list<CPPVariable> l) {
            assign(vector<unique_ptr<CPPVariable>>());
            a.reserve(l.size());
            for (auto it = l.begin(); it != l.end(); ++it)
                a.push_back(make_unique<CPPVariable>(*it));
        }


        ~CPPVariable() { clear(); }

        void clear() {
            switch (type) {
                case Type::STRING:
                    s.~string();
                break;
                case Type::DICTIONARY:
                    d.~unordered_map<string, unique_ptr<CPPVariable>>();
                break;
                case Type::ARRAY:
                    a.~vector<unique_ptr<CPPVariable>>();
                break;
                default:
                break;
            }
            type = Type::NIL;
        }


        void move(CPPVariable&& v) {
            clear();
            type = v.type;
            switch (type) {
                case Type::NIL:
                    type = v.type;
                break;
                case Type::BOOL:
                    b = v.b;
                break;
                case Type::FLOAT:
                    f = v.f;
                break;
                case Type::INT:
                    i = v.i;
                break;
                case Type::STRING:
                    new(&s) string(std::move(v.s));
                break;
                case Type::ARRAY:
                    new(&a) vector<unique_ptr<CPPVariable>>(std::move(v.a));
                break;
                case Type::DICTIONARY:
                    new(&d) unordered_map<string, unique_ptr<CPPVariable>>(std::move(v.d));
                break;
                case Type::OTHER:
                    assert(false);
                break;
            }
            v.type = Type::NIL;
        }

        void assign(long long i) { clear(); this->i = i; type = Type::INT; }
        void assign(int i) { assign((long long)i); }
        void assign(bool b) { clear(); this->b = b; type = Type::BOOL; }
        void assign(double f) { clear(); this->f = f; type = Type::FLOAT; }
        void assign(const string& s) { clear(); new(&this->s) string(); this->s = s; type = Type::STRING; }
        void assign(const char* s) { assign(string(s)); }
        void assign(const vector<unique_ptr<CPPVariable>>& a) {
            clear();
            new(&this->a) vector<unique_ptr<CPPVariable>>();
            this->a.reserve(a.size());
            type = Type::ARRAY;
            for (auto it = a.begin(); it != a.end(); ++it)
                this->a.push_back(make_unique<CPPVariable>(**it));
        }
        void assign(const unordered_map<string, unique_ptr<CPPVariable>>& d) {
            clear();
            new(&this->d) unordered_map<string, unique_ptr<CPPVariable>>();
            type = Type::DICTIONARY;
            for (auto it = d.begin(); it != d.end(); ++it)
                this->d[it->first] = make_unique<CPPVariable>(*it->second);
        }
        void assign(const CPPVariable& v) {
            clear();
            switch (v.type) {
                case Type::NIL:
                    type = v.type;
                break;
                case Type::BOOL:
                    assign(v.b);
                break;
                case Type::FLOAT:
                    assign(v.f);
                break;
                case Type::INT:
                    assign(v.i);
                break;
                case Type::STRING:
                    assign(v.s);
                break;
                case Type::ARRAY:
                    assign(v.a);
                break;
                case Type::DICTIONARY:
                    assign(v.d);
                break;
                case Type::OTHER:
                    assert(false);
                break;
            }
        }

        void assign(const Variable& v) {
            assign(static_cast<const CPPVariable&>(v));
        }

        CPPVariable& operator[](const string& s) {
            if (type == Type::NIL) {
                new(&this->d) unordered_map<string, unique_ptr<CPPVariable>>();
                type = Type::DICTIONARY;
            }
            assert(type == Type::DICTIONARY);
            auto it = d.find(s);
            if (it != d.end())
                return *it->second.get();
            return *d.emplace(s, make_unique<CPPVariable>()).first->second.get();
        }
        CPPVariable& operator[](size_t idx) {
            if (type == Type::NIL) {
                new(&this->a) vector<unique_ptr<CPPVariable>>();
                type = Type::ARRAY;
            }
            assert(type == Type::ARRAY);
            if (a.size() < idx+1) {
                size_t oldSize = a.size();
                a.resize(idx+1);
                for (size_t i = oldSize; i < idx+1; ++i)
                    a[i] = make_unique<CPPVariable>();
            }
            return *a[idx].get();
        }

        CPPVariable& operator = (const std::string& s) { assign(s); return *this; }
        CPPVariable& operator = (const char* s) { assign(std::string(s)); return *this; }
        CPPVariable& operator = (double f) { assign(f); return *this; }
        CPPVariable& operator = (bool b) { assign(b); return *this; }
        CPPVariable& operator = (long long i) { assign(i); return *this; }
        CPPVariable& operator = (int i) { assign(i); return *this; }
        CPPVariable& operator = (CPPVariable&& v) { move(std::move(v)); return *this; }
        /*CPPVariable& operator = (const CPPVariable& v) { assign(v); return *this; }*/


        Type getType() const  {
            return type;
        }

        bool getBool(bool& b) const {
            if (type != Type::BOOL)
                return false;
            b = this->b;
            return true;
        }
        bool getString(std::string& s) const  {
            if (type != Type::STRING)
                return false;
            s = this->s;
            return true;
        }
        bool getInteger(long long& i) const  {
            if (type != Type::INT)
                return false;
            i = this->i;
            return true;
        }
        bool getFloat(double& f) const  {
            if (type != Type::FLOAT)
                return false;
            f = this->f;
            return true;
        }
        bool getDictionaryVariable(Variable*& variable, const std::string& key) const  {
            if (type != Type::DICTIONARY)
                return false;
            auto it = d.find(key);
            if (it == d.end()) {
                return false;
            } else {
                variable = it->second.get();
            }
            return true;
        }

        bool getDictionaryVariable(Variable*& variable, const std::string& key, bool createOnNotExists)  {
            if (type != Type::DICTIONARY) {
                if (type != Type::NIL)
                    return false;
                assign(unordered_map<string, unique_ptr<CPPVariable>>());
            }
            auto it = d.find(key);
            if (it == d.end()) {
                if (!createOnNotExists)
                    return false;
                variable = &(*this)[key];
            } else {
                variable = it->second.get();
            }
            return true;
        }

        bool getArrayVariable(Variable*& variable, size_t idx, bool createOnNotExists) const  {
            if (type != Type::ARRAY)
                return false;
            if (idx >= a.size()) {
                if (!createOnNotExists)
                    return false;
                const_cast<CPPVariable*>(this)->a.resize(idx+1);
            }
            variable = &(*const_cast<CPPVariable*>(this))[idx];
            return true;
        }

        long long getArraySize() const {
            if (type != Type::ARRAY)
                return -1;
            return a.size();
        }

        bool iterate(bool (*callback)(Variable* variable, void* data), void* data, int start = 0, int limit = -1) const {
            if (type != Type::ARRAY)
                return false;
            if (limit < 0)
                limit = (int)a.size() + limit;
            if (start < 0)
                start = 0;
            for (int i = start; i <= limit; ++i) {
                if (!callback(a[i].get(), data))
                    break;
            }
            return true;
        }
    };
}

#endif
