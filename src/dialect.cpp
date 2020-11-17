#include "context.h"
#include "dialect.h"


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

        ForNode() : TagNodeType(Composition::ENCLOSED, "for") {
            intermediates["else"] = make_unique<ElseNode>();
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

            if (result.type != nullptr || (result.variant.type != Parser::Variant::Type::VARIABLE && result.variant.type != Parser::Variant::Type::ARRAY))
                return Parser::Node();

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
                if (forLoopContext.renderer.control != Renderer::Control::NONE)  {
                    if (forLoopContext.renderer.control == Renderer::Control::BREAK) {
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                        return false;
                    } else
                        forLoopContext.renderer.control = Renderer::Control::NONE;
                }
                ++forLoopContext.idx;
                return true;
            };

            ForLoopContext forLoopContext = { renderer, node, store, *targetVariable, iterator };
            if (result.variant.type == Parser::Variant::Type::ARRAY) {
                forLoopContext.length = result.variant.a.size();
                for (auto it = result.variant.a.begin(); it != result.variant.a.end(); ++it) {
                    forLoopContext.targetVariable.clear();
                    it->inject(forLoopContext.targetVariable);
                    if (!forLoopContext.iterator(forLoopContext))
                        break;
                }
            } else {
                forLoopContext.length = static_cast<Variable*>(result.variant.p)->getArraySize();
                static_cast<Variable*>(result.variant.p)->iterate(+[](Variable* variable, void* data) {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    forLoopContext.targetVariable.assign(*variable);
                    return forLoopContext.iterator(forLoopContext);
                }, const_cast<void*>((void*)&forLoopContext));
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

    struct JoinFilterNode : FilterNodeType {
        JoinFilterNode() : FilterNodeType("join", 1, 1) { }

        Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
            assert(node.children[1]->type && node.children[1]->type->type == NodeType::Type::ARGUMENTS);
            Parser::Node operand = getOperand(renderer, node, store);
            if (operand.type || operand.variant.type != Parser::Variant::Type::VARIABLE) {

            }
            Parser::Node arg1 = getArgument(renderer, node, store, 0);
        }
    };

    void StandardDialect::implement(Context& context) {
        context.registerType<IfNode>();
        context.registerType<UnlessNode>();
        context.registerType<CaseNode>();
        context.registerType<ForNode>();
        context.registerType<CycleNode>();
        context.registerType<ForNode::InOperatorNode>();
        context.registerType<ForNode::BreakNode>();
        context.registerType<ForNode::ContinueNode>();
        context.registerType<AssignNode>();
        context.registerType<CaptureNode>();
        context.registerType<IncrementNode>();
        context.registerType<DecrementNode>();

        context.registerType<AssignOperator>();
        context.registerType<PlusOperatorNode>();
        context.registerType<MinusOperatorNode>();
        context.registerType<MultiplyOperatorNode>();
        context.registerType<DivideOperatorNode>();

        context.registerType<LessThanOperatorNode>();
        context.registerType<LessThanEqualOperatorNode>();
        context.registerType<GreaterThanOperatorNode>();
        context.registerType<GreaterThanEqualOperatorNode>();

        context.registerType<EqualOperatorNode>();
        context.registerType<NotEqualOperatorNode>();

        context.registerType<AndOperatorNode>();
        context.registerType<OrOperatorNode>();

        context.registerType<RangeOperatorNode>();

        context.registerType<PlusFilterNode>();
        context.registerType<MinusFilterNode>();
        context.registerType<MultiplyFilterNode>();
        context.registerType<DivideFilterNode>();

    }
}
