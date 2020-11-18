#include "context.h"
#include "dialect.h"
#include "parser.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace Liquid {
    struct AssignNode : TagNodeType {
        AssignNode() : TagNodeType(Composition::FREE, "assign", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto& argumentNode = node.children.front();
            auto& assignmentNode = argumentNode->children.front();
            auto& variableNode = assignmentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(renderer, *variableNode.get(), store, true);
                if (targetVariable) {
                    auto& operandNode = assignmentNode->children.back();
                    Parser::Node node = renderer.retrieveRenderedNode(*operandNode.get(), store);
                    assert(!node.type);
                    node.variant.inject(*targetVariable);
                }
            }
            return Parser::Node();
        }
    };

    struct AssignOperator : OperatorNodeType {
        AssignOperator() : OperatorNodeType("=", Arity::BINARY, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
    };

    struct CaptureNode : TagNodeType {
        CaptureNode() : TagNodeType(Composition::ENCLOSED, "capture", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(renderer, *variableNode.get(), store, true);
                if (targetVariable)
                    targetVariable->assign(renderer.retrieveRenderedNode(*node.children[1].get(), store).getString());
            }
            return Parser::Node();
        }
    };

    struct IncrementNode : TagNodeType {
        IncrementNode() : TagNodeType(Composition::FREE, "increment", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(renderer, *variableNode.get(), store, true);
                if (targetVariable) {
                    long long i = -1;
                    targetVariable->getInteger(i);
                    targetVariable->assign(i+1);
                }
            }
            return Parser::Node();
        }
    };

     struct DecrementNode : TagNodeType {
        DecrementNode() : TagNodeType(Composition::FREE, "decrement", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(renderer, *variableNode.get(), store, true);
                if (targetVariable) {
                    long long i = 0;
                    targetVariable->getInteger(i);
                    targetVariable->assign(i-1);
                }
            }
            return Parser::Node();
        }
    };

    template <bool Inverse>
    struct BranchNode : TagNodeType {
        static Parser::Node internalRender(Renderer& renderer, const Parser::Node& node, Variable& store) {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            auto result = renderer.retrieveRenderedNode(*arguments->children.front().get(), store);
            assert(result.type == nullptr);

            bool truthy = result.variant.isTruthy();
            if (Inverse)
                truthy = !truthy;

            if (truthy)
                return renderer.retrieveRenderedNode(*node.children[1].get(), store);
            else {
                // Loop through the elsifs and elses, and anything that's true, run the next concatenation.
                for (size_t i = 2; i < node.children.size()-1; i += 2) {
                    auto conditionalResult = renderer.retrieveRenderedNode(*node.children[i].get(), store);
                    if (!conditionalResult.type && conditionalResult.variant.isTruthy())
                        return renderer.retrieveRenderedNode(*node.children[i+1].get(), store);
                }
                return Parser::Node();
            }
        }


        struct ElsifNode : TagNodeType {
            ElsifNode() : TagNodeType(Composition::FREE, "elsif", 1, 1) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                auto& arguments = node.children.front();
                return renderer.retrieveRenderedNode(*arguments->children.front().get(), store);
            }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                return Parser::Node(Parser::Variant(true));
            }
        };

        BranchNode(const std::string symbol) : TagNodeType(Composition::ENCLOSED, symbol, 1, 1) {
            intermediates["elsif"] = make_unique<ElsifNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }


        Parser::Error validate(const Context& context, const Parser::Node& node) const {
            auto elseNode = intermediates.find("else")->second.get();
            auto elsifNode = intermediates.find("elsif")->second.get();
            for (size_t i = 1; i < node.children.size(); ++i) {
                if (((i - 1) % 2) == 0) {
                    if (node.children[i]->type != elseNode && node.children[i]->type != elsifNode)
                        return Parser::Error(Parser::Error::Type::INVALID_SYMBOL);
                }
            }
            return Parser::Error();
        }


        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            return BranchNode<Inverse>::internalRender(renderer, node, store);
        }
    };



    struct IfNode : BranchNode<false> {
        IfNode() : BranchNode<false>("if") { }
    };

    struct UnlessNode : BranchNode<true> {
        UnlessNode() : BranchNode<true>("unless") { }
    };

    struct CaseNode : TagNodeType {
        struct WhenNode : TagNodeType {
            WhenNode() : TagNodeType(Composition::FREE, "when", 1, 1) { }

            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                return renderer.retrieveRenderedNode(*node.children[0]->children[0].get(), store);
            }
        };
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
        };

        CaseNode() : TagNodeType(Composition::ENCLOSED, "case", 1, 1) {
            intermediates["when"] = make_unique<WhenNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            auto result = renderer.retrieveRenderedNode(*arguments->children.front().get(), store);
            assert(result.type == nullptr);
            auto whenNodeType = intermediates.find("when")->second.get();
            // Loop through the whens and elses.
            // Skip the first concatenation; it's not needed.
            for (size_t i = 2; i < node.children.size()-1; i += 2) {
                if (node.children[i]->type == whenNodeType) {
                    auto conditionalResult = renderer.retrieveRenderedNode(*node.children[i].get(), store);
                    if (conditionalResult.variant == result.variant)
                        return renderer.retrieveRenderedNode(*node.children[i+1].get(), store);
                } else {
                    return renderer.retrieveRenderedNode(*node.children[i+1].get(), store);
                }
            }
            return Parser::Node();
        }
    };

    struct ForNode : TagNodeType {
        struct InOperatorNode : OperatorNodeType {
            InOperatorNode() :  OperatorNodeType("in", Arity::BINARY, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const { return Parser::Node(); }
        };

        struct BreakNode : TagNodeType {
            BreakNode() : TagNodeType(Composition::FREE, "break", 0, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                renderer.control = Renderer::Control::BREAK;
                return Parser::Node();
            }
        };

        struct ContinueNode : TagNodeType {
            ContinueNode() : TagNodeType(Composition::FREE, "continue", 0, 0) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                renderer.control = Renderer::Control::CONTINUE;
                return Parser::Node();
            }
        };

        struct ReverseQualifierNode : TagNodeType::QualifierNodeType {
            ReverseQualifierNode() : TagNodeType::QualifierNodeType("reversed", TagNodeType::QualifierNodeType::Arity::NONARY) { }
        };
        struct LimitQualifierNode : TagNodeType::QualifierNodeType {
            LimitQualifierNode() : TagNodeType::QualifierNodeType("limit", TagNodeType::QualifierNodeType::Arity::UNARY) { }
        };
        struct OffsetQualifierNode : TagNodeType::QualifierNodeType {
            OffsetQualifierNode() : TagNodeType::QualifierNodeType("offset", TagNodeType::QualifierNodeType::Arity::UNARY) { }
        };

        ForNode() : TagNodeType(Composition::ENCLOSED, "for") {
            registerType<ElseNode>();
            registerType<ReverseQualifierNode>();
            registerType<LimitQualifierNode>();
            registerType<OffsetQualifierNode>();
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            assert(arguments->children.size() >= 1);
            // Should be move to validation.
            assert(arguments->children[0]->type && arguments->children[0]->type->type == NodeType::Type::OPERATOR && arguments->children[0]->type->symbol == "in");
            assert(arguments->children[0]->children[0]->type && arguments->children[0]->children[0]->type->type == NodeType::Type::VARIABLE);
            // Should be deleted eventually, as they're handled by the parser.
            assert(arguments->children[0]->children.size() == 2);

            auto& variableNode = arguments->children[0]->children[0];
            Variable* targetVariable = static_cast<const Context::VariableNode*>(variableNode->type)->getVariable(renderer, *variableNode, store, true);
            assert(targetVariable);

            Parser::Node result = renderer.retrieveRenderedNode(*arguments->children[0]->children[1].get(), store);

            if (result.type != nullptr || (result.variant.type != Parser::Variant::Type::VARIABLE && result.variant.type != Parser::Variant::Type::ARRAY)) {
                if (node.children.size() >= 4) {
                    // Run the else statement if there is one.
                    return renderer.retrieveRenderedNode(*node.children[3].get(), store);
                }
                return Parser::Node();
            }

            bool reversed = false;
            int start = 0;
            int limit = -1;
            bool hasLimit = false;

            const NodeType* reversedQualifier = qualifiers.find("reversed")->second.get();
            const NodeType* limitQualifier = qualifiers.find("limit")->second.get();
            const NodeType* offsetQualifier = qualifiers.find("offset")->second.get();

            for (size_t i = 1; i < arguments->children.size(); ++i) {
                Parser::Node* child = arguments->children[i].get();
                if (child->type && child->type->type == NodeType::Type::QUALIFIER) {
                    const NodeType* argumentType = arguments->children[i]->type;
                    if (reversedQualifier == argumentType)
                        reversed = true;
                    else if (limitQualifier == argumentType) {
                        auto result = renderer.retrieveRenderedNode(*child->children[0].get(), store);
                        if (!result.type && result.variant.isNumeric()) {
                            limit = (int)result.variant.getInt();
                            hasLimit = true;
                        }
                    } else if (offsetQualifier == argumentType) {
                        auto result = renderer.retrieveRenderedNode(*child->children[0].get(), store);
                        if (!result.type && result.variant.isNumeric())
                            start = std::max((int)result.variant.getInt(), 0);
                    }
                }
            }


            struct ForLoopContext {
                Renderer& renderer;
                const Parser::Node& node;
                Variable& store;
                Variable& targetVariable;
                bool (*iterator)(ForLoopContext& forloopContext);
                long long length;
                string result;
                long long idx = 0;
            };
            auto iterator = +[](ForLoopContext& forLoopContext) {
                Variable* forLoopVariable;
                Variable* index0, *index, *rindex, *rindex0, *first, *last, *length;
                forLoopContext.store.getDictionaryVariable(forLoopVariable, "forloop", true);
                forLoopVariable->getDictionaryVariable(index0, "index0", true);
                forLoopVariable->getDictionaryVariable(index, "index", true);
                forLoopVariable->getDictionaryVariable(rindex, "rindex", true);
                forLoopVariable->getDictionaryVariable(rindex0, "rindex0", true);
                forLoopVariable->getDictionaryVariable(first, "first", true);
                forLoopVariable->getDictionaryVariable(last, "last", true);
                forLoopVariable->getDictionaryVariable(length, "length", true);
                index0->assign(forLoopContext.idx);
                index->assign(forLoopContext.idx+1);
                rindex0->assign(forLoopContext.length - forLoopContext.idx);
                rindex->assign(forLoopContext.length - (forLoopContext.idx+1));
                first->assign(forLoopContext.idx == 0);
                last->assign(forLoopContext.idx == forLoopContext.length-1);
                length->assign(forLoopContext.length);
                forLoopContext.result.append(forLoopContext.renderer.retrieveRenderedNode(*forLoopContext.node.children[1].get(), forLoopContext.store).getString());
                ++forLoopContext.idx;
                if (forLoopContext.renderer.control != Renderer::Control::NONE)  {
                    if (forLoopContext.renderer.control == Renderer::Control::BREAK) {
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                        return false;
                    } else
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                }
                return true;
            };

            ForLoopContext forLoopContext = { renderer, node, store, *targetVariable, iterator };

            forLoopContext.length = result.variant.type == Parser::Variant::Type::ARRAY ? result.variant.a.size() : static_cast<Variable*>(result.variant.p)->getArraySize();
            if (!hasLimit)
                limit = forLoopContext.length;
            else if (limit < 0)
                limit = std::max((int)(limit+forLoopContext.length), 0);
            if (result.variant.type == Parser::Variant::Type::ARRAY) {
                int endIndex = std::min(limit+start-1, (int)forLoopContext.length-1);
                if (reversed) {
                    for (int i = endIndex; i >= start; --i) {
                        forLoopContext.targetVariable.clear();
                        result.variant.a[i].inject(forLoopContext.targetVariable);
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                } else {
                    for (int i = start; i <= endIndex; ++i) {
                        forLoopContext.targetVariable.clear();
                        result.variant.a[i].inject(forLoopContext.targetVariable);
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                }
            } else {
                static_cast<Variable*>(result.variant.p)->iterate(+[](Variable* variable, void* data) {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    forLoopContext.targetVariable.assign(*variable);
                    return forLoopContext.iterator(forLoopContext);
                }, const_cast<void*>((void*)&forLoopContext), start, limit, reversed);
            }
            if (forLoopContext.idx == 0 && node.children.size() >= 4) {
                // Run the else statement if there is one.
                return renderer.retrieveRenderedNode(*node.children[3].get(), store);
            }
            return Parser::Node(forLoopContext.result);
        }
    };

    struct CycleNode : TagNodeType {
        CycleNode() : TagNodeType(Composition::FREE, "cycle", 2) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            assert(node.children.size() == 1 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            Variable* forloopVariable;
            if (store.getDictionaryVariable(forloopVariable, "forloop", false)) {
                Variable* index0;
                if (forloopVariable->getDictionaryVariable(index0, "index0", false)) {
                    long long i;
                    if (index0->getInteger(i))
                        return renderer.retrieveRenderedNode(*arguments->children[i % arguments->children.size()].get(), store);
                }
            }
            return Parser::Node();
        }
    };


    template <class Function>
    struct ArithmeticOperatorNode : OperatorNodeType {
        ArithmeticOperatorNode(const char* symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
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
    struct ModuloOperatorNode : OperatorNodeType {
        ModuloOperatorNode() : OperatorNodeType("%", Arity::BINARY, 10) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Parser::Node();
            long long divisor = op2.variant.getInt();
            if (divisor == 0)
                return Parser::Node();
            return Parser::Variant(op1.variant.getInt() % divisor);
        }
    };

    template <class Function>
    struct NumericalComparaisonOperatorNode : OperatorNodeType {
        NumericalComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
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

    template <class Function>
    struct QualitativeComparaisonOperatorNode : OperatorNodeType {
        QualitativeComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
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
                case Parser::Variant::Type::STRING:
                    if (op2.variant.type != Parser::Variant::Type::STRING)
                        return Parser::Node(Parser::Variant(false));
                    return Parser::Node(Parser::Variant(operate(op1.variant.s, op2.variant.s)));
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
    struct EqualOperatorNode : QualitativeComparaisonOperatorNode<std::equal_to<>> { EqualOperatorNode() : QualitativeComparaisonOperatorNode<std::equal_to<>>("==", 2) {  } };
    struct NotEqualOperatorNode : QualitativeComparaisonOperatorNode<std::not_equal_to<>> { NotEqualOperatorNode() : QualitativeComparaisonOperatorNode<std::not_equal_to<>>("!=", 2) {  } };

    struct AndOperatorNode : OperatorNodeType {
        AndOperatorNode() : OperatorNodeType("and", Arity::BINARY, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (!op1.type || !op1.variant.isTruthy())
                return Parser::Node(Parser::Variant(false));
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Parser::Node(Parser::Variant(op2.type && op2.variant.isTruthy()));
        }
    };
    struct OrOperatorNode : OperatorNodeType {
        OrOperatorNode() : OperatorNodeType("or", Arity::BINARY, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (op1.type && op1.variant.isTruthy())
                return Parser::Node(Parser::Variant(true));
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Parser::Node(Parser::Variant(op2.type && op2.variant.isTruthy()));
        }
    };

    struct RangeOperatorNode : OperatorNodeType {
        RangeOperatorNode() : OperatorNodeType("..", Arity::BINARY, 10) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Parser::Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            // Requires integers.
            if (op1.type || op2.type || op1.variant.type != Parser::Variant::Type::INT || op2.variant.type != Parser::Variant::Type::INT)
                return Parser::Node();
            auto result = Parser::Node(Parser::Variant(vector<Parser::Variant>()));
            // TODO: This can allocate a lot of memory. Short-circuit that; but should be plugge dinto the allocator.
            long long size = op2.variant.i - op1.variant.i;
            if (size > 1000)
                return Parser::Node();
            result.variant.a.reserve(size);
            for (long long i = op1.variant.i; i <= op2.variant.i; ++i)
                result.variant.a.push_back(Parser::Variant(i));
            return result;
        }
    };

    template <class Function>
    struct ArithmeticFilterNode : FilterNodeType {
        ArithmeticFilterNode(const string& symbol) : FilterNodeType(symbol, 1, 1) { }

        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            assert(node.children[1]->type && node.children[1]->type->type == NodeType::Type::ARGUMENTS);
            Parser::Node op1 = getOperand(renderer, node, store);
            Parser::Node op2 = getArgument(renderer, node, store, 0);
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
    struct ModuloFilterNode : FilterNodeType {
        ModuloFilterNode() : FilterNodeType("modulo", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            long long divisor = argument.variant.getInt();
            if (divisor == 0)
                return Parser::Node();
            return Parser::Node(Parser::Variant(operand.variant.getInt() % divisor));
        }
    };

    struct AbsFilterNode : FilterNodeType {
        AbsFilterNode() : FilterNodeType("abs", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT:
                        return Parser::Node(fabs(operand.variant.f));
                    case Parser::Variant::Type::INT:
                        return Parser::Node((long long)abs(operand.variant.i));
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };

    struct CeilFilterNode : FilterNodeType {
        CeilFilterNode() : FilterNodeType("ceil", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT:
                        return Parser::Node(ceil(operand.variant.f));
                    case Parser::Variant::Type::INT:
                        return Parser::Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };

    struct FloorFilterNode : FilterNodeType {
        FloorFilterNode() : FilterNodeType("floor", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT:
                        return Parser::Node(ceil(operand.variant.f));
                    case Parser::Variant::Type::INT:
                        return Parser::Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };


    struct RoundFilterNode : FilterNodeType {
        RoundFilterNode() : FilterNodeType("round", 0, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT: {
                        if (!argument.type) {
                            int digits = argument.variant.getInt();
                            int multiplier = pow(10, digits);
                            double number = operand.variant.f * multiplier;
                            number = round(number);
                            return Parser::Node(number / multiplier);
                        }
                        return Parser::Node(round(operand.variant.f));
                    }
                    case Parser::Variant::Type::INT:
                        return Parser::Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };

    struct AtMostFilterNode : FilterNodeType {
        AtMostFilterNode() : FilterNodeType("at_most", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT:
                        return Parser::Node(std::min(operand.variant.f, argument.variant.getFloat()));
                    case Parser::Variant::Type::INT:
                        return Parser::Node(std::min(operand.variant.i, argument.variant.getInt()));
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };

    struct AtLeastFilterNode : FilterNodeType {
        AtLeastFilterNode() : FilterNodeType("at_least", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Parser::Variant::Type::FLOAT:
                        return Parser::Node(std::max(operand.variant.f, argument.variant.getFloat()));
                    case Parser::Variant::Type::INT:
                        return Parser::Node(std::max(operand.variant.i, argument.variant.getInt()));
                    default:
                    break;
                }
            }
            return Parser::Node();
        }
    };

    struct AppendFilterNode : FilterNodeType {
        AppendFilterNode() : FilterNodeType("append", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            return Parser::Variant(operand.getString() + argument.getString());
        }
    };

    struct camelCaseFilterNode : FilterNodeType {
        camelCaseFilterNode() : FilterNodeType("camelcase", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            // TODO
            return Parser::Node();
        }
    };

    struct CapitalizeFilterNode : FilterNodeType {
        CapitalizeFilterNode() : FilterNodeType("capitalize", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            str[0] = toupper(str[0]);
            return Parser::Node(str);
        }
    };
    struct DowncaseFilterNode : FilterNodeType {
        DowncaseFilterNode() : FilterNodeType("downcase", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
            return Parser::Variant(str);
        }
    };
    struct HandleGenericFilterNode : FilterNodeType {
        HandleGenericFilterNode(const string& symbol) : FilterNodeType(symbol, 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            string str = getOperand(renderer, node, store).getString();
            string accumulator;
            accumulator.reserve(str.size());
            bool lastHyphen = true;
            for (auto it = str.begin(); it != str.end(); ++it) {
                if (isalnum(*it)) {
                    if (isupper(*it)) {
                        accumulator.push_back(tolower(*it));
                    } else {
                        accumulator.push_back(*it);
                    }
                    lastHyphen = false;
                } else if (!lastHyphen) {
                    accumulator.push_back('-');
                }
            }
            return Parser::Variant(accumulator);
        }
    };
    struct HandleFilterNode : HandleGenericFilterNode { HandleFilterNode() : HandleGenericFilterNode("handle") { } };
    struct HandleizeFilterNode : HandleGenericFilterNode { HandleizeFilterNode() : HandleGenericFilterNode("handleize") { } };

    struct PluralizeFilterNode : FilterNodeType {
        PluralizeFilterNode() : FilterNodeType("pluralize", 2, 2) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument1 = getArgument(renderer, node, store, 1);
            auto argument2 = getArgument(renderer, node, store, 2);
            if (operand.type)
                return Parser::Node();
            return Parser::Variant(operand.variant.getInt() > 1 ? argument2.getString() : argument1.getString());
        }
    };

    struct PrependFilterNode : FilterNodeType {
        PrependFilterNode() : FilterNodeType("prepend", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            return Parser::Variant(argument.getString() + operand.getString());
        }
    };
    struct RemoveFilterNode : FilterNodeType {
        RemoveFilterNode() : FilterNodeType("remove", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            string accumulator;
            string str = operand.getString();
            string rm = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(rm, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                start = idx + rm.size();
            }
            accumulator.append(str, start, str.size() - start);
            return Parser::Variant(accumulator);
        }
    };
    struct RemoveFirstFilterNode : FilterNodeType {
        RemoveFirstFilterNode() : FilterNodeType("removefirst", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            string accumulator;
            string str = operand.getString();
            string rm = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(rm, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                start = idx + rm.size();
                break;
            }
            accumulator.append(str, start, str.size() - start);
            return Parser::Variant(accumulator);
        }
    };
    struct ReplaceFilterNode : FilterNodeType {
        ReplaceFilterNode() : FilterNodeType("replace", 2, 2) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            string accumulator;
            string str = operand.getString();
            string rm = argument.getString();
            string replacement = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(rm, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                accumulator.append(replacement);
                start = idx + rm.size();
            }
            accumulator.append(str, start, str.size() - start);
            return Parser::Variant(accumulator);
        }
    };
    struct ReplaceFirstFilterNode : FilterNodeType {
        ReplaceFirstFilterNode() : FilterNodeType("replacefirst", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            string accumulator;
            string str = operand.getString();
            string rm = argument.getString();
            string replacement = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(rm, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                accumulator.append(replacement);
                start = idx + rm.size();
                break;
            }
            accumulator.append(str, start, str.size() - start);
            return Parser::Variant(accumulator);
        }
    };
    struct SliceFilterNode : FilterNodeType {
        SliceFilterNode() : FilterNodeType("slice", 1, 2) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            auto arg2 = getArgument(renderer, node, store, 0);

            if (operand.type || arg1.type || arg2.type || operand.variant.type != Parser::Variant::Type::STRING)
                return Parser::Node();
            string str = operand.getString();
            long long offset = std::max(std::min(arg1.variant.getInt(), (long long)str.size()), 0LL);
            long long size = std::min(arg2.variant.getInt(), (long long)(str.size() - offset));
            return Parser::Variant(str.substr(offset, size));
        }
    };
    struct SplitFilterNode : FilterNodeType {
        SplitFilterNode() : FilterNodeType("split", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            Parser::Variant result({ });
            string str = operand.getString();
            string splitter = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(splitter, start)) != string::npos) {
                if (idx > start)
                    result.a.push_back(Parser::Variant(str.substr(start, idx - start)));
                start = idx + splitter.size();
            }
            result.a.push_back(str.substr(start, str.size() - start));
            return Parser::Variant(std::move(result));
        }
    };
    struct StripFilterNode : FilterNodeType {
        StripFilterNode() : FilterNodeType("strip", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            size_t start, end;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Parser::Node(str.substr(start, end - start + 1));
        }
    };
    struct LStripFilterNode : FilterNodeType {
        LStripFilterNode() : FilterNodeType("lstrip", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            size_t start;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            return Parser::Node(str.substr(start, str.size() - start));
        }
    };
    struct RStripFilterNode : FilterNodeType {
        RStripFilterNode() : FilterNodeType("rstrip", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            size_t end;
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Parser::Node(str.substr(0, end + 1));
        }
    };
    struct StripNewlinesFilterNode : FilterNodeType {
        StripNewlinesFilterNode() : FilterNodeType("strip_newlines", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            string str = operand.getString();
            string accumulator;
            accumulator.reserve(str.size());
            for (auto it = str.begin(); it != str.end(); ++it) {
                if (*it != '\n' && *it != '\r')
                    accumulator.push_back(*it);
            }
            return Parser::Node(accumulator);
        }
    };
    struct TruncateFilterNode : FilterNodeType {
        TruncateFilterNode() : FilterNodeType("truncate", 1, 2) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto characterCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
            if (operand.type || characterCount.type || customEllipsis.type)
                return Parser::Node();
            string ellipsis = "...";
            if (customEllipsis.variant.type == Parser::Variant::Type::STRING)
                ellipsis = customEllipsis.getString();
            long long count = characterCount.variant.getInt();
            if (count > (long long)ellipsis.size()) {
                string str = operand.getString();
                return Parser::Variant(str.substr(0, std::min((long long)(count - ellipsis.size()), (long long)str.size())) + ellipsis);
            }
            return Parser::Variant(ellipsis.substr(0, std::min(count, (long long)ellipsis.size())));
        }
    };
    struct TruncateWordsFilterNode : FilterNodeType {
        TruncateWordsFilterNode() : FilterNodeType("truncatewords", 1, 2) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto wordCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
            if (operand.type || wordCount.type || customEllipsis.type)
                return Parser::Node();
            string ellipsis = "...";
            string str = operand.getString();
            int targetWordCount = wordCount.variant.getInt();
            int ongoingWordCount = 0;
            bool previousBlank = false;
            size_t i;
            for (i = 0; i < str.size(); ++i) {
                if (isblank(str[i])) {
                    if (previousBlank) {
                        if (++ongoingWordCount == targetWordCount)
                            break;
                    }
                    previousBlank = true;
                } else {
                    previousBlank = false;
                }
            }
            return Parser::Variant(str.substr(0, i-1));
        }
    };
    struct UpcaseFilterNode : FilterNodeType {
        UpcaseFilterNode() : FilterNodeType("upcase", 0, 0) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::toupper(c); });
            return Parser::Variant(str);
        }
    };

    struct ArrayFilterNodeType : FilterNodeType {
        ArrayFilterNodeType(const std::string& symbol, int minArguments = -1, int maxArguments = -1) : FilterNodeType(symbol, minArguments, maxArguments) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Parser::Node();

            return Parser::Node();
        }
    };

    struct JoinFilterNode : ArrayFilterNodeType {
        JoinFilterNode() : ArrayFilterNodeType("join", 0, 1) { }

        Parser::Node variableOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Variable& operand) {
            struct JoinStruct {
                string accumulator;
                string joiner = "";
                int idx = 0;
            };
            JoinStruct joinStruct;
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                joinStruct.joiner = argument.getString();

            operand.iterate(+[](Variable* variable, void* data) {
                JoinStruct& joinStruct = *(JoinStruct*)data;
                string part;
                if (joinStruct.idx++ > 0)
                    joinStruct.accumulator.append(joinStruct.joiner);
                if (variable->getString(part))
                    joinStruct.accumulator.append(part);
                return true;
            }, &joinStruct);
            return Parser::Node(joinStruct.accumulator);
        }

        Parser::Node variantOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Parser::Variant& operand) {
            string accumulator;
            if (operand.type != Parser::Variant::Type::ARRAY || operand.a.size() == 0)
                return Parser::Node();
            string joiner = "";
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                joiner = argument.getString();
            for (size_t i = 0; i < operand.a.size(); ++i) {
                if (i > 0)
                    accumulator.append(joiner);
                accumulator.append(operand.a[i].getString());
            }
            return operand.a[operand.a.size()-1];
        }
    };

    struct ConcatFilterNode : ArrayFilterNodeType {
        ConcatFilterNode() : ArrayFilterNodeType("concat", 1, 1) { }

        void accumulate(Parser::Variant& accumulator, const Variable& v) const {
            v.iterate(+[](Variable* variable, void* data) {
                    static_cast<Parser::Variant*>(data)->a.push_back(variable);
                return true;
            }, &accumulator);
        }

        void accumulate(Parser::Variant& accumulator, const Parser::Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it)
                accumulator.a.push_back(*it);
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Parser::Node();
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    accumulate(accumulator, *operand.variant.v);
                break;
                case Parser::Variant::Type::ARRAY:
                    accumulate(accumulator, operand.variant.a);
                break;
                default:
                    return Parser::Node();
            }
            if (argument.type)
                return accumulator;
            switch (argument.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    accumulate(accumulator, *argument.variant.v);
                break;
                case Parser::Variant::Type::ARRAY:
                    accumulate(accumulator, argument.variant.a);
                break;
                default:
                    return accumulator;
            }
            return accumulator;
        }
    };

    struct MapFilterNode : ArrayFilterNodeType {
        MapFilterNode() : ArrayFilterNodeType("map", 1, 1) { }

        struct MapStruct {
            string property;
            Parser::Variant accumulator;
        };

        void accumulate(MapStruct& mapStruct, const Variable& v) const {
            v.iterate(+[](Variable* variable, void* data) {
                MapStruct& mapStruct = *static_cast<MapStruct*>(data);
                Variable* target;
                if (variable->getDictionaryVariable(target, mapStruct.property))
                    mapStruct.accumulator.a.push_back(target);
                else
                    mapStruct.accumulator.a.push_back(Parser::Variant());
                return true;
            }, &mapStruct);
        }

        void accumulate(MapStruct& mapStruct, const Parser::Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (it->type == Parser::Variant::Type::VARIABLE) {
                    Variable* target;
                    if (it->v->getDictionaryVariable(target, mapStruct.property))
                        mapStruct.accumulator.a.push_back(target);
                    else
                        mapStruct.accumulator.a.push_back(Parser::Variant());
                } else {
                    mapStruct.accumulator.a.push_back(Parser::Variant());
                }
            }
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Parser::Node();
            MapStruct mapStruct = { argument.getString() };
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    accumulate(mapStruct, *operand.variant.v);
                break;
                case Parser::Variant::Type::ARRAY:
                    accumulate(mapStruct, operand.variant.a);
                break;
                default:
                    return Parser::Node();
            }
            return mapStruct.accumulator;
        }
    };

    struct ReverseFilterNode : ArrayFilterNodeType {
        ReverseFilterNode() : ArrayFilterNodeType("reverse", 0, 0) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();
            auto& v = operand.variant;
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    v.v->iterate(+[](Variable* variable, void* data) {
                        static_cast<Parser::Variant*>(data)->a.push_back(variable);
                        return true;
                    }, &accumulator);
                break;
                case Parser::Variant::Type::ARRAY:
                    for (auto it = v.a.begin(); it != v.a.end(); ++it)
                        accumulator.a.push_back(*it);
                break;
                default:
                    return Parser::Node();
            }
            std::reverse(accumulator.a.begin(), accumulator.a.end());
            return accumulator;
        }
    };

    struct SortFilterNode : ArrayFilterNodeType {
        SortFilterNode() : ArrayFilterNodeType("sort", 0, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            Parser::Variant accumulator;
            string property;
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Parser::Node();
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE: {
                    operand.variant.v->iterate(+[](Variable* variable, void* data) {
                        static_cast<Parser::Variant*>(data)->a.push_back(variable);
                        return true;
                    }, &accumulator);
                } break;
                case Parser::Variant::Type::ARRAY: {
                    auto& v = operand.variant;
                    for (auto it = v.a.begin(); it != v.a.end(); ++it)
                        accumulator.a.push_back(*it);
                } break;
                default:
                    return Parser::Node();
            }

            if (!argument.type && argument.variant.type == Parser::Variant::Type::STRING) {
                property = argument.getString();
                std::sort(accumulator.a.begin(), accumulator.a.end(), [&property](Parser::Variant& a, Parser::Variant& b) -> bool {
                    if (a.type != Parser::Variant::Type::VARIABLE || b.type != Parser::Variant::Type::VARIABLE)
                        return false;
                    Variable* targetA, *targetB;
                    if (!a.v->getDictionaryVariable(targetA, property))
                        return false;
                    if (!b.v->getDictionaryVariable(targetB, property))
                        return false;
                    return targetA->compare(*targetB) < 0;
                });
            } else {
                std::sort(accumulator.a.begin(), accumulator.a.end(), [](Parser::Variant& a, Parser::Variant& b) -> bool { return a < b; });
            }
            return accumulator;
        }
    };


    struct WhereFilterNode : ArrayFilterNodeType {
        WhereFilterNode() : ArrayFilterNodeType("where", 1, 2) { }

        struct WhereStruct {
            string property;
            Parser::Variant value;
            Parser::Variant accumulator;
        };

        void accumulate(WhereStruct& whereStruct, const Variable& v) const {
            v.iterate(+[](Variable* variable, void* data) {
                WhereStruct& whereStruct = *static_cast<WhereStruct*>(data);
                Variable* target;
                if (variable->getDictionaryVariable(target, whereStruct.property)) {
                    if (whereStruct.property.empty()) {
                        if (target->getTruthy())
                            whereStruct.accumulator.a.push_back(variable);
                    } else {
                        std::string str;
                        if (target->getString(str)) {
                            // TODO.
                            // if (str == whereStruct.value)
                            //    whereStruct.accumulator.a.push_back(variable);
                        }
                    }
                }
                return true;
            }, &whereStruct);
        }

        void accumulate(WhereStruct& whereStruct, const Parser::Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (it->type == Parser::Variant::Type::VARIABLE) {
                    Variable* target;
                    if (it->v->getDictionaryVariable(target, whereStruct.property)) {
                        if (whereStruct.property.empty()) {
                            if (target->getTruthy())
                                whereStruct.accumulator.a.push_back(*it);
                        } else {
                            std::string str;
                            if (target->getString(str)) {
                                // TODO
                                //if (str == whereStruct.value)
                                //    whereStruct.accumulator.a.push_back(*it);
                            }
                        }
                    }
                }
            }
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Parser::Node();
            auto arg2 = getArgument(renderer, node, store, 1);
            if (arg2.type)
                return Parser::Node();
            WhereStruct whereStruct = { arg1.getString(), arg2.variant };
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    accumulate(whereStruct, *operand.variant.v);
                break;
                case Parser::Variant::Type::ARRAY:
                    accumulate(whereStruct, operand.variant.a);
                break;
                default:
                    return Parser::Node();
            }
            return whereStruct.accumulator;
        }
    };

    struct UniqFilterNode : ArrayFilterNodeType {
        UniqFilterNode() : ArrayFilterNodeType("uniq", 0, 0) { }

        struct UniqStruct {
            std::unordered_set<size_t> hashes;
            Parser::Variant accumulator;
        };

        void accumulate(UniqStruct& uniqStruct, const Variable& v) const {
            v.iterate(+[](Variable* variable, void* data) {
                UniqStruct& uniqStruct = *static_cast<UniqStruct*>(data);
                Parser::Variant v = Parser::Variant::parse(*variable);
                if (!uniqStruct.hashes.emplace(v.hash()).second)
                    uniqStruct.accumulator.a.push_back(variable);
                return true;
            }, &uniqStruct);
        }

        void accumulate(UniqStruct& uniqStruct, const Parser::Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (!uniqStruct.hashes.emplace(it->hash()).second)
                    uniqStruct.accumulator.a.push_back(v);
            }
        }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Parser::Node();
            auto arg2 = getArgument(renderer, node, store, 1);
            if (arg2.type)
                return Parser::Node();
            UniqStruct uniqStruct;
            switch (operand.variant.type) {
                case Parser::Variant::Type::VARIABLE:
                    accumulate(uniqStruct, *operand.variant.v);
                break;
                case Parser::Variant::Type::ARRAY:
                    accumulate(uniqStruct, operand.variant.a);
                break;
                default:
                    return Parser::Node();
            }
            return uniqStruct.accumulator;
        }
    };



    struct FirstFilterNode : ArrayFilterNodeType {
        FirstFilterNode() : ArrayFilterNodeType("first", 0, 0) { }

        Parser::Node variableOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Variable& operand) const {
            Variable* v;
            if (!operand.getArrayVariable(v, 0))
                return Parser::Node();
            return Parser::Node(v);
        }

        Parser::Node variantOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Parser::Variant& operand) const {
            if (operand.type != Parser::Variant::Type::ARRAY || operand.a.size() == 0)
                return Parser::Node();
            return operand.a[0];
        }
    };

    struct LastFilterNode : ArrayFilterNodeType {
        LastFilterNode() : ArrayFilterNodeType("last", 0, 0) { }

        Parser::Node variableOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Variable& operand) const {
            Variable* v;
            long long size = operand.getArraySize();
            if (size == -1 || !operand.getArrayVariable(v, 0))
                return Parser::Node();
            return Parser::Node(v);
        }

        Parser::Node variantOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Parser::Variant& operand) const {
            if (operand.type != Parser::Variant::Type::ARRAY || operand.a.size() == 0)
                return Parser::Node();
            return operand.a[operand.a.size()-1];
        }
    };

    struct IndexFilterNode : ArrayFilterNodeType {
        IndexFilterNode() : ArrayFilterNodeType("index", 1, 1) { }

        Parser::Node variableOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Variable& operand) const {
            Variable* v;
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                return Parser::Node();
            int idx = argument.variant.i;
            if (!operand.getArrayVariable(v, idx))
                return Parser::Node();
            return Parser::Node(v);
        }

        Parser::Node variantOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Parser::Variant& operand) const {
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                return Parser::Node();
            long long idx = argument.variant.i;
            if (operand.type != Parser::Variant::Type::ARRAY || idx >= (long long)operand.a.size())
                return Parser::Node();
            return operand.a[idx];
        }
    };
    struct SizeFilterNode : FilterNodeType {
        SizeFilterNode() : FilterNodeType("size", 0, 0) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Parser::Node();

            switch (operand.variant.type) {
                case Parser::Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Parser::Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, *operand.variant.v);
                case Parser::Variant::Type::STRING:
                    return Parser::Variant((long long)operand.variant.s.size());
                default:
                    return Parser::Node();
            }
        }

        Parser::Node variableOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Variable& operand) const {
            long long size = operand.getArraySize();
            return size != -1 ? Parser::Node(size) : Parser::Node();
        }

        Parser::Node variantOperate(Renderer& renderer, const Parser::Node& node, Variable& store, const Parser::Variant& operand) const {
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                return Parser::Node();
            long long idx = argument.variant.i;
            if (operand.type != Parser::Variant::Type::ARRAY || idx >= (long long)operand.a.size())
                return Parser::Node();
            return operand.a[idx];
        }
    };


    struct DefaultFilterNode : FilterNodeType {
        DefaultFilterNode() : FilterNodeType("default", 1, 1) { }
        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || operand.variant.isTruthy())
                return operand;
            return argument;
        }
    };


    void StandardDialect::implement(Context& context) {
        // Control flow tags.
        context.registerType<IfNode>();
        context.registerType<UnlessNode>();
        context.registerType<CaseNode>();

        // Iteration tags.
        context.registerType<ForNode>();
        context.registerType<CycleNode>();
        context.registerType<ForNode::InOperatorNode>();
        context.registerType<ForNode::BreakNode>();
        context.registerType<ForNode::ContinueNode>();

        // Variable tags.
        context.registerType<AssignNode>();
        context.registerType<CaptureNode>();
        context.registerType<IncrementNode>();
        context.registerType<DecrementNode>();

        // Standard set of operators.
        context.registerType<AssignOperator>();
        context.registerType<PlusOperatorNode>();
        context.registerType<MinusOperatorNode>();
        context.registerType<MultiplyOperatorNode>();
        context.registerType<DivideOperatorNode>();
        context.registerType<ModuloOperatorNode>();
        context.registerType<LessThanOperatorNode>();
        context.registerType<LessThanEqualOperatorNode>();
        context.registerType<GreaterThanOperatorNode>();
        context.registerType<GreaterThanEqualOperatorNode>();
        context.registerType<EqualOperatorNode>();
        context.registerType<NotEqualOperatorNode>();
        context.registerType<AndOperatorNode>();
        context.registerType<OrOperatorNode>();
        context.registerType<RangeOperatorNode>();

        // Math filters.
        context.registerType<PlusFilterNode>();
        context.registerType<MinusFilterNode>();
        context.registerType<MultiplyFilterNode>();
        context.registerType<DivideFilterNode>();
        context.registerType<AbsFilterNode>();
        context.registerType<AtMostFilterNode>();
        context.registerType<AtLeastFilterNode>();
        context.registerType<CeilFilterNode>();
        context.registerType<FloorFilterNode>();
        context.registerType<RoundFilterNode>();
        context.registerType<ModuloFilterNode>();

        // String filters. Those that are HTML speciifc, or would require minor external libraries
        // are left off for now.
        context.registerType<AppendFilterNode>();
        context.registerType<camelCaseFilterNode>();
        context.registerType<CapitalizeFilterNode>();
        context.registerType<DowncaseFilterNode>();
        context.registerType<HandleFilterNode>();
        context.registerType<HandleizeFilterNode>();
        context.registerType<PluralizeFilterNode>();
        context.registerType<PrependFilterNode>();
        context.registerType<RemoveFilterNode>();
        context.registerType<RemoveFirstFilterNode>();
        context.registerType<ReplaceFilterNode>();
        context.registerType<ReplaceFirstFilterNode>();
        context.registerType<SliceFilterNode>();
        context.registerType<SplitFilterNode>();
        context.registerType<StripFilterNode>();
        context.registerType<LStripFilterNode>();
        context.registerType<RStripFilterNode>();
        context.registerType<StripNewlinesFilterNode>();
        context.registerType<TruncateFilterNode>();
        context.registerType<TruncateWordsFilterNode>();
        context.registerType<UpcaseFilterNode>();

        // context.registerType<EscapeFilterNode>();
        // context.registerType<MD5FilterNode>();
        // context.registerType<SHA1FilterNode>();
        // context.registerType<SHA256FilterNode>();
        // context.registerType<NewlineToBr>();
        // context.registerType<StripHTML>();

        // Array filters.
        context.registerType<JoinFilterNode>();
        context.registerType<FirstFilterNode>();
        context.registerType<LastFilterNode>();
        context.registerType<ConcatFilterNode>();
        context.registerType<IndexFilterNode>();
        context.registerType<MapFilterNode>();
        context.registerType<ReverseFilterNode>();
        context.registerType<SizeFilterNode>();
        context.registerType<SortFilterNode>();
        context.registerType<WhereFilterNode>();
        context.registerType<UniqFilterNode>();

        // Other filters.
        context.registerType<DefaultFilterNode>();
    }
}
