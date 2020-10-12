#ifndef CONTEXT_H
#define CONTEXT_H

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


    struct NodeType {
        string name;
        bool free;
    };

    struct OperatorType {
        string name;
    };

    struct Context {
        unordered_map<string, NodeType> nodeTypes;
        unordered_map<string, OperatorType> operatorTypes;
        const NodeType* getConcatenationNodeType() const {
            static NodeType concatenationNodeType({"concat", false});
            return &concatenationNodeType;
        }
        const NodeType* getStaticNodeType() const {
            static NodeType staticNodeType({"static", false});
            return &staticNodeType;
        }

        struct DefaultVariable {
            union {
                string s;
                double f;
                long long i;
                vector<unique_ptr<DefaultVariable>> a;
                unordered_map<string, unique_ptr<DefaultVariable>> d;
            };

            enum class Type {
                NIL,
                FLOAT,
                INT,
                STRING,
                ARRAY,
                DICTIONARY
            };
            Type type;

            DefaultVariable(long long i) : i(i), type(Type::INT) { }
            DefaultVariable(int i) : DefaultVariable((long long)i) { }
            DefaultVariable(double f) : f(f), type(Type::FLOAT) { }
            DefaultVariable(const string& s) : s(s), type(Type::STRING) { }
            DefaultVariable(void* nil) : type(Type::NIL) { }
            ~DefaultVariable() { if (type == Type::STRING) s.~string(); }
        };

        using Variable = DefaultVariable;


        void registerType(string name, bool free);
        void registerOperator(string name);
        void registerOutputType();

        template <class T>
        void registerType() {
            registerType(T::name, T::free);
        }

        template <class T>
        void registerOperator() {
            registerOperator(T::name);
        }

        const NodeType* getType(string name) const {
            auto it = nodeTypes.find(name);
            if (it == nodeTypes.end())
                return nullptr;
            return &it->second;
        }
    };

    struct DefaultContext : Context {
        DefaultContext();
    };
}

#endif
