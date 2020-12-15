
#ifndef RAPIDJSONVARAIBLE_H
#define RAPIDJSONVARIABLE_H

#include "common.h"
#include "context.h"

#include <rapidjson/document.h>

namespace Liquid {

    struct RapidJSONVariableResolver : LiquidVariableResolver {
        RapidJSONVariableResolver() {
            getType = +[](void* variable) {
                switch (static_cast<rapidjson::Value*>(variable)->GetType()) {
                    case 0:
                        return LIQUID_VARIABLE_TYPE_NIL;
                    case 1:
                    case 2:
                        return LIQUID_VARIABLE_TYPE_BOOL;
                    case 3:
                        return LIQUID_VARIABLE_TYPE_DICTIONARY;
                    case 4:
                        return LIQUID_VARIABLE_TYPE_ARRAY;
                    case 5:
                        return LIQUID_VARIABLE_TYPE_STRING;
                    case 6:
                        return static_cast<rapidjson::Value*>(variable)->IsDouble() ? LIQUID_VARIABLE_TYPE_FLOAT : LIQUID_VARIABLE_TYPE_INT;
                    default:
                        return LIQUID_VARIABLE_TYPE_OTHER;
                }
            };
            getBool = +[](void* variable, bool* target) {
                int type = static_cast<rapidjson::Value*>(variable)->GetType();
                if (type != 2 && type != 3)
                    return false;
                *target = type == 2;
                return true;
            };
            getTruthy = +[](void* variable) {
                switch (static_cast<rapidjson::Value*>(variable)->GetType()) {
                    case 0:
                    case 1:
                        return false;
                    case 5:
                        return static_cast<rapidjson::Value*>(variable)->GetStringLength() > 0;
                    case 6:
                        return static_cast<rapidjson::Value*>(variable)->GetInt64() != 0;
                    default:
                        return true;
                }
            };
            getString = +[](void* variable, char* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsString())
                    return false;
                strcpy(target, static_cast<rapidjson::Value*>(variable)->GetString());
                return true;
            };
            getStringLength = +[](void* variable) {
                if (!static_cast<rapidjson::Value*>(variable)->IsString())
                    return -1LL;
                return (long long)static_cast<rapidjson::Value*>(variable)->GetStringLength();
            };
            getInteger = +[](void* variable, long long* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsNumber())
                    return false;
                *target = static_cast<rapidjson::Value*>(variable)->GetInt64();
                return true;
            };
            getFloat = +[](void* variable, double* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsNumber())
                    return false;
                *target = static_cast<rapidjson::Value*>(variable)->GetFloat();
                return false;
            };
            getDictionaryVariable = +[](void* variable, const char* key, void** target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsObject() || !static_cast<rapidjson::Value*>(variable)->HasMember(key))
                    return false;
                auto& value = (*static_cast<rapidjson::Value*>(variable))[key];
                *target = &value;
                return true;
            };
            getArrayVariable = +[](void* variable, size_t idx, void** target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsArray() || idx < static_cast<rapidjson::Value*>(variable)->Size())
                    return false;
                auto& value = (*static_cast<rapidjson::Value*>(variable))[idx];
                *target = &value;
                return true;
            };

            getArraySize = +[](void* variable) {
                if (!static_cast<rapidjson::Value*>(variable)->IsArray())
                    return -1LL;
                return (long long)static_cast<rapidjson::Value*>(variable)->Size();
            };

            iterate = +[](void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse) {
                if (static_cast<rapidjson::Value*>(variable)->IsArray()) {

                } else if (static_cast<rapidjson::Value*>(variable)->IsObject()) {

                }
                return false;
            };

            setArrayVariable = +[](LiquidRenderer renderer, void* variable, size_t idx, void* target) { return (void*)static_cast<CPPVariable*>(variable)->setArrayVariable(idx, static_cast<CPPVariable*>(target)); };
            setDictionaryVariable = +[](LiquidRenderer renderer, void* variable, const char* key, void* target) { return (void*)static_cast<CPPVariable*>(variable)->setDictionaryVariable(key, static_cast<CPPVariable*>(target)); };

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

