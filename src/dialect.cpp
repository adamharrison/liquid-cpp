#include "context.h"
#include "dialect.h"


namespace Liquid {
    struct IfNode : TagNodeType {
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Type::TAG_FREE, "else") { }
            Parser::Node render(const RenderMode mode, const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        IfNode() : TagNodeType(Type::TAG_ENCLOSED, "if") {
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(const RenderMode mode, const Context& context, const Parser::Node& node, Variable& store) const {
            return Parser::Node();
        }
    };

    struct ForNode : TagNodeType {
        struct ElseNode : NodeType {
            ElseNode() : NodeType(Type::TAG_FREE, "else") { }
            Parser::Node render(const RenderMode mode, const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        ForNode() : TagNodeType(Type::TAG_ENCLOSED, "for") {
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(const RenderMode mode, const Context& context, const Parser::Node& node, Variable& store) const {
            return Parser::Node();
        }
    };

    struct PlusOperatorNode : OperatorNodeType {
        PlusOperatorNode() : OperatorNodeType("+", Arity::BINARY) { }
        Parser::Node render(const RenderMode mode, const Context& context, const Parser::Node& node, Variable& store) const {
            double number = 0.0;
            bool convertToInteger = true;
            for (auto it = node.children.begin(); it != node.children.end(); ++it) {
                Parser::Node result = (*it)->type ? (*it)->type->render(mode, context, *(*it).get(), store) : **it;
                auto& variable = result.variable();
                assert(variable.type == Parser::Variable::Type::STATIC);
                switch (variable.variant.type) {
                    case Parser::Variant::Type::INT:
                        number += variable.variant.i;
                    break;
                    case Parser::Variant::Type::FLOAT:
                        number += variable.variant.f;
                        convertToInteger = false;
                    break;
                    case Parser::Variant::Type::NIL:
                    case Parser::Variant::Type::STRING:
                    case Parser::Variant::Type::POINTER:
                    break;
                }
            }
            if (convertToInteger)
                return Parser::Node(Parser::Variant((long long)number));
            return Parser::Node(Parser::Variant(number));

        }
    };

    void StandardDialect::implement(Context& context) {
        context.registerType(make_unique<IfNode>());
        context.registerType(make_unique<ForNode>());
        context.registerType(make_unique<PlusOperatorNode>());
    }
}
