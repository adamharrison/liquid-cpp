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
            assert(result.type == nullptr);

            if (result.variant.isTruthy())
                return context.retrieveRenderedNode(*node.children[1].get(), store);
            else {
                if (node.children.size() > 2)
                    return context.retrieveRenderedNode(*node.children[2].get(), store);
                return Parser::Node();
            }
        }
    };

    struct ForNode : TagNodeType {
        struct InOperatorNode : OperatorNodeType {
            InOperatorNode() :  OperatorNodeType("in", Arity::BINARY, 0) { }
            Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
        };

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
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            assert(arguments->children.size() >= 1);
            // Should be move to validation.
            assert(arguments->children[0]->type && arguments->children[0]->type->type == NodeType::Type::OPERATOR && arguments->children[0]->type->symbol == "in");
            assert(arguments->children[0]->children[0]->type && arguments->children[0]->children[0]->type->type == NodeType::Type::VARIABLE);
            // Should be deleted eventually, as they're handled by the parser.
            assert(arguments->children[0]->children.size() == 2);

            auto& variableNode = arguments->children[0]->children[0];
            Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(context, *variableNode, store, true);
            assert(targetVariable);

            context.retrieveRenderedNode(*arguments->children[0]->children[0].get(), store);
            Parser::Node result = context.retrieveRenderedNode(*arguments->children[0]->children[1].get(), store);

            if (result.type != nullptr || result.variant.type != Parser::Variant::Type::VARIABLE)
                return Parser::Node();

            struct ForLoopContext {
                const Context& context;
                const Parser::Node& node;
                Variable& store;
                Variable& targetVariable;
                string result;
                int idx = 0;
            };

            ForLoopContext forLoopContext = { context, node, store, *targetVariable };
            static_cast<Variable*>(result.variant.p)->iterate(+[](Variable* variable, void* data) {
                ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                forLoopContext.targetVariable.assign(*variable);
                forLoopContext.result.append(forLoopContext.context.retrieveRenderedNode(*forLoopContext.node.children[1].get(), forLoopContext.store).getString());
                ++forLoopContext.idx;
            }, const_cast<void*>((void*)&forLoopContext));
            return Parser::Node(forLoopContext.result);
        }
    };

    template <class Function>
    struct ArithmeticOperatorNode : OperatorNodeType {
        ArithmeticOperatorNode(const char* symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }



        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = context.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = context.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Parser::Node();
            switch (op1.variant.type) {
                case Parser::Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Parser::Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.f)));
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

    struct PlusOperatorNode : ArithmeticOperatorNode<std::plus<>> { PlusOperatorNode() : ArithmeticOperatorNode<std::plus<>>("+", 5) { } };
    struct MinusOperatorNode : ArithmeticOperatorNode<std::minus<>> { MinusOperatorNode() : ArithmeticOperatorNode<std::minus<>>("-", 5) { } };
    struct MultiplyOperatorNode : ArithmeticOperatorNode<std::multiplies<>> { MultiplyOperatorNode() : ArithmeticOperatorNode<std::multiplies<>>("*", 10) { } };
    struct DivideOperatorNode : ArithmeticOperatorNode<std::divides<>> { DivideOperatorNode() : ArithmeticOperatorNode<std::divides<>>("/", 10) { } };

    template <class Function>
    struct NumericalComparaisonOperatorNode : OperatorNodeType {
        NumericalComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = context.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = context.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Parser::Node();
            switch (op1.variant.type) {
                return Parser::Node();
                case Parser::Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Parser::Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.f)));
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

    struct LessThanOperatorNode : NumericalComparaisonOperatorNode<std::less<>> { LessThanOperatorNode() : NumericalComparaisonOperatorNode<std::less<>>("<", 2) {  } };
    struct LessThanEqualOperatorNode : NumericalComparaisonOperatorNode<std::less_equal<>> { LessThanEqualOperatorNode() : NumericalComparaisonOperatorNode<std::less_equal<>>("<=", 2) {  } };
    struct GreaterThanOperatorNode : NumericalComparaisonOperatorNode<std::greater<>> { GreaterThanOperatorNode() : NumericalComparaisonOperatorNode<std::greater<>>(">", 2) {  } };
    struct GreaterThanEqualOperatorNode : NumericalComparaisonOperatorNode<std::greater_equal<>> { GreaterThanEqualOperatorNode() : NumericalComparaisonOperatorNode<std::greater_equal<>>(">=", 2) {  } };

    template <class Function>
    struct ArithmeticFilterNode : FilterNodeType {
        ArithmeticFilterNode(const string& symbol) : FilterNodeType(symbol, 1, 1) { }

        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Parser::Node render(const Context& context, const Parser::Node& node, Variable& store) const {
            assert(node.children[1]->type && node.children[1]->type->type == NodeType::Type::ARGUMENTS);
            Parser::Node op1 = context.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = context.retrieveRenderedNode(*node.children[1]->children[0].get(), store);
            if (op1.type || op2.type)
                return Parser::Node();
            switch (op1.variant.type) {
                case Parser::Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Parser::Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Parser::Variant::Type::INT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Parser::Variant::Type::FLOAT:
                            return Parser::Node(Parser::Variant(operate(op1.variant.f, op2.variant.f)));
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

    struct PlusFilterNode : ArithmeticFilterNode<std::plus<>> { PlusFilterNode() : ArithmeticFilterNode<std::plus<>>("plus") { } };
    struct MinusFilterNode : ArithmeticFilterNode<std::minus<>> { MinusFilterNode() : ArithmeticFilterNode<std::minus<>>("minus") { } };
    struct MultiplyFilterNode : ArithmeticFilterNode<std::multiplies<>> { MultiplyFilterNode() : ArithmeticFilterNode<std::multiplies<>>("times") { } };
    struct DivideFilterNode : ArithmeticFilterNode<std::divides<>> { DivideFilterNode() : ArithmeticFilterNode<std::divides<>>("divided_by") { } };

    void StandardDialect::implement(Context& context) {
        context.registerType<IfNode>();
        context.registerType<ForNode>();
        context.registerType<ForNode::InOperatorNode>();
        context.registerType<AssignNode>();

        context.registerType<AssignOperator>();
        context.registerType<PlusOperatorNode>();
        context.registerType<MinusOperatorNode>();
        context.registerType<MultiplyOperatorNode>();
        context.registerType<DivideOperatorNode>();

        context.registerType<LessThanOperatorNode>();
        context.registerType<LessThanEqualOperatorNode>();
        context.registerType<GreaterThanOperatorNode>();
        context.registerType<GreaterThanEqualOperatorNode>();

        context.registerType<PlusFilterNode>();
        context.registerType<MinusFilterNode>();
        context.registerType<MultiplyFilterNode>();
        context.registerType<DivideFilterNode>();
    }
}
