
#ifndef CPPVARIABLE_H
#define CPPVARIABLE_H

#include "common.h"
#include "context.h"

namespace Liquid {

    struct CPPVariable {
        union {
            bool b;
            string s;
            double f;
            long long i;
            void* p;
            vector<unique_ptr<CPPVariable>> a;
            unordered_map<string, unique_ptr<CPPVariable>> d;
        };
        ELiquidVariableType type;

        CPPVariable() : type(LIQUID_VARIABLE_TYPE_NIL) { }
        CPPVariable(bool b) : b(b), type(LIQUID_VARIABLE_TYPE_BOOL) { }
        CPPVariable(long long i) : i(i), type(LIQUID_VARIABLE_TYPE_INT) { }
        CPPVariable(int i) : CPPVariable((long long)i) { }
        CPPVariable(double f) : f(f), type(LIQUID_VARIABLE_TYPE_FLOAT) { }
        CPPVariable(const string& s) : s(s), type(LIQUID_VARIABLE_TYPE_STRING) { }
        CPPVariable(const char* s) : CPPVariable(string(s)) { }
        CPPVariable(void* nil) : type(LIQUID_VARIABLE_TYPE_NIL) { }
        CPPVariable(const CPPVariable& v) :type(LIQUID_VARIABLE_TYPE_NIL) { assign(v); }
        CPPVariable(CPPVariable&& v) : type(LIQUID_VARIABLE_TYPE_NIL) { this->move(std::move(v)); }
        CPPVariable(std::initializer_list<CPPVariable> l) : type(LIQUID_VARIABLE_TYPE_NIL) {
            assign(vector<unique_ptr<CPPVariable>>());
            a.reserve(l.size());
            for (auto it = l.begin(); it != l.end(); ++it)
                a.push_back(make_unique<CPPVariable>(*it));
        }
        CPPVariable(const unordered_map<string, unique_ptr<CPPVariable>>& d) : type(LIQUID_VARIABLE_TYPE_NIL) {
            assign(d);
        }

        operator Variable () { return Variable({ this }); }


        ~CPPVariable() { clear(); }

        void clear() {
            switch (type) {
                case LIQUID_VARIABLE_TYPE_STRING:
                    s.~string();
                break;
                case LIQUID_VARIABLE_TYPE_DICTIONARY:
                    d.~unordered_map<string, unique_ptr<CPPVariable>>();
                break;
                case LIQUID_VARIABLE_TYPE_ARRAY:
                    a.~vector<unique_ptr<CPPVariable>>();
                break;
                default:
                break;
            }
            type = LIQUID_VARIABLE_TYPE_NIL;
        }


        void move(CPPVariable&& v) {
            clear();
            type = v.type;
            switch (type) {
                case LIQUID_VARIABLE_TYPE_NIL:
                    type = v.type;
                break;
                case LIQUID_VARIABLE_TYPE_BOOL:
                    b = v.b;
                break;
                case LIQUID_VARIABLE_TYPE_FLOAT:
                    f = v.f;
                break;
                case LIQUID_VARIABLE_TYPE_INT:
                    i = v.i;
                break;
                case LIQUID_VARIABLE_TYPE_STRING:
                    new(&s) string(std::move(v.s));
                break;
                case LIQUID_VARIABLE_TYPE_ARRAY:
                    new(&a) vector<unique_ptr<CPPVariable>>(std::move(v.a));
                break;
                case LIQUID_VARIABLE_TYPE_DICTIONARY:
                    new(&d) unordered_map<string, unique_ptr<CPPVariable>>(std::move(v.d));
                break;
                case LIQUID_VARIABLE_TYPE_OTHER:
                    assert(false);
                break;
            }
            v.type = LIQUID_VARIABLE_TYPE_NIL;
        }

        void assign(long long i) { clear(); this->i = i; type = LIQUID_VARIABLE_TYPE_INT; }
        void assign(int i) { assign((long long)i); }
        void assign(bool b) { clear(); this->b = b; type = LIQUID_VARIABLE_TYPE_BOOL; }
        void assign(double f) { clear(); this->f = f; type = LIQUID_VARIABLE_TYPE_FLOAT; }
        void assign(const string& s) { clear(); new(&this->s) string(); this->s = s; type = LIQUID_VARIABLE_TYPE_STRING; }
        void assign(const char* s) { assign(string(s)); }
        void assign(const vector<unique_ptr<CPPVariable>>& a) {
            clear();
            new(&this->a) vector<unique_ptr<CPPVariable>>();
            this->a.reserve(a.size());
            type = LIQUID_VARIABLE_TYPE_ARRAY;
            for (auto it = a.begin(); it != a.end(); ++it)
                this->a.push_back(make_unique<CPPVariable>(**it));
        }
        void assign(const unordered_map<string, unique_ptr<CPPVariable>>& d) {
            clear();
            new(&this->d) unordered_map<string, unique_ptr<CPPVariable>>();
            type = LIQUID_VARIABLE_TYPE_DICTIONARY;
            for (auto it = d.begin(); it != d.end(); ++it)
                this->d[it->first] = make_unique<CPPVariable>(*it->second.get());
        }
        void assign(const CPPVariable& v) {
            clear();
            switch (v.type) {
                case LIQUID_VARIABLE_TYPE_NIL:
                    type = v.type;
                break;
                case LIQUID_VARIABLE_TYPE_BOOL:
                    assign(v.b);
                break;
                case LIQUID_VARIABLE_TYPE_FLOAT:
                    assign(v.f);
                break;
                case LIQUID_VARIABLE_TYPE_INT:
                    assign(v.i);
                break;
                case LIQUID_VARIABLE_TYPE_STRING:
                    assign(v.s);
                break;
                case LIQUID_VARIABLE_TYPE_ARRAY:
                    assign(v.a);
                break;
                case LIQUID_VARIABLE_TYPE_DICTIONARY:
                    assign(v.d);
                break;
                case LIQUID_VARIABLE_TYPE_OTHER:
                    assert(false);
                break;
            }
        }

        CPPVariable& operator[](const string& s) {
            if (type == LIQUID_VARIABLE_TYPE_NIL) {
                new(&this->d) unordered_map<string, unique_ptr<CPPVariable>>();
                type = LIQUID_VARIABLE_TYPE_DICTIONARY;
            }
            assert(type == LIQUID_VARIABLE_TYPE_DICTIONARY);
            auto it = d.find(s);
            if (it != d.end())
                return *it->second.get();
            return *d.emplace(s, make_unique<CPPVariable>()).first->second.get();
        }
        CPPVariable& operator[](size_t idx) {
            if (type == LIQUID_VARIABLE_TYPE_NIL) {
                new(&this->a) vector<unique_ptr<CPPVariable>>();
                type = LIQUID_VARIABLE_TYPE_ARRAY;
            }
            assert(type == LIQUID_VARIABLE_TYPE_ARRAY);
            if (a.size() < idx+1) {
                size_t oldSize = a.size();
                a.resize(idx+1);
                for (size_t i = oldSize; i < idx+1; ++i)
                    a[i] = make_unique<CPPVariable>();
            }
            return *a[idx].get();
        }
        void pushBack(const CPPVariable& variable) {
            if (type == LIQUID_VARIABLE_TYPE_NIL) {
                new(&this->a) vector<unique_ptr<CPPVariable>>();
                type = LIQUID_VARIABLE_TYPE_ARRAY;
            }
            a.push_back(make_unique<CPPVariable>(variable));
        }
        void pushBack(unique_ptr<CPPVariable> variable) {
            if (type == LIQUID_VARIABLE_TYPE_NIL) {
                new(&this->a) vector<unique_ptr<CPPVariable>>();
                type = LIQUID_VARIABLE_TYPE_ARRAY;
            }
            a.push_back(std::move(variable));
        }

        CPPVariable& operator = (const std::string& s) { assign(s); return *this; }
        CPPVariable& operator = (const char* s) { assign(std::string(s)); return *this; }
        CPPVariable& operator = (double f) { assign(f); return *this; }
        CPPVariable& operator = (bool b) { assign(b); return *this; }
        CPPVariable& operator = (long long i) { assign(i); return *this; }
        CPPVariable& operator = (int i) { assign(i); return *this; }
        CPPVariable& operator = (CPPVariable&& v) { move(std::move(v)); return *this; }
        CPPVariable& operator = (const CPPVariable& v) { assign(v); return *this; }
        CPPVariable& operator = (const vector<unique_ptr<CPPVariable>>& a) { assign(a); return *this; }


        ELiquidVariableType getType() const  {
            return type;
        }

        bool getBool(bool& b) const {
            if (type != LIQUID_VARIABLE_TYPE_BOOL)
                return false;
            b = this->b;
            return true;
        }

        bool getTruthy() const {
            return !(
                (type == LIQUID_VARIABLE_TYPE_BOOL && !b) ||
                (type == LIQUID_VARIABLE_TYPE_INT && !i) ||
                (type == LIQUID_VARIABLE_TYPE_FLOAT && !f) ||
                (type == LIQUID_VARIABLE_TYPE_OTHER && !p) ||
                (type == LIQUID_VARIABLE_TYPE_NIL)
            );
        }

        bool getString(std::string& s) const  {
            if (type != LIQUID_VARIABLE_TYPE_STRING)
                return false;
            s = this->s;
            return true;
        }
        bool getInteger(long long& i) const  {
            if (type != LIQUID_VARIABLE_TYPE_INT)
                return false;
            i = this->i;
            return true;
        }
        bool getFloat(double& f) const  {
            if (type != LIQUID_VARIABLE_TYPE_FLOAT)
                return false;
            f = this->f;
            return true;
        }

        bool getDictionaryVariable(const CPPVariable** variable, const std::string& key) const {
            if (type != LIQUID_VARIABLE_TYPE_DICTIONARY)
                return false;
            auto it = d.find(key);
            if (it == d.end())
                return false;
            *variable = it->second.get();
            return true;
        }


        CPPVariable* setDictionaryVariable(const std::string& key, CPPVariable* target)  {
            if (type != LIQUID_VARIABLE_TYPE_DICTIONARY) {
                if (type != LIQUID_VARIABLE_TYPE_NIL)
                    return nullptr;
                assign(unordered_map<string, unique_ptr<CPPVariable>>());
            }
            auto it = d.find(key);
            if (it == d.end()) {
                it = d.emplace(key, unique_ptr<CPPVariable>(target)).first;
            } else {
                it->second = unique_ptr<CPPVariable>(target);
            }
            return it->second.get();
        }

        bool getArrayVariable(const CPPVariable** variable, size_t idx) const {
            if (type != LIQUID_VARIABLE_TYPE_ARRAY)
                return false;
            if (idx >= a.size())
                return false;
            *variable = &((*const_cast<CPPVariable*>(this))[idx]);
            return true;
        }

        CPPVariable* setArrayVariable(size_t idx, CPPVariable* target)  {
            if (type != LIQUID_VARIABLE_TYPE_ARRAY)
                return nullptr;
            if (idx >= a.size())
                a.resize(idx+1);
            a[idx] = make_unique<CPPVariable>(*target);
            return a[idx].get();
        }

        long long getArraySize() const {
            if (type != LIQUID_VARIABLE_TYPE_ARRAY)
                return -1;
            return a.size();
        }

        bool iterate(bool (*callback)(void* variable, void* data), void* data, int start = 0, int limit = -1, bool reverse = false) const {
            if (type != LIQUID_VARIABLE_TYPE_ARRAY)
                return false;
            if (limit < 0)
                limit = (int)a.size() + limit;
            if (start < 0)
                start = 0;
            int endIndex = std::min(start+limit-1, (int)a.size());
            if (reverse) {
                for (int i = endIndex; i >= start; --i) {
                    if (!callback(a[i].get(), data))
                        break;
                }
            } else {
                for (int i = start; i <= endIndex; ++i) {
                    if (!callback(a[i].get(), data))
                        break;
                }
            }
            return true;
        }

        bool operator < (const CPPVariable& v) const {
            if (v.type != type)
                return false;
            switch (type) {
                case LIQUID_VARIABLE_TYPE_INT:
                    return i < v.i;
                case LIQUID_VARIABLE_TYPE_FLOAT:
                    return f < v.f;
                case LIQUID_VARIABLE_TYPE_STRING: {
                    if (v.type == LIQUID_VARIABLE_TYPE_STRING)
                        return s < v.s;
                    string str;
                    v.getString(str);
                    return s < str;
                }
                default:
                    return p < v.p;
            }
            return false;
        }
    };


    struct CPPVariableResolver : LiquidVariableResolver {
        CPPVariableResolver() {
            getType = +[](void* variable) { return static_cast<CPPVariable*>(variable)->type; };
            getBool = +[](void* variable, bool* target) { return static_cast<CPPVariable*>(variable)->getBool(*target); };
            getTruthy = +[](void* variable) { return static_cast<CPPVariable*>(variable)->getTruthy(); };
            getString = +[](void* variable, char* target) {
                if (static_cast<CPPVariable*>(variable)->type != LIQUID_VARIABLE_TYPE_STRING)
                    return false;
                strcpy(target, static_cast<CPPVariable*>(variable)->s.data());
                return true;
            };
            getStringLength = +[](void* variable) {
                if (static_cast<CPPVariable*>(variable)->type != LIQUID_VARIABLE_TYPE_STRING)
                    return -1LL;
                return (long long)static_cast<CPPVariable*>(variable)->s.size();
            };
            getInteger = +[](void* variable, long long* target) { return static_cast<CPPVariable*>(variable)->getInteger(*target); };
            getFloat = +[](void* variable, double* target) { return static_cast<CPPVariable*>(variable)->getFloat(*target); };
            getDictionaryVariable = +[](void* variable, const char* key, void** target) { return static_cast<CPPVariable*>(variable)->getDictionaryVariable((const CPPVariable**)target, key); };
            getArrayVariable = +[](void* variable, size_t idx, void** target) { return static_cast<CPPVariable*>(variable)->getArrayVariable((const CPPVariable**)target, idx); };
            setArrayVariable = +[](LiquidRenderer renderer, void* variable, size_t idx, void* target) { return (void*)static_cast<CPPVariable*>(variable)->setArrayVariable(idx, static_cast<CPPVariable*>(target)); };
            setDictionaryVariable = +[](LiquidRenderer renderer, void* variable, const char* key, void* target) { return (void*)static_cast<CPPVariable*>(variable)->setDictionaryVariable(key, static_cast<CPPVariable*>(target)); };
            iterate = +[](void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse) { return static_cast<CPPVariable*>(variable)->iterate(callback, data, start, limit, reverse); };
            getArraySize = +[](void* variable) { return static_cast<CPPVariable*>(variable)->getArraySize(); };

            createHash = +[](LiquidRenderer renderer) { return (void*)new CPPVariable(unordered_map<string, unique_ptr<CPPVariable>>()); };
            createArray = +[](LiquidRenderer renderer) { return (void*)new CPPVariable({ }); };
            createFloat = +[](LiquidRenderer renderer, double value) { return (void*)new CPPVariable(value); };
            createBool = +[](LiquidRenderer renderer, bool value) { return (void*)new CPPVariable(value); };
            createInteger = +[](LiquidRenderer renderer, long long value) { return (void*)new CPPVariable(value); };
            createString = +[](LiquidRenderer renderer,  const char* value) { return (void*)new CPPVariable(string(value)); };
            createPointer = +[](LiquidRenderer renderer, void* value) { return (void*)new CPPVariable(value); };
            createNil = +[](LiquidRenderer renderer) { return (void*)new CPPVariable(); };
            createClone = +[](LiquidRenderer renderer, void* variable) { return (void*)new CPPVariable(*static_cast<CPPVariable*>(variable)); };
            freeVariable = +[](LiquidRenderer renderer, void* variable) { delete (CPPVariable*)variable;  };

            compare = +[](void* a, void* b) { return *static_cast<CPPVariable*>(a) < *static_cast<CPPVariable*>(b) ? -1 : 0; };
        }
    };

}

#endif
