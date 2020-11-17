#ifndef COMMON_H
#define COMMON_H

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

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


    struct Variable {
        enum class Type {
            NIL,
            FLOAT,
            INT,
            STRING,
            ARRAY,
            BOOL,
            DICTIONARY,
            OTHER
        };
        virtual ~Variable() { }
        virtual Type getType() const { return Type::NIL; }
        virtual bool getBool(bool& b) const { return false; }
        virtual bool getTruthy() const { return false; }
        virtual bool getString(std::string& str) const { return false; }
        virtual bool getInteger(long long& i) const { return false; }
        virtual bool getFloat(double& i) const { return false; }
        virtual bool getDictionaryVariable(Variable*& variable, const std::string& key) const { return false;  }
        virtual bool getDictionaryVariable(Variable*& variable, const std::string& key, bool createOnNotExists) { return false;  }
        virtual bool getArrayVariable(Variable*& variable, size_t idx) const { return false; }
        virtual bool getArrayVariable(Variable*& variable, size_t idx, bool createOnNotExists) { return false; }
        virtual bool iterate(bool(*)(Variable* variable, void* data),  void* data, int start = 0, int limit = -1) const { return false; }
        virtual long long getArraySize() const { return -1; }
        virtual void assign(const Variable& v) { }
        virtual void assign(double f) { }
        virtual void assign(bool b) { }
        virtual void assign(long long i) { }
        virtual void assign(const std::string& s) { }
        virtual void clear() { }
        virtual int compare(Variable& v) const { return 0; }
    };

}

#endif
