#include "context.h"

namespace Liquid {

    Node FilterNodeType::getOperand(Renderer& renderer, const Node& node, Variable store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }

    Node FilterNodeType::getArgument(Renderer& renderer, const Node& node, Variable store, int idx) const {
        if (idx >= (int)node.children[1]->children.size())
            return Node();
        return renderer.retrieveRenderedNode(*node.children[1]->children[idx].get(), store);
    }

    Node Context::ConcatenationNode::render(Renderer& renderer, const Node& node, Variable store) const {
        string s;
        for (auto& child : node.children) {
            s.append(renderer.retrieveRenderedNode(*child.get(), store).getString());
            if (renderer.control != Renderer::Control::NONE)
                return Node(s);
        }
        return Node(s);
    }

    void Context::inject(Variable& variable, const Variant& variant) const {
        const LiquidVariableResolver& resolver = getVariableResolver();
        switch (variant.type) {
            case Variant::Type::STRING:
                variable = resolver.createString(variant.s.data());
            break;
            case Variant::Type::INT:
                variable = resolver.createInteger(variant.i);
            break;
            case Variant::Type::FLOAT:
                variable = resolver.createFloat(variant.f);
            break;
            case Variant::Type::NIL:
                variable = resolver.createNil();
            break;
            case Variant::Type::BOOL:
                variable = resolver.createBool(variant.b);
            break;
            case Variant::Type::VARIABLE:
                variable = variant.v;
            break;
            case Variant::Type::ARRAY: {
                variable = resolver.createArray();
                for (size_t i = 0; i < variant.a.size(); ++i) {
                    Variable target;
                    inject(target, variant.a[i]);
                    if (!resolver.setArrayVariable(variable, i, target))
                        break;
                }
            } break;
            default:
                variable = resolver.createPointer(variant.p);
            break;
        }
    }

    Variant Context::parseVariant(Variable variable) const {
        if (!variable.exists())
            return Variant();
        const LiquidVariableResolver& resolver = getVariableResolver();
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
                s.resize(size);
                if (resolver.getString(variable, const_cast<char*>(s.data())))
                    return Variant(std::move(s));
            } break;
            case LIQUID_VARIABLE_TYPE_NIL:
                return Variant();
        }
        return Variant();
    }
};
