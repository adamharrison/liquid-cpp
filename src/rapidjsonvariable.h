
#ifndef LIQUIDRAPIDJSONVARAIBLE_H
#define LIQUIDRAPIDJSONVARIABLE_H

#include "common.h"
#include "context.h"

#include <rapidjson/document.h>

namespace Liquid {

    struct RapidJSONVariableResolver : LiquidVariableResolver {
        RapidJSONVariableResolver() {
            getType = +[](LiquidRenderer renderer, void* variable) {
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
            getBool = +[](LiquidRenderer renderer, void* variable, bool* target) {
                int type = static_cast<rapidjson::Value*>(variable)->GetType();
                if (type != 2 && type != 3)
                    return false;
                *target = type == 2;
                return true;
            };
            getTruthy = +[](LiquidRenderer renderer, void* variable) {
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
            getString = +[](LiquidRenderer renderer, void* variable, char* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsString())
                    return false;
                strcpy(target, static_cast<rapidjson::Value*>(variable)->GetString());
                return true;
            };
            getStringLength = +[](LiquidRenderer renderer, void* variable) {
                if (!static_cast<rapidjson::Value*>(variable)->IsString())
                    return -1LL;
                return (long long)static_cast<rapidjson::Value*>(variable)->GetStringLength();
            };
            getInteger = +[](LiquidRenderer renderer, void* variable, long long* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsNumber())
                    return false;
                *target = static_cast<rapidjson::Value*>(variable)->GetInt64();
                return true;
            };
            getFloat = +[](LiquidRenderer renderer, void* variable, double* target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsNumber())
                    return false;
                *target = static_cast<rapidjson::Value*>(variable)->GetFloat();
                return false;
            };
            getDictionaryVariable = +[](LiquidRenderer renderer, void* variable, const char* key, void** target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsObject() || !static_cast<rapidjson::Value*>(variable)->HasMember(key))
                    return false;
                auto& value = (*static_cast<rapidjson::Value*>(variable))[key];
                *target = &value;
                return true;
            };
            getArrayVariable = +[](LiquidRenderer renderer, void* variable, long long idx, void** target) {
                if (!static_cast<rapidjson::Value*>(variable)->IsArray())
                    return false;
                if (idx < 0)
                    idx += static_cast<rapidjson::Value*>(variable)->Size();
                if (idx < 0 || idx >= static_cast<rapidjson::Value*>(variable)->Size())
                    return false;
                auto& value = (*static_cast<rapidjson::Value*>(variable))[idx];
                *target = &value;
                return true;
            };

            getArraySize = +[](LiquidRenderer renderer, void* variable) {
                if (!static_cast<rapidjson::Value*>(variable)->IsArray())
                    return -1LL;
                return (long long)static_cast<rapidjson::Value*>(variable)->Size();
            };

            iterate = +[](LiquidRenderer renderer, void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse) {
                if (static_cast<rapidjson::Value*>(variable)->IsArray()) {

                } else if (static_cast<rapidjson::Value*>(variable)->IsObject()) {

                }
                return false;
            };

            setArrayVariable = +[](LiquidRenderer renderer, void* variable, long long idx, void* target) { 
                assert(static_cast<Renderer*>(renderer.renderer)->resolverCustomData);
                rapidjson::Value& value = *static_cast<rapidjson::Value*>(target);
                if (!value.IsArray())
                    return (void*)NULL;
                if (idx >= value.Size())
                    value.Reserve(idx, ((rapidjson::Document*)static_cast<Renderer*>(renderer.renderer)->resolverCustomData)->GetAllocator());
                value[idx] = *static_cast<rapidjson::Value*>(target);
                return (void*)&value[idx];
            };
            
            setDictionaryVariable = +[](LiquidRenderer renderer, void* variable, const char* key, void* target) { 
                assert(static_cast<Renderer*>(renderer.renderer)->resolverCustomData);
                rapidjson::Value& value = *static_cast<rapidjson::Value*>(variable);
                if (!value.IsObject())
                    return (void*)NULL;
                value.AddMember(rapidjson::Value(key, ((rapidjson::Document*)static_cast<Renderer*>(renderer.renderer)->resolverCustomData)->GetAllocator()), *static_cast<rapidjson::Value*>(target), ((rapidjson::Document*)static_cast<Renderer*>(renderer.renderer)->resolverCustomData)->GetAllocator());
                return (void*)&value[key];
            };

            createHash = +[](LiquidRenderer renderer) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                return (void*)new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value(rapidjson::kObjectType); 
            };
            createArray = +[](LiquidRenderer renderer) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                return (void*)new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value(rapidjson::kArrayType); 
            };
            createFloat = +[](LiquidRenderer renderer, double value) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->SetDouble(value);
                return (void*)jvalue;
            };
            createBool = +[](LiquidRenderer renderer, bool value) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->SetBool(value);
                return (void*)jvalue;
            };
            createInteger = +[](LiquidRenderer renderer, long long value) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->SetInt64(value);
                return (void*)jvalue;
            };
            createString = +[](LiquidRenderer renderer,  const char* value) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value(); 
                jvalue->SetString(value, ((rapidjson::Document*)static_cast<Renderer*>(renderer.renderer)->resolverCustomData)->GetAllocator()); 
                return (void*)jvalue; 
            };
            createPointer = +[](LiquidRenderer renderer, void* value) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->SetInt64((long long)value); 
                return (void*)jvalue; 
            };
            createNil = +[](LiquidRenderer renderer) {
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->SetNull();
                return (void*)jvalue; 
            };
            createClone = +[](LiquidRenderer renderer, void* variable) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                assert(static_cast<Renderer*>(renderer.renderer)->resolverCustomData);
                rapidjson::Value* jvalue = new (document->GetAllocator().Malloc(sizeof(rapidjson::Value))) rapidjson::Value();
                jvalue->CopyFrom(*(rapidjson::Value*)variable, ((rapidjson::Document*)static_cast<Renderer*>(renderer.renderer)->resolverCustomData)->GetAllocator()); 
                return (void*)jvalue; 
            };
            freeVariable = +[](LiquidRenderer renderer, void* variable) { 
                rapidjson::Document* document = static_cast<rapidjson::Document*>(static_cast<Renderer*>(renderer.renderer)->resolverCustomData); 
                assert(document);
                rapidjson::Value* val = ((rapidjson::Value*)variable);
                val->~GenericValue();
                document->GetAllocator().Free(variable);
            };

            compare = +[](void* a, void* b) { return static_cast<rapidjson::Value*>(a)->GetInt64() < static_cast<rapidjson::Value*>(b)->GetInt64() ? -1 : 0; };
        }
    };

}

#endif

