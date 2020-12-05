#include "renderer.h"
#include "context.h"

namespace Liquid {
    struct Context;


    LiquidRenderErrorType Renderer::render(const Node& ast, Variable store, void (*callback)(const char* chunk, size_t size, void* data), void* data) {
        renderStartTime = std::chrono::system_clock::now();
        currentMemoryUsage = 0;
        currentRenderingDepth = 0;
        error = Error::Type::LIQUID_RENDERER_ERROR_TYPE_NONE;
        Node node = ast.type->render(*this, ast, store);
        assert(node.type == nullptr);
        auto s = node.getString();
        callback(s.data(), s.size(), data);
        return error;
    }

    string Renderer::render(const Node& ast, Variable store) {
        string accumulator;
        LiquidRenderErrorType error = render(ast, store, +[](const char* chunk, size_t size, void* data){
            string* accumulator = (string*)data;
            accumulator->append(chunk, size);
        }, &accumulator);
        if (error != LIQUID_RENDERER_ERROR_TYPE_NONE)
            throw Error(error);
        return accumulator;
    }

    Node Renderer::retrieveRenderedNode(const Node& node, Variable store) {
        if (node.type)
            return node.type->render(*this, node, store);
        return node;
    }



    void Renderer::inject(Variable& variable, const Variant& variant) {
        const LiquidVariableResolver& resolver = context.getVariableResolver();
        switch (variant.type) {
            case Variant::Type::STRING:
                variable = resolver.createString(*this, variant.s.data());
            break;
            case Variant::Type::INT:
                variable = resolver.createInteger(*this, variant.i);
            break;
            case Variant::Type::FLOAT:
                variable = resolver.createFloat(*this, variant.f);
            break;
            case Variant::Type::NIL:
                variable = resolver.createNil(*this);
            break;
            case Variant::Type::BOOL:
                variable = resolver.createBool(*this, variant.b);
            break;
            case Variant::Type::VARIABLE:
                variable = resolver.createClone(*this, variant.v.pointer);
            break;
            case Variant::Type::ARRAY: {
                variable = resolver.createArray(*this);
                for (size_t i = 0; i < variant.a.size(); ++i) {
                    Variable target;
                    inject(target, variant.a[i]);
                    if (!resolver.setArrayVariable(*this, variable, i, target))
                        break;
                }
            } break;
            default:
                variable = resolver.createPointer(*this, variant.p);
            break;
        }
    }

    Variant Renderer::parseVariant(Variable variable) {
        if (!variable.exists())
            return Variant();
        const LiquidVariableResolver& resolver = context.getVariableResolver();
        ELiquidVariableType type = resolver.getType(variable);
        switch (type) {
            case LIQUID_VARIABLE_TYPE_OTHER:
            case LIQUID_VARIABLE_TYPE_DICTIONARY:
            case LIQUID_VARIABLE_TYPE_ARRAY:
                return Variant(Variable({variable}));
            break;
            case LIQUID_VARIABLE_TYPE_BOOL: {
                bool b;
                if (resolver.getBool(variable, &b))
                    return Variant(b);
            } break;
            case LIQUID_VARIABLE_TYPE_INT: {
                long long i;
                if (resolver.getInteger(variable, &i))
                    return Variant(i);
            } break;
            case LIQUID_VARIABLE_TYPE_FLOAT: {
                double f;
                if (resolver.getFloat(variable, &f))
                    return Variant(f);
            } break;
            case LIQUID_VARIABLE_TYPE_STRING: {
                string s;
                long long size = resolver.getStringLength(variable);
                if (size >= 0) {
                    s.resize(size);
                    if (resolver.getString(variable, const_cast<char*>(s.data())))
                        return Variant(move(s));
                }
            } break;
            case LIQUID_VARIABLE_TYPE_NIL:
                return Variant();
        }
        return Variant();
    }


    std::string Renderer::getString(const Node& node) {
        const LiquidVariableResolver& resolver = context.getVariableResolver();

        if (!node.type) {
            if (node.variant.type == Variant::Type::VARIABLE) {
                string s;
                long long size = resolver.getStringLength(node.variant.v.pointer);
                if (size >= 0) {
                    s.resize(size);
                    if (resolver.getString(node.variant.v.pointer, const_cast<char*>(s.data())))
                        return move(s);
                }
            } else {
                return node.variant.getString();
            }
        }
        return string();
    }
}
