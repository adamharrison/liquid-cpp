#include "renderer.h"
#include "context.h"

namespace Liquid {
    struct Context;

    Renderer::Renderer(const Context& context) : context(context) {
        memset(&variableResolver, 0, sizeof(LiquidVariableResolver));
    }

    Renderer::Renderer(const Context& context, LiquidVariableResolver variableResolver) : context(context), variableResolver(variableResolver) {

    }

    Variant Renderer::renderArgument(const Node& ast, Variable store) {
        nodeContext = nullptr;
        errors.clear();
        unknownErrors.clear();
        renderStartTime = std::chrono::system_clock::now();
        currentMemoryUsage = 0;
        currentRenderingDepth = 0;
        error = Error::Type::LIQUID_RENDERER_ERROR_TYPE_NONE;
        internalRender = true;
        Node node = retrieveRenderedNode(ast, store);
        internalRender = false;
        assert(node.type == nullptr);
        if (error != LIQUID_RENDERER_ERROR_TYPE_NONE)
            throw Error(error, Node());
        return node.variant;
    }

    LiquidRendererErrorType Renderer::render(const Node& ast, Variable store, void (*callback)(const char* chunk, size_t size, void* data), void* data) {
        if (internalRender) {
            Node node = retrieveRenderedNode(ast, store);
            assert(node.type == nullptr);
            auto s = node.getString();
            callback(s.data(), s.size(), data);
        } else {
            nodeContext = nullptr;
            errors.clear();
            unknownErrors.clear();
            renderStartTime = std::chrono::system_clock::now();
            currentMemoryUsage = 0;
            currentRenderingDepth = 0;
            error = Error::Type::LIQUID_RENDERER_ERROR_TYPE_NONE;
            internalRender = true;
            Node node = retrieveRenderedNode(ast, store);
            internalRender = false;
            assert(node.type == nullptr);
            auto s = node.getString();
            callback(s.data(), s.size(), data);
        }
        return error;
    }

    string Renderer::render(const Node& ast, Variable store) {
        string accumulator;
        LiquidRendererErrorType error = render(ast, store, +[](const char* chunk, size_t size, void* data){
            string* accumulator = (string*)data;
            accumulator->append(chunk, size);
        }, &accumulator);
        if (error != LIQUID_RENDERER_ERROR_TYPE_NONE)
            throw Error(error, Node());
        return accumulator;
    }

    pair<void*, Renderer::DropFunction> Renderer::getInternalDrop(const string& key) {
        auto it = internalDrops.find(key);
        if (it == internalDrops.end())
            return { nullptr, nullptr };
        return it->second.back();
    }

    pair<void*, Renderer::DropFunction> Renderer::getInternalDrop(const Node& node, Variable store) {
        assert(node.type && node.children.size() > 0);
        string key = retrieveRenderedNode(*node.children[0].get(), store).getString();
        return getInternalDrop(key);
    }

    void Renderer::pushInternalDrop(const std::string& key, std::pair<void*, DropFunction> func) {
        internalDrops[key].push_back(func);
    }

    void Renderer::popInternalDrop(const std::string& key) {
        auto it = internalDrops.find(key);
        if (it != internalDrops.end()) {
            it->second.pop_back();
            if (it->second.size() == 0)
                internalDrops.erase(it);
        }
    }


    void Renderer::inject(Variable& variable, const Variant& variant) {
        switch (variant.type) {
            case Variant::Type::STRING:
                variable = variableResolver.createString(*this, variant.s.data());
            break;
            case Variant::Type::INT:
                variable = variableResolver.createInteger(*this, variant.i);
            break;
            case Variant::Type::FLOAT:
                variable = variableResolver.createFloat(*this, variant.f);
            break;
            case Variant::Type::NIL:
                variable = variableResolver.createNil(*this);
            break;
            case Variant::Type::BOOL:
                variable = variableResolver.createBool(*this, variant.b);
            break;
            case Variant::Type::VARIABLE:
                variable = variableResolver.createClone(*this, variant.v.pointer);
            break;
            case Variant::Type::ARRAY: {
                variable = variableResolver.createArray(*this);
                for (size_t i = 0; i < variant.a.size(); ++i) {
                    Variable target;
                    inject(target, variant.a[i]);
                    if (!variableResolver.setArrayVariable(*this, variable, i, target))
                        break;
                    variableResolver.freeVariable(*this, target);
                }
            } break;
            default:
                variable = variableResolver.createPointer(*this, variant.p);
            break;
        }
    }

    Variant Renderer::parseVariant(Variable variable) {
        if (!variable.exists())
            return Variant();
        ELiquidVariableType type = variableResolver.getType(*this, variable);
        switch (type) {
            case LIQUID_VARIABLE_TYPE_OTHER:
            case LIQUID_VARIABLE_TYPE_DICTIONARY:
            case LIQUID_VARIABLE_TYPE_ARRAY:
                return Variant(Variable({variable}));
            break;
            case LIQUID_VARIABLE_TYPE_BOOL: {
                bool b;
                if (variableResolver.getBool(LiquidRenderer { this }, variable, &b))
                    return Variant(b);
            } break;
            case LIQUID_VARIABLE_TYPE_INT: {
                long long i;
                if (variableResolver.getInteger(*this, variable, &i))
                    return Variant(i);
            } break;
            case LIQUID_VARIABLE_TYPE_FLOAT: {
                double f;
                if (variableResolver.getFloat(*this, variable, &f))
                    return Variant(f);
            } break;
            case LIQUID_VARIABLE_TYPE_STRING: {
                string s;
                long long size = variableResolver.getStringLength(*this, variable);
                if (size >= 0) {
                    s.resize(size);
                    if (variableResolver.getString(*this, variable, const_cast<char*>(s.data())))
                        return Variant(move(s));
                }
            } break;
            case LIQUID_VARIABLE_TYPE_NIL:
                return Variant();
        }
        return Variant();
    }


    std::string Renderer::getString(const Node& node) {
        if (!node.type) {
            if (node.variant.type == Variant::Type::VARIABLE) {
                string s;
                long long size = variableResolver.getStringLength(*this, node.variant.v.pointer);
                if (size >= 0) {
                    s.resize(size);
                    if (variableResolver.getString(*this, node.variant.v.pointer, const_cast<char*>(s.data())))
                        return s;
                }
            } else {
                return node.variant.getString();
            }
        }
        return string();
    }

    void Renderer::pushUnknownFilterWarning(const Node& node, Variable stash) {
        assert(node.type);
        if (unknownErrors.find(&node) == unknownErrors.end()) {
            unknownErrors.insert(&node);
            errors.push_back(Error(LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_FILTER, node, node.type->symbol));
        }
    }
    void Renderer::pushUnknownVariableWarning(const Node& node, int offset, Variable store) {
        if (unknownErrors.find(&node) == unknownErrors.end()) {
            unknownErrors.insert(&node);
            string variableName;
            for (size_t i = offset; i < node.children.size(); ++i) {
                auto result = retrieveRenderedNode(*node.children[i].get(), store);
                if (!variableName.empty()) {
                    if (result.variant.type == Variant::Type::INT) {
                        variableName.push_back('[');
                        variableName.append(std::to_string(result.variant.i));
                        variableName.push_back(']');
                    } else
                        variableName.append(".");
                    variableName.append(result.getString());
                }
            }
            errors.push_back(Error(LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_VARIABLE, node, variableName));
        }
    }

    Variable Renderer::getVariable(const Node& node, Variable store, size_t offset) {
        Variable storePointer = store;
        for (size_t i = offset; i < node.children.size(); ++i) {
            auto& link = node.children[i];
            auto node = retrieveRenderedNode(*link.get(), store);
            switch (node.variant.type) {
                case Variant::Type::INT:
                    if (!variableResolver.getArrayVariable(*this, storePointer, node.variant.i, storePointer))
                        storePointer = Variable({ nullptr });
                break;
                case Variant::Type::STRING: {
                    if (!variableResolver.getDictionaryVariable(*this, storePointer, node.variant.s.data(), storePointer))
                        storePointer = Variable({ nullptr });
                } break;
                default:
                    storePointer = Variable({ nullptr });
                break;
            }
            if (!storePointer.pointer)
                break;
        }
        if (logUnknownVariables && !storePointer.exists())
            pushUnknownVariableWarning(node, offset, store);
        return storePointer;
    }

    bool Renderer::setVariable(const Node& node, Variable store, Variable value, size_t offset) {
        Variable storePointer = store;
        for (size_t i = offset; i < node.children.size(); ++i) {
            auto& link = node.children[i];
            auto part = retrieveRenderedNode(*link.get(), store);
            if (i == node.children.size() - 1) {
                switch (part.variant.type) {
                    case Variant::Type::INT:
                        return variableResolver.setArrayVariable(*this, storePointer, part.variant.i, value);
                    case Variant::Type::STRING:
                        return variableResolver.setDictionaryVariable(*this, storePointer, part.variant.s.data(), value);
                    default:
                        return false;
                }
            } else {
                switch (part.variant.type) {
                    case Variant::Type::INT:
                        if (!variableResolver.getArrayVariable(*this, storePointer, part.variant.i, storePointer))
                            return false;
                    break;
                    case Variant::Type::STRING:
                        if (!variableResolver.getDictionaryVariable(*this, storePointer, part.variant.s.data(), storePointer))
                            return false;
                    break;
                    default:
                        return false;
                }
            }
            if (!storePointer.pointer)
                return false;
        }
        return false;
    }
}
