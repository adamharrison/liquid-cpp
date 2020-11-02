#include "context.h"
#include "dialect.h"


namespace Liquid {
    struct AssignNode : TagNodeType {
        AssignNode() : TagNodeType(Composition::FREE, "assign", 1, 1) { }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            auto& argumentNode = node.children.front();
            auto& assignmentNode = argumentNode->children.front();
            auto& variableNode = assignmentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(context, *variableNode.get(), store, true);
                if (targetVariable) {
                    auto& operandNode = assignmentNode->children.back();
                    Parser::Node node = context.retrieveRenderedNode(*operandNode.get(), store);
                    assert(!node.type);
                    switch (node.variant.type) {
                        case Liquid::Parser::Variant::Type::FLOAT:
                            targetVariable->assign(node.variant.f);
                        break;
                        case Liquid::Parser::Variant::Type::INT:
                            targetVariable->assign(node.variant.i);
                        break;
                        case Liquid::Parser::Variant::Type::STRING:
                            targetVariable->assign(node.variant.s);
                        break;
                        case Liquid::Parser::Variant::Type::VARIABLE:
                            targetVariable->assign(*(Variable*)node.variant.p);
                        break;
                        default:
                            targetVariable->clear();
                        break;
                    }
                }
            }
            return Parser::Node();
        }
    };
    struct AssignOperator : OperatorNodeType {
        AssignOperator() : OperatorNodeType("=", Arity::BINARY, 0) { }
        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
    };



    struct IfNode : TagNodeType {
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        IfNode() : TagNodeType(Composition::ENCLOSED, "if", 1, 1) {
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            auto result = context.retrieveRenderedNode(*arguments->children.front().get(), store);
            bool isTrue = true;
            if (isTrue)
                return context.retrieveRenderedNode(*node.children[1].get(), store);
            else {
                if (node.children.size() >= 2)
                    return context.retrieveRenderedNode(*node.children[2].get(), store);
                return Parser::Node();
            }
        }
    };

    struct ForNode : TagNodeType {
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
                return Parser::Node();
            }
        };

        ForNode() : TagNodeType(Composition::ENCLOSED, "for") {
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
        context.registerType(make_unique<AssignNode>());
        context.registerType(make_unique<AssignOperator>());
        context.registerType(make_unique<PlusOperatorNode>());
        context.registerType(make_unique<MinusOperatorNode>());
        context.registerType(make_unique<MultiplyOperatorNode>());
        context.registerType(make_unique<DivideOperatorNode>());
    }
}
