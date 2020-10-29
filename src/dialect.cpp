#include "context.h"
#include "dialect.h"


namespace Liquid {
    struct IfNode : TagNodeType {
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Type::TAG_FREE, "else") { }
            Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        IfNode() : TagNodeType(Type::TAG_ENCLOSED, "if") {
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            return Parser::Node();
        }
    };

    struct ForNode : TagNodeType {
        struct ElseNode : NodeType {
            ElseNode() : NodeType(Type::TAG_FREE, "else") { }
            Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        ForNode() : TagNodeType(Type::TAG_ENCLOSED, "for") {
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            return Parser::Node();
        }
    };


    template <class DerivedNode>
    struct ArithmeticOperatorNode : OperatorNodeType {
        ArithmeticOperatorNode(const char* symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }
        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = node.children[0]->type ? node.children[0]->type->render(context, *node.children[0].get(), store) : *node.children[0];
            Parser::Node op2 = node.children[1]->type ? node.children[1]->type->render(context, *node.children[1].get(), store) : *node.children[1];
            if (op1.type || op2.type)
                return Parser::Node();
            switch (op1.variant.type) {
                case Parser::Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(static_cast<const DerivedNode*>(this)->operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(static_cast<const DerivedNode*>(this)->operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Parser::Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(static_cast<const DerivedNode*>(this)->operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(static_cast<const DerivedNode*>(this)->operate(op1.variant.f, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
            return Parser::Node();
        }
    };

    struct PlusOperatorNode : ArithmeticOperatorNode<PlusOperatorNode> {
        PlusOperatorNode() : ArithmeticOperatorNode<PlusOperatorNode>("+", 5) { }

        double operate(double v1, long long v2) const { return v1 + v2; }
        double operate(double v1, double v2) const { return v1 + v2; }
        double operate(long long v1, double v2) const { return v1 + v2; }
        long long operate(long long v1, long long v2) const { return v1 + v2; }
    };
    struct MinusOperatorNode : ArithmeticOperatorNode<MinusOperatorNode> {
        MinusOperatorNode() : ArithmeticOperatorNode<MinusOperatorNode>("-", 5) { }

        double operate(double v1, long long v2) const { return v1 - v2; }
        double operate(double v1, double v2) const { return v1 - v2; }
        double operate(long long v1, double v2) const { return v1 - v2; }
        long long operate(long long v1, long long v2) const { return v1 - v2; }
    };

    struct MultiplyOperatorNode : ArithmeticOperatorNode<MultiplyOperatorNode> {
        MultiplyOperatorNode() : ArithmeticOperatorNode<MultiplyOperatorNode>("*", 10) { }

        double operate(double v1, long long v2) const { return v1 * v2; }
        double operate(double v1, double v2) const { return v1 * v2; }
        double operate(long long v1, double v2) const { return v1 * v2; }
        long long operate(long long v1, long long v2) const { return v1 * v2; }
    };

    struct DivideOperatorNode : ArithmeticOperatorNode<DivideOperatorNode> {
        DivideOperatorNode() : ArithmeticOperatorNode<DivideOperatorNode>("/", 10) { }

        double operate(double v1, long long v2) const { return v1 / v2; }
        double operate(double v1, double v2) const { return v1 / v2; }
        double operate(long long v1, double v2) const { return v1 / v2; }
        long long operate(long long v1, long long v2) const { return v1 / v2; }
    };


    void StandardDialect::implement(Context& context) {
        context.registerType(make_unique<IfNode>());
        context.registerType(make_unique<ForNode>());
        context.registerType(make_unique<PlusOperatorNode>());
        context.registerType(make_unique<MinusOperatorNode>());
        context.registerType(make_unique<MultiplyOperatorNode>());
        context.registerType(make_unique<DivideOperatorNode>());
    }
}
