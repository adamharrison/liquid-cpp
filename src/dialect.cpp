#include "context.h"
#include "dialect.h"
#include "parser.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace Liquid {
    struct AssignNode : TagNodeType {
        AssignNode() : TagNodeType(Composition::FREE, "assign", 1, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto& argumentNode = node.children.front();
            auto& assignmentNode = argumentNode->children.front();
            auto& variableNode = assignmentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable;
                auto& operandNode = assignmentNode->children.back();
                Node node = renderer.retrieveRenderedNode(*operandNode.get(), store);
                assert(!node.type);
                renderer.inject(targetVariable, node.variant);

                static_cast<const Context::VariableNode*>(variableNode->type)->setVariable(renderer, *variableNode.get(), store, targetVariable);
            }
            return Node();
        }
    };

    struct AssignOperator : OperatorNodeType {
        AssignOperator() : OperatorNodeType("=", Arity::BINARY, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
    };

    struct CaptureNode : TagNodeType {
        CaptureNode() : TagNodeType(Composition::ENCLOSED, "capture", 1, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            const Context::VariableNode* variableTypeNode = static_cast<const Context::VariableNode*>(variableNode->type);
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable = variableTypeNode->resolver.createString(renderer, renderer.retrieveRenderedNode(*node.children[1].get(), store).getString().data());
                variableTypeNode->setVariable(renderer, *variableNode.get(), store, targetVariable);
            }
            return Node();
        }
    };

    struct IncrementNode : TagNodeType {
        IncrementNode() : TagNodeType(Composition::FREE, "increment", 1, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                const Context::VariableNode* variableNodeType = static_cast<const Context::VariableNode*>(variableNode->type);
                Variable targetVariable = variableNodeType->getVariable(renderer, *variableNode.get(), store);
                if (targetVariable.exists()) {
                    long long i = -1;
                    if (variableNodeType->resolver.getInteger(targetVariable,&i))
                        variableNodeType->setVariable(renderer, *variableNode.get(), targetVariable, variableNodeType->resolver.createInteger(renderer, i+1));
                }
            }
            return Node();
        }
    };

     struct DecrementNode : TagNodeType {
        DecrementNode() : TagNodeType(Composition::FREE, "decrement", 1, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                const Context::VariableNode* variableNodeType = static_cast<const Context::VariableNode*>(variableNode->type);
                Variable targetVariable = variableNodeType->getVariable(renderer, *variableNode.get(), store);
                if (targetVariable.exists()) {
                    long long i = -1;
                    if (variableNodeType->resolver.getInteger(targetVariable,&i))
                        variableNodeType->setVariable(renderer, *variableNode.get(), targetVariable, variableNodeType->resolver.createInteger(renderer, i-1));
                }
            }
            return Node();
        }
    };

    struct CommentNode : EnclosedNodeType {
        CommentNode() : EnclosedNodeType("comment", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
    };

    template <bool Inverse>
    struct BranchNode : TagNodeType {
        static Node internalRender(Renderer& renderer, const Node& node, Variable store) {
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
                return Node();
            }
        }


        struct ElsifNode : TagNodeType {
            ElsifNode() : TagNodeType(Composition::FREE, "elsif", 1, 1) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const {
                auto& arguments = node.children.front();
                return renderer.retrieveRenderedNode(*arguments->children.front().get(), store);
            }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const {
                return Node(Variant(true));
            }
        };

        BranchNode(const std::string symbol) : TagNodeType(Composition::ENCLOSED, symbol, 1, 1) {
            intermediates["elsif"] = make_unique<ElsifNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }


        Parser::Error validate(const Context& context, const Node& node) const {
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


        Node render(Renderer& renderer, const Node& node, Variable store) const {
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

            Node render(Renderer& renderer, const Node& node, Variable store) const {
                return renderer.retrieveRenderedNode(*node.children[0]->children[0].get(), store);
            }
        };
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        CaseNode() : TagNodeType(Composition::ENCLOSED, "case", 1, 1) {
            intermediates["when"] = make_unique<WhenNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
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
            return Node();
        }
    };

    struct ForNode : TagNodeType {
        struct InOperatorNode : OperatorNodeType {
            InOperatorNode() :  OperatorNodeType("in", Arity::BINARY, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        struct BreakNode : TagNodeType {
            BreakNode() : TagNodeType(Composition::FREE, "break", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const {
                renderer.control = Renderer::Control::BREAK;
                return Node();
            }
        };

        struct ContinueNode : TagNodeType {
            ContinueNode() : TagNodeType(Composition::FREE, "continue", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const {
                renderer.control = Renderer::Control::CONTINUE;
                return Node();
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

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            assert(arguments->children.size() >= 1);
            // Should be move to validation.
            assert(arguments->children[0]->type && arguments->children[0]->type->type == NodeType::Type::OPERATOR && arguments->children[0]->type->symbol == "in");
            assert(arguments->children[0]->children[0]->type && arguments->children[0]->children[0]->type->type == NodeType::Type::VARIABLE);
            // Should be deleted eventually, as they're handled by the parser.
            assert(arguments->children[0]->children.size() == 2);

            auto& variableNode = arguments->children[0]->children[0];

            // TODO: Should have an optimization for when the operand from "in" ia s sequence; so that it doesn't render out to a ridiclous thing, it
            // simply loops through the existing stuff.
            Node result = renderer.retrieveRenderedNode(*arguments->children[0]->children[1].get(), store);

            if (result.type != nullptr || (result.variant.type != Variant::Type::VARIABLE && result.variant.type != Variant::Type::ARRAY)) {
                if (node.children.size() >= 4) {
                    // Run the else statement if there is one.
                    return renderer.retrieveRenderedNode(*node.children[3].get(), store);
                }
                return Node();
            }

            bool reversed = false;
            int start = 0;
            int limit = -1;
            bool hasLimit = false;

            const NodeType* reversedQualifier = qualifiers.find("reversed")->second.get();
            const NodeType* limitQualifier = qualifiers.find("limit")->second.get();
            const NodeType* offsetQualifier = qualifiers.find("offset")->second.get();

            for (size_t i = 1; i < arguments->children.size(); ++i) {
                Node* child = arguments->children[i].get();
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
                const Node& node;
                Variable store;
                const Node& variableNode;
                bool (*iterator)(ForLoopContext& forloopContext);
                long long length;
                string result;
                long long idx = 0;
            };
            auto iterator = +[](ForLoopContext& forLoopContext) {
                Variable forLoopVariable;
                const Context::VariableNode* variableType = forLoopContext.renderer.context.getVariableNodeType();
                auto& resolver = variableType->resolver;

                if (!resolver.getDictionaryVariable(forLoopContext.store, "forloop", forLoopVariable))
                    forLoopVariable = resolver.setDictionaryVariable(forLoopContext.renderer, forLoopContext.store, "forloop", resolver.createHash(forLoopContext.renderer));

                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "index0", resolver.createInteger(forLoopContext.renderer, forLoopContext.idx));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "index", resolver.createInteger(forLoopContext.renderer, forLoopContext.idx+1));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "rindex0", resolver.createInteger(forLoopContext.renderer, forLoopContext.length - forLoopContext.idx));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "rindex", resolver.createInteger(forLoopContext.renderer, forLoopContext.length - (forLoopContext.idx+1)));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "first", resolver.createBool(forLoopContext.renderer, forLoopContext.idx == 0));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "last", resolver.createBool(forLoopContext.renderer, forLoopContext.idx ==  forLoopContext.length-1));
                resolver.setDictionaryVariable(forLoopContext.renderer, forLoopVariable, "length", resolver.createInteger(forLoopContext.renderer, forLoopContext.length));

                forLoopContext.result.append(forLoopContext.renderer.retrieveRenderedNode(*forLoopContext.node.children[1].get(), forLoopContext.store).getString());
                ++forLoopContext.idx;;

                if (forLoopContext.renderer.control != Renderer::Control::NONE)  {
                    if (forLoopContext.renderer.control == Renderer::Control::BREAK) {
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                        return false;
                    } else
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                }
                return true;
            };

            const Context::VariableNode* variableType = renderer.context.getVariableNodeType();
            auto& resolver = variableType->resolver;
            ForLoopContext forLoopContext = { renderer, node, store, *variableNode.get(), iterator };

            forLoopContext.length = result.variant.type == Variant::Type::ARRAY ? result.variant.a.size() : resolver.getArraySize(result.variant.p);
            if (!hasLimit)
                limit = forLoopContext.length;
            else if (limit < 0)
                limit = std::max((int)(limit+forLoopContext.length), 0);
            if (result.variant.type == Variant::Type::ARRAY) {
                int endIndex = std::min(limit+start-1, (int)forLoopContext.length-1);
                if (reversed) {
                    for (int i = endIndex; i >= start; --i) {
                        Variable targetVariable;
                        renderer.inject(targetVariable, result.variant.a[i]);
                        variableType->setVariable(renderer, *variableNode, store, targetVariable);
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                } else {
                    for (int i = start; i <= endIndex; ++i) {
                        Variable targetVariable;
                        renderer.inject(targetVariable, result.variant.a[i]);
                        variableType->setVariable(renderer, *variableNode, store, targetVariable);
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                }
            } else {
                resolver.iterate(result.variant.v, +[](void* variable, void* data) {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    const Context::VariableNode* variableType = forLoopContext.renderer.context.getVariableNodeType();
                    variableType->setVariable(forLoopContext.renderer, forLoopContext.variableNode, forLoopContext.store, variableType->resolver.createClone(forLoopContext.renderer, variable));
                    return forLoopContext.iterator(forLoopContext);
                }, const_cast<void*>((void*)&forLoopContext), start, limit, reversed);
            }
            if (forLoopContext.idx == 0 && node.children.size() >= 4) {
                // Run the else statement if there is one.
                return renderer.retrieveRenderedNode(*node.children[3].get(), store);
            }
            return Node(forLoopContext.result);
        }
    };

    struct CycleNode : TagNodeType {
        CycleNode() : TagNodeType(Composition::FREE, "cycle", 2) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            assert(node.children.size() == 1 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            const LiquidVariableResolver& resolver = renderer.context.getVariableNodeType()->resolver;
            Variable forloopVariable;
            if (resolver.getDictionaryVariable(store, "forloop", forloopVariable)) {
                Variable index0;
                if (resolver.getDictionaryVariable(forloopVariable, "index0", index0)) {
                    long long i;
                    if (resolver.getInteger(index0, &i))
                        return renderer.retrieveRenderedNode(*arguments->children[i % arguments->children.size()].get(), store);
                }
            }
            return Node();
        }
    };


    template <class Function>
    struct ArithmeticOperatorNode : OperatorNodeType {
        ArithmeticOperatorNode(const char* symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Node();
            switch (op1.variant.type) {
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
            return Node();
        }
    };

    struct PlusOperatorNode : ArithmeticOperatorNode<std::plus<>> { PlusOperatorNode() : ArithmeticOperatorNode<std::plus<>>("+", 5) { } };
    struct MinusOperatorNode : ArithmeticOperatorNode<std::minus<>> { MinusOperatorNode() : ArithmeticOperatorNode<std::minus<>>("-", 5) { } };
    struct MultiplyOperatorNode : ArithmeticOperatorNode<std::multiplies<>> { MultiplyOperatorNode() : ArithmeticOperatorNode<std::multiplies<>>("*", 10) { } };
    struct DivideOperatorNode : ArithmeticOperatorNode<std::divides<>> { DivideOperatorNode() : ArithmeticOperatorNode<std::divides<>>("/", 10) { } };
    struct ModuloOperatorNode : OperatorNodeType {
        ModuloOperatorNode() : OperatorNodeType("%", Arity::BINARY, 10) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Node();
            long long divisor = op2.variant.getInt();
            if (divisor == 0)
                return Node();
            return Variant(op1.variant.getInt() % divisor);
        }
    };

    template <class Function>
    struct NumericalComparaisonOperatorNode : OperatorNodeType {
        NumericalComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Node();
            switch (op1.variant.type) {
                return Node();
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
            return Node();
        }
    };

    template <class Function>
    struct QualitativeComparaisonOperatorNode : OperatorNodeType {
        QualitativeComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            if (op1.type || op2.type)
                return Node();
            switch (op1.variant.type) {
                return Node();
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::STRING:
                    if (op2.variant.type != Variant::Type::STRING)
                        return Node(Variant(false));
                    return Node(Variant(operate(op1.variant.s, op2.variant.s)));
                break;
                default:
                break;
            }
            return Node();
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

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (!op1.type || !op1.variant.isTruthy())
                return Node(Variant(false));
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Node(Variant(op2.type && op2.variant.isTruthy()));
        }
    };
    struct OrOperatorNode : OperatorNodeType {
        OrOperatorNode() : OperatorNodeType("or", Arity::BINARY, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (op1.type && op1.variant.isTruthy())
                return Node(Variant(true));
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Node(Variant(op2.type && op2.variant.isTruthy()));
        }
    };

    struct RangeOperatorNode : OperatorNodeType {
        RangeOperatorNode() : OperatorNodeType("..", Arity::BINARY, 10) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            // Requires integers.
            if (op1.type || op2.type || op1.variant.type != Variant::Type::INT || op2.variant.type != Variant::Type::INT)
                return Node();
            auto result = Node(Variant(vector<Variant>()));
            // TODO: This can allocate a lot of memory. Short-circuit that; but should be plugge dinto the allocator.
            long long size = op2.variant.i - op1.variant.i + 1;
            if (size > 10000)
                return Node();
            result.variant.a.reserve(size);
            for (long long i = op1.variant.i; i <= op2.variant.i; ++i)
                result.variant.a.push_back(Variant(i));
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

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            assert(node.children[1]->type && node.children[1]->type->type == NodeType::Type::ARGUMENTS);
            Node op1 = getOperand(renderer, node, store);
            Node op2 = getArgument(renderer, node, store, 0);
            if (op1.type || op2.type)
                return Node();
            switch (op1.variant.type) {
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.f)));
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
            return Node();
        }
    };

    struct PlusFilterNode : ArithmeticFilterNode<std::plus<>> { PlusFilterNode() : ArithmeticFilterNode<std::plus<>>("plus") { } };
    struct MinusFilterNode : ArithmeticFilterNode<std::minus<>> { MinusFilterNode() : ArithmeticFilterNode<std::minus<>>("minus") { } };
    struct MultiplyFilterNode : ArithmeticFilterNode<std::multiplies<>> { MultiplyFilterNode() : ArithmeticFilterNode<std::multiplies<>>("times") { } };
    struct DivideFilterNode : ArithmeticFilterNode<std::divides<>> { DivideFilterNode() : ArithmeticFilterNode<std::divides<>>("divided_by") { } };
    struct ModuloFilterNode : FilterNodeType {
        ModuloFilterNode() : FilterNodeType("modulo", 1, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
            long long divisor = argument.variant.getInt();
            if (divisor == 0)
                return Node();
            return Node(Variant(operand.variant.getInt() % divisor));
        }
    };

    struct AbsFilterNode : FilterNodeType {
        AbsFilterNode() : FilterNodeType("abs", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT:
                        return Node(fabs(operand.variant.f));
                    case Variant::Type::INT:
                        return Node((long long)abs(operand.variant.i));
                    default:
                    break;
                }
            }
            return Node();
        }
    };

    struct CeilFilterNode : FilterNodeType {
        CeilFilterNode() : FilterNodeType("ceil", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT:
                        return Node(ceil(operand.variant.f));
                    case Variant::Type::INT:
                        return Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Node();
        }
    };

    struct FloorFilterNode : FilterNodeType {
        FloorFilterNode() : FilterNodeType("floor", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT:
                        return Node(ceil(operand.variant.f));
                    case Variant::Type::INT:
                        return Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Node();
        }
    };


    struct RoundFilterNode : FilterNodeType {
        RoundFilterNode() : FilterNodeType("round", 0, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT: {
                        if (!argument.type) {
                            int digits = argument.variant.getInt();
                            int multiplier = pow(10, digits);
                            double number = operand.variant.f * multiplier;
                            number = round(number);
                            return Node(number / multiplier);
                        }
                        return Node(round(operand.variant.f));
                    }
                    case Variant::Type::INT:
                        return Node(operand.variant.i);
                    default:
                    break;
                }
            }
            return Node();
        }
    };

    struct AtMostFilterNode : FilterNodeType {
        AtMostFilterNode() : FilterNodeType("at_most", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT:
                        return Node(std::min(operand.variant.f, argument.variant.getFloat()));
                    case Variant::Type::INT:
                        return Node(std::min(operand.variant.i, argument.variant.getInt()));
                    default:
                    break;
                }
            }
            return Node();
        }
    };

    struct AtLeastFilterNode : FilterNodeType {
        AtLeastFilterNode() : FilterNodeType("at_least", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type) {
                switch (operand.variant.type) {
                    case Variant::Type::FLOAT:
                        return Node(std::max(operand.variant.f, argument.variant.getFloat()));
                    case Variant::Type::INT:
                        return Node(std::max(operand.variant.i, argument.variant.getInt()));
                    default:
                    break;
                }
            }
            return Node();
        }
    };

    struct AppendFilterNode : FilterNodeType {
        AppendFilterNode() : FilterNodeType("append", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
            return Variant(operand.getString() + argument.getString());
        }
    };

    struct camelCaseFilterNode : FilterNodeType {
        camelCaseFilterNode() : FilterNodeType("camelcase", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            // TODO
            return Node();
        }
    };

    struct CapitalizeFilterNode : FilterNodeType {
        CapitalizeFilterNode() : FilterNodeType("capitalize", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            str[0] = toupper(str[0]);
            return Node(str);
        }
    };
    struct DowncaseFilterNode : FilterNodeType {
        DowncaseFilterNode() : FilterNodeType("downcase", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
            return Variant(str);
        }
    };
    struct HandleGenericFilterNode : FilterNodeType {
        HandleGenericFilterNode(const string& symbol) : FilterNodeType(symbol, 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
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
            return Variant(accumulator);
        }
    };
    struct HandleFilterNode : HandleGenericFilterNode { HandleFilterNode() : HandleGenericFilterNode("handle") { } };
    struct HandleizeFilterNode : HandleGenericFilterNode { HandleizeFilterNode() : HandleGenericFilterNode("handleize") { } };

    struct PluralizeFilterNode : FilterNodeType {
        PluralizeFilterNode() : FilterNodeType("pluralize", 2, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument1 = getArgument(renderer, node, store, 1);
            auto argument2 = getArgument(renderer, node, store, 2);
            if (operand.type)
                return Node();
            return Variant(operand.variant.getInt() > 1 ? argument2.getString() : argument1.getString());
        }
    };

    struct PrependFilterNode : FilterNodeType {
        PrependFilterNode() : FilterNodeType("prepend", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
            return Variant(argument.getString() + operand.getString());
        }
    };
    struct RemoveFilterNode : FilterNodeType {
        RemoveFilterNode() : FilterNodeType("remove", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
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
            return Variant(accumulator);
        }
    };
    struct RemoveFirstFilterNode : FilterNodeType {
        RemoveFirstFilterNode() : FilterNodeType("removefirst", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
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
            return Variant(accumulator);
        }
    };
    struct ReplaceFilterNode : FilterNodeType {
        ReplaceFilterNode() : FilterNodeType("replace", 2, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
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
            return Variant(accumulator);
        }
    };
    struct ReplaceFirstFilterNode : FilterNodeType {
        ReplaceFirstFilterNode() : FilterNodeType("replacefirst", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
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
            return Variant(accumulator);
        }
    };
    struct SliceFilterNode : FilterNodeType {
        SliceFilterNode() : FilterNodeType("slice", 1, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            auto arg2 = getArgument(renderer, node, store, 0);

            if (operand.type || arg1.type || arg2.type || operand.variant.type != Variant::Type::STRING)
                return Node();
            string str = operand.getString();
            long long offset = std::max(std::min(arg1.variant.getInt(), (long long)str.size()), 0LL);
            long long size = std::min(arg2.variant.getInt(), (long long)(str.size() - offset));
            return Variant(str.substr(offset, size));
        }
    };
    struct SplitFilterNode : FilterNodeType {
        SplitFilterNode() : FilterNodeType("split", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
            Variant result({ });
            string str = operand.getString();
            string splitter = argument.getString();
            size_t start = 0, idx;
            while ((idx = str.find(splitter, start)) != string::npos) {
                if (idx > start)
                    result.a.push_back(Variant(str.substr(start, idx - start)));
                start = idx + splitter.size();
            }
            result.a.push_back(str.substr(start, str.size() - start));
            return Variant(std::move(result));
        }
    };
    struct StripFilterNode : FilterNodeType {
        StripFilterNode() : FilterNodeType("strip", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            size_t start, end;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Node(str.substr(start, end - start + 1));
        }
    };
    struct LStripFilterNode : FilterNodeType {
        LStripFilterNode() : FilterNodeType("lstrip", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            size_t start;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            return Node(str.substr(start, str.size() - start));
        }
    };
    struct RStripFilterNode : FilterNodeType {
        RStripFilterNode() : FilterNodeType("rstrip", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            size_t end;
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Node(str.substr(0, end + 1));
        }
    };
    struct StripNewlinesFilterNode : FilterNodeType {
        StripNewlinesFilterNode() : FilterNodeType("strip_newlines", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            string str = operand.getString();
            string accumulator;
            accumulator.reserve(str.size());
            for (auto it = str.begin(); it != str.end(); ++it) {
                if (*it != '\n' && *it != '\r')
                    accumulator.push_back(*it);
            }
            return Node(accumulator);
        }
    };
    struct TruncateFilterNode : FilterNodeType {
        TruncateFilterNode() : FilterNodeType("truncate", 1, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto characterCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
            if (operand.type || characterCount.type || customEllipsis.type)
                return Node();
            string ellipsis = "...";
            if (customEllipsis.variant.type == Variant::Type::STRING)
                ellipsis = customEllipsis.getString();
            long long count = characterCount.variant.getInt();
            if (count > (long long)ellipsis.size()) {
                string str = operand.getString();
                return Variant(str.substr(0, std::min((long long)(count - ellipsis.size()), (long long)str.size())) + ellipsis);
            }
            return Variant(ellipsis.substr(0, std::min(count, (long long)ellipsis.size())));
        }
    };
    struct TruncateWordsFilterNode : FilterNodeType {
        TruncateWordsFilterNode() : FilterNodeType("truncatewords", 1, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto wordCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
            if (operand.type || wordCount.type || customEllipsis.type)
                return Node();
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
            return Variant(str.substr(0, i-1));
        }
    };
    struct UpcaseFilterNode : FilterNodeType {
        UpcaseFilterNode() : FilterNodeType("upcase", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::toupper(c); });
            return Variant(str);
        }
    };

    struct ArrayFilterNodeType : FilterNodeType {
        ArrayFilterNodeType(const std::string& symbol, int minArguments = -1, int maxArguments = -1) : FilterNodeType(symbol, minArguments, maxArguments) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type || argument.type)
                return Node();

            return Node();
        }
    };

    struct JoinFilterNode : ArrayFilterNodeType {
        JoinFilterNode() : ArrayFilterNodeType("join", 0, 1) { }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) {
            struct JoinStruct {
                const Context& context;
                string accumulator;
                string joiner = "";
                int idx = 0;
            };
            const LiquidVariableResolver& resolver = renderer.context.getVariableResolver();
            JoinStruct joinStruct({ renderer.context });
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                joinStruct.joiner = argument.getString();


            resolver.iterate(operand, +[](void* variable, void* data) {
                JoinStruct& joinStruct = *(JoinStruct*)data;
                string part;
                if (joinStruct.idx++ > 0)
                    joinStruct.accumulator.append(joinStruct.joiner);
                if (joinStruct.context.resolveVariableString(part, variable))
                    joinStruct.accumulator.append(part);
                return true;
            }, &joinStruct, 0, -1, false);
            return Node(joinStruct.accumulator);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, Variant operand) {
            string accumulator;
            if (operand.type != Variant::Type::ARRAY || operand.a.size() == 0)
                return Node();
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

        void accumulate(Renderer& renderer, Variant& accumulator, Variable v) const {
            renderer.context.getVariableResolver().iterate(v, +[](void* variable, void* data) {
                    static_cast<Variant*>(data)->a.push_back(Variant(static_cast<Variable*>(variable)));
                return true;
            }, &accumulator, 0, -1, false);
        }

        void accumulate(Renderer& renderer, Variant& accumulator, const Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it)
                accumulator.a.push_back(*it);
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Node();
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    accumulate(renderer, accumulator, operand.variant.v);
                break;
                case Variant::Type::ARRAY:
                    accumulate(renderer, accumulator, operand.variant.a);
                break;
                default:
                    return Node();
            }
            if (argument.type)
                return accumulator;
            switch (argument.variant.type) {
                case Variant::Type::VARIABLE:
                    accumulate(renderer, accumulator, argument.variant.v);
                break;
                case Variant::Type::ARRAY:
                    accumulate(renderer, accumulator, argument.variant.a);
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
            const LiquidVariableResolver& resolver;
            string property;
            Variant accumulator;
        };

        void accumulate(Renderer& renderer, MapStruct& mapStruct, Variable v) const {
            mapStruct.resolver.iterate(v, +[](void* variable, void* data) {
                MapStruct& mapStruct = *static_cast<MapStruct*>(data);
                Variable target;
                if (mapStruct.resolver.getDictionaryVariable(variable, mapStruct.property.data(), target))
                    mapStruct.accumulator.a.push_back(target);
                else
                    mapStruct.accumulator.a.push_back(Variant());
                return true;
            }, &mapStruct, 0, -1, false);
        }

        void accumulate(Renderer& renderer, MapStruct& mapStruct, const Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (it->type == Variant::Type::VARIABLE) {
                    Variable target;
                    if (mapStruct.resolver.getDictionaryVariable(const_cast<Variable&>(it->v), mapStruct.property.data(), target))
                        mapStruct.accumulator.a.push_back(target);
                    else
                        mapStruct.accumulator.a.push_back(Variant());
                } else {
                    mapStruct.accumulator.a.push_back(Variant());
                }
            }
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Node();
            MapStruct mapStruct = { renderer.context.getVariableResolver(), argument.getString() };
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    accumulate(renderer, mapStruct, operand.variant.v);
                break;
                case Variant::Type::ARRAY:
                    accumulate(renderer, mapStruct, operand.variant.a);
                break;
                default:
                    return Node();
            }
            return mapStruct.accumulator;
        }
    };

    struct ReverseFilterNode : ArrayFilterNodeType {
        ReverseFilterNode() : ArrayFilterNodeType("reverse", 0, 0) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();
            auto& v = operand.variant;
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    renderer.context.getVariableResolver().iterate(v.v, +[](void* variable, void* data) {
                        static_cast<Variant*>(data)->a.push_back(Variable({variable}));
                        return true;
                    }, &accumulator, 0, -1, true);
                break;
                case Variant::Type::ARRAY:
                    for (auto it = v.a.rbegin(); it != v.a.rend(); ++it)
                        accumulator.a.push_back(*it);
                break;
                default:
                    return Node();
            }
            std::reverse(accumulator.a.begin(), accumulator.a.end());
            return accumulator;
        }
    };

    struct SortFilterNode : ArrayFilterNodeType {
        SortFilterNode() : ArrayFilterNodeType("sort", 0, 1) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            Variant accumulator;
            string property;
            const LiquidVariableResolver& resolver = renderer.context.getVariableResolver();
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Node();
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE: {
                    resolver.iterate(operand.variant.v, +[](void* variable, void* data) {
                        static_cast<Variant*>(data)->a.push_back(Variable({variable}));
                        return true;
                    }, &accumulator, 0, -1, false);
                } break;
                case Variant::Type::ARRAY: {
                    auto& v = operand.variant;
                    for (auto it = v.a.begin(); it != v.a.end(); ++it)
                        accumulator.a.push_back(*it);
                } break;
                default:
                    return Node();
            }

            if (!argument.type && argument.variant.type == Variant::Type::STRING) {
                property = argument.getString();
                std::sort(accumulator.a.begin(), accumulator.a.end(), [&property, &resolver](Variant& a, Variant& b) -> bool {
                    if (a.type != Variant::Type::VARIABLE || b.type != Variant::Type::VARIABLE)
                        return false;
                    Variable targetA, targetB;
                    if (!resolver.getDictionaryVariable(a.v, property.data(), targetA))
                        return false;
                    if (!resolver.getDictionaryVariable(b.v, property.data(), targetB))
                        return false;
                    return resolver.compare(targetA, targetB) < 0;
                });
            } else {
                std::sort(accumulator.a.begin(), accumulator.a.end(), [](Variant& a, Variant& b) -> bool { return a < b; });
            }
            return accumulator;
        }
    };


    struct WhereFilterNode : ArrayFilterNodeType {
        WhereFilterNode() : ArrayFilterNodeType("where", 1, 2) { }

        struct WhereStruct {
            const LiquidVariableResolver& resolver;
            string property;
            Variant value;
            Variant accumulator;
        };

        void accumulate(WhereStruct& whereStruct, Variable v) const {
            whereStruct.resolver.iterate(v, +[](void* variable, void* data) {
                WhereStruct& whereStruct = *static_cast<WhereStruct*>(data);
                Variable target;
                if (whereStruct.resolver.getDictionaryVariable(variable, whereStruct.property.data(), target)) {
                    if (whereStruct.property.empty()) {
                        if (whereStruct.resolver.getTruthy(target))
                            whereStruct.accumulator.a.push_back(variable);
                    } else {
                        // std::string str;
                        //if (whereStruct.resolver.getString(str)) {
                            // TODO.
                            // if (str == whereStruct.value)
                            //    whereStruct.accumulator.a.push_back(variable);
                        //}
                    }
                }
                return true;
            }, &whereStruct, 0, -1, false);
        }

        void accumulate(WhereStruct& whereStruct, const Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (it->type == Variant::Type::VARIABLE) {
                    Variable target;
                    if (whereStruct.resolver.getDictionaryVariable(const_cast<Variable&>(it->v), whereStruct.property.data(), target)) {
                        if (whereStruct.property.empty()) {
                            if (whereStruct.resolver.getTruthy(target))
                                whereStruct.accumulator.a.push_back(*it);
                        } else {
                            //std::string str;
                            //if (target->getString(str)) {
                                // TODO
                                //if (str == whereStruct.value)
                                //    whereStruct.accumulator.a.push_back(*it);
                            //}
                        }
                    }
                }
            }
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Node();
            auto arg2 = getArgument(renderer, node, store, 1);
            if (arg2.type)
                return Node();
            WhereStruct whereStruct = { renderer.context.getVariableResolver(), arg1.getString(), arg2.variant };
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    accumulate(whereStruct, operand.variant.v);
                break;
                case Variant::Type::ARRAY:
                    accumulate(whereStruct, operand.variant.a);
                break;
                default:
                    return Node();
            }
            return whereStruct.accumulator;
        }
    };

    struct UniqFilterNode : ArrayFilterNodeType {
        UniqFilterNode() : ArrayFilterNodeType("uniq", 0, 0) { }

        struct UniqStruct {
            Renderer& renderer;
            const LiquidVariableResolver& resolver;
            std::unordered_set<size_t> hashes;
            Variant accumulator;
        };

        void accumulate(UniqStruct& uniqStruct, Variable v) const {
            uniqStruct.resolver.iterate(v, +[](void* variable, void* data) {
                UniqStruct& uniqStruct = *static_cast<UniqStruct*>(data);
                Variant v = uniqStruct.renderer.parseVariant(Variable({variable}));
                if (!uniqStruct.hashes.emplace(v.hash()).second)
                    uniqStruct.accumulator.a.push_back(variable);
                return true;
            }, &uniqStruct, 0, -1, false);
        }

        void accumulate(UniqStruct& uniqStruct, const Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it) {
                if (!uniqStruct.hashes.emplace(it->hash()).second)
                    uniqStruct.accumulator.a.push_back(v);
            }
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            if (operand.type)
                return Node();
            auto arg2 = getArgument(renderer, node, store, 1);
            if (arg2.type)
                return Node();
            UniqStruct uniqStruct = { renderer, renderer.context.getVariableResolver() };
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    accumulate(uniqStruct, operand.variant.v);
                break;
                case Variant::Type::ARRAY:
                    accumulate(uniqStruct, operand.variant.a);
                break;
                default:
                    return Node();
            }
            return uniqStruct.accumulator;
        }
    };



    struct FirstFilterNode : ArrayFilterNodeType {
        FirstFilterNode() : ArrayFilterNodeType("first", 0, 0) { }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            Variable v;
            if (!renderer.context.getVariableResolver().getArrayVariable(operand, 0, v))
                return Node();
            return Node(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant operand) const {
            if (operand.type != Variant::Type::ARRAY || operand.a.size() == 0)
                return Node();
            return operand.a[0];
        }
    };

    struct LastFilterNode : ArrayFilterNodeType {
        LastFilterNode() : ArrayFilterNodeType("last", 0, 0) { }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            Variable v;
            const LiquidVariableResolver& resolver = renderer.context.getVariableResolver();
            long long size = resolver.getArraySize(operand);
            if (size == -1 || !resolver.getArrayVariable(operand, size - 1, v))
                return Node();
            return Node(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant operand) const {
            if (operand.type != Variant::Type::ARRAY || operand.a.size() == 0)
                return Node();
            return operand.a[operand.a.size()-1];
        }
    };

    struct IndexFilterNode : ArrayFilterNodeType {
        IndexFilterNode() : ArrayFilterNodeType("index", 1, 1) { }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            Variable v;
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                return Node();
            int idx = argument.variant.i;
            if (!renderer.context.getVariableResolver().getArrayVariable(operand, idx, v))
                return Node();
            return Node(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                return Node();
            long long idx = argument.variant.i;
            if (operand.type != Variant::Type::ARRAY || idx >= (long long)operand.a.size())
                return Node();
            return operand.a[idx];
        }
    };
    struct SizeFilterNode : FilterNodeType {
        SizeFilterNode() : FilterNodeType("size", 0, 0) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();

            switch (operand.variant.type) {
                case Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, operand.variant.v);
                case Variant::Type::STRING:
                    return Variant((long long)operand.variant.s.size());
                default:
                    return Node();
            }
        }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            long long size = renderer.context.getVariableResolver().getArraySize(operand);
            return size != -1 ? Node(size) : Node();
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
            if (operand.type != Variant::Type::ARRAY)
                return Node();
            return Node((long long)operand.a.size());
        }
    };


    struct SizeDotFilterNode : DotFilterNodeType {
        SizeDotFilterNode() : DotFilterNodeType("size") { }

        Node render(Renderer& renderer, const Node& node, Variable store) const {
            auto operand = getOperand(renderer, node, store);
            if (operand.type)
                return Node();

            switch (operand.variant.type) {
                case Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, operand.variant.v);
                case Variant::Type::STRING:
                    return Variant((long long)operand.variant.s.size());
                default:
                    return Node();
            }
        }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            long long size = renderer.context.getVariableResolver().getArraySize(operand);
            return size != -1 ? Node(size) : Node();
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
            if (operand.type != Variant::Type::ARRAY)
                return Node();
            return Node((long long)operand.a.size());
        }
    };



    struct DefaultFilterNode : FilterNodeType {
        DefaultFilterNode() : FilterNodeType("default", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const {
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

        // Other tags.
        context.registerType<CommentNode>();

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
        context.registerType<SizeDotFilterNode>();
        context.registerType<SortFilterNode>();
        context.registerType<WhereFilterNode>();
        context.registerType<UniqFilterNode>();

        // Other filters.
        context.registerType<DefaultFilterNode>();
    }
}
