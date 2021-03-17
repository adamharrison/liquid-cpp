#include "context.h"
#include "dialect.h"
#include "parser.h"
#include "optimizer.h"
#include "compiler.h"
#include <cmath>
#include <ctime>
#include <algorithm>
#include <unordered_set>

namespace Liquid {

    template <bool allowGlobals>
    struct AssignNode : TagNodeType {

        struct AssignOperator : OperatorNodeType { AssignOperator() : OperatorNodeType("=", Arity::BINARY, 0) { } };

        AssignNode() : TagNodeType(Composition::FREE, "assign", 1, 1, LIQUID_OPTIMIZATION_SCHEME_NONE) {
            registerType<AssignOperator>();
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto& argumentNode = node.children.front();
            auto& assignmentNode = argumentNode->children.front();
            auto& variableNode = assignmentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable;
                auto& operandNode = assignmentNode->children.back();
                Node node = renderer.retrieveRenderedNode(*operandNode.get(), store);
                assert(!node.type);
                renderer.inject(targetVariable, node.variant);
                renderer.setVariable(*variableNode.get(), store, targetVariable);
            }
            return Node();
        }

        bool validate(Parser& parser, const Node& node) const override {
            auto& argumentNode = node.children.front();
            auto& assignmentNode = argumentNode->children.front();
            if (assignmentNode->type != parser.context.getBinaryOperatorType("=")) {
                parser.pushError(Parser::Error(parser.lexer, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS, assignmentNode->getString()));
                return false;
            }
            auto& variableNode = assignmentNode->children.front();
            if (variableNode->type != parser.context.getVariableNodeType()) {
                parser.pushError(Parser::Error(parser.lexer, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS, variableNode->getString()));
                return false;
            }
            if (!allowGlobals && variableNode->children.size() > 1) {
                parser.pushError(Parser::Error(parser.lexer, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS, variableNode->getString()));
                return false;
            }
            return true;
        }
    };


    struct CaptureNode : TagNodeType {
        CaptureNode() : TagNodeType(Composition::ENCLOSED, "capture", 1, 1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable = renderer.variableResolver.createString(renderer, renderer.retrieveRenderedNode(*node.children[1].get(), store).getString().data());
                renderer.setVariable(*variableNode.get(), store, targetVariable);
            }
            return Node();
        }
    };

    struct IncrementNode : TagNodeType {
        IncrementNode() : TagNodeType(Composition::FREE, "increment", 1, 1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable = renderer.getVariable(*variableNode.get(), store);
                if (targetVariable.exists()) {
                    long long i = -1;
                    if (renderer.variableResolver.getInteger(renderer, targetVariable,&i))
                        renderer.setVariable(*variableNode.get(), targetVariable, renderer.variableResolver.createInteger(renderer, i+1));
                }
            }
            return Node();
        }
    };

     struct DecrementNode : TagNodeType {
        DecrementNode() : TagNodeType(Composition::FREE, "decrement", 1, 1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto& argumentNode = node.children.front();
            auto& variableNode = argumentNode->children.front();
            if (variableNode->type->type == NodeType::VARIABLE) {
                Variable targetVariable = renderer.getVariable(*variableNode.get(), store);
                if (targetVariable.exists()) {
                    long long i = -1;
                    if (renderer.variableResolver.getInteger(renderer, targetVariable,&i))
                        renderer.setVariable(*variableNode.get(), targetVariable, renderer.variableResolver.createInteger(renderer, i-1));
                }
            }
            return Node();
        }
    };

    struct CommentNode : EnclosedNodeType {
        CommentNode() : EnclosedNodeType("comment", 0, 0, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
    };

    template <bool INVERSE>
    struct BranchNode : TagNodeType {
        static Node internalRender(Renderer& renderer, const Node& node, Variable store) {
            Node result = static_cast<const BranchNode*>(node.type)->getArgument(renderer, node, store, 0);
            bool truthy = result.variant.isTruthy(renderer.context.falsiness);
            if (INVERSE)
                truthy = !truthy;
            if (truthy)
                return renderer.retrieveRenderedNode(*node.children[1].get(), store);
            else {
                // Loop through the elsifs and elses, and anything that's true, run the next concatenation.
                for (size_t i = 2; i < node.children.size()-1; i += 2) {
                    auto conditionalResult = renderer.retrieveRenderedNode(*node.children[i].get(), store);
                    if (!conditionalResult.type && conditionalResult.variant.isTruthy(renderer.context.falsiness)) {
                        return renderer.retrieveRenderedNode(*node.children[i+1].get(), store);
                    }
                }
                return Node();
            }
        }

        static bool internalOptimize(Optimizer& optimizer, Node& node, Variable store) {
            auto& arguments = node.children.front();
            if (arguments->children.front().get()->type)
                return false;
            bool truthy = arguments->children.front().get()->variant.isTruthy(optimizer.renderer.context.falsiness);
            if (INVERSE)
                truthy = !truthy;
            if (truthy) {
                unique_ptr<Node> ptr = move(node.children[1]);
                node = move(*ptr.get());
                return true;
            }
            // Loop through the elsifs and elses, and anything that's true, run the next concatenation.
            // If it's false, prune it from the list.
            size_t target = 0;
            for (size_t i = 2; i < node.children.size()-2; i += 2) {
                Node& argumentNode = *node.children[i]->children[0]->children[0].get();
                if (!argumentNode.type) {
                    bool truthy = argumentNode.variant.isTruthy(optimizer.renderer.context.falsiness);
                    if (INVERSE)
                        truthy = !truthy;
                    if (truthy) {
                        unique_ptr<Node> ptr = move(node.children[i+1]);
                        node = move(*ptr.get());
                        return true;
                    }
                } else {
                    if (target == 0)
                        node.children[target] = move(node.children[i]->children[0]);
                    else
                        node.children[target] = move(node.children[i]);
                    node.children[target+1] = move(node.children[i+1]);
                    target += 2;
                }
            }
            const TagNodeType* nodeType = static_cast<const TagNodeType*>(node.type);
            auto it = nodeType->intermediates.find("else");
            if (target == 0)
                node = node.children[node.children.size()-2].get()->type == it->second.get() ? move(*node.children[node.children.size()-1].get()) : Node();
            else
                node.children.resize(target + (node.children.size() % 2));
            return true;
        }

        static void internalCompile(Compiler& compiler, const Node& node) {
            vector<int> endJumps;
            for (size_t i = 0; i < node.children.size(); i += 2) {
                if (node.children[i]->type->symbol == "else") {
                    compiler.compileBranch(*node.children[i+1].get());
                } else {
                    compiler.compileBranch(*node.children[i].get()->children[0].get());
                    int conditionalFalseJump = compiler.add(OP_JMPFALSE, 0x0, 0x0);
                    compiler.compileBranch(*node.children[i+1].get());
                    endJumps.push_back(compiler.add(OP_JMP, 0x0, 0x0));
                    compiler.modify(conditionalFalseJump, OP_JMPFALSE, 0x0, compiler.currentOffset());
                }
            }
            for (int jmp : endJumps)
                compiler.modify(jmp, OP_JMP, 0x0, compiler.currentOffset());

        }


        struct ElsifNode : TagNodeType {
            ElsifNode() : TagNodeType(Composition::FREE, "elsif", 1, 1) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                auto& arguments = node.children.front();
                return renderer.retrieveRenderedNode(*arguments->children.front().get(), store);
            }
            void compile(Compiler& compiler, const Node& node) const override { }
            bool optimize(Optimizer& optimizer, Node& node, Variable store) const override {
                return true;
            }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                return Node(true);
            }
            void compile(Compiler& compiler, const Node& node) const override { }
            bool optimize(Optimizer& optimizer, Node& node, Variable store) const override {
                return true;
            }
        };

        BranchNode(const std::string symbol) : TagNodeType(Composition::ENCLOSED, symbol, 1, 1, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) {
            intermediates["elsif"] = make_unique<ElsifNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }


        bool validate(Parser& parser, const Node& node) const override {
            /*auto elseNode = intermediates.find("else")->second.get();
            auto elsifNode = intermediates.find("elsif")->second.get();
            for (size_t i = 1; i < node.children.size(); ++i) {
                if (((i - 1) % 2) == 0) {
                    if (node.children[i]->type != elseNode && node.children[i]->type != elsifNode)
                        return Parser::Error(Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL);
                }
            }*/
            return true;
        }


        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            return BranchNode<INVERSE>::internalRender(renderer, node, store);
        }

        bool optimize(Optimizer& optimizer, Node& node, Variable store) const override {
            return BranchNode<INVERSE>::internalOptimize(optimizer, node, store);
        }

        void compile(Compiler& compiler, const Node& node) const override {
            return BranchNode<INVERSE>::internalCompile(compiler, node);
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

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                return renderer.retrieveRenderedNode(*node.children[0]->children[0].get(), store);
            }
        };
        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };

        CaseNode() : TagNodeType(Composition::ENCLOSED, "case", 1, 1, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) {
            intermediates["when"] = make_unique<WhenNode>();
            intermediates["else"] = make_unique<ElseNode>();
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
            InOperatorNode() :  OperatorNodeType("in", Arity::BINARY, 0, Fixness::INFIX, LIQUID_OPTIMIZATION_SCHEME_SHIELD) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };

        struct ElseNode : TagNodeType {
            ElseNode() : TagNodeType(Composition::FREE, "else", 0, 0, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };

        struct BreakNode : TagNodeType {
            BreakNode() : TagNodeType(Composition::FREE, "break", 0, 0, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                renderer.control = Renderer::Control::BREAK;
                return Node();
            }
        };

        struct ContinueNode : TagNodeType {
            ContinueNode() : TagNodeType(Composition::FREE, "continue", 0, 0, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override {
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

        struct ForLoopContext {
            Renderer& renderer;
            const Node& node;
            Variable store;
            void* variable;
            bool (*iterator)(ForLoopContext& forloopContext);
            long long length;
            string result;
            long long idx;
        };

        struct CycleNode : TagNodeType {
            CycleNode() : TagNodeType(Composition::FREE, "cycle", 1, -1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                assert(node.children.size() == 1 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
                auto& arguments = node.children.front();
                pair<void*, Renderer::DropFunction> internalDrop = renderer.getInternalDrop("forloop");

                if (internalDrop.second)
                    return renderer.retrieveRenderedNode(*arguments->children[static_cast<ForLoopContext*>(internalDrop.first)->idx % arguments->children.size()].get(), store);
                return Node();
            }
        };

        const NodeType* reversedQualifier;
        const NodeType* limitQualifier;
        const NodeType* offsetQualifier;

        ForNode() : TagNodeType(Composition::ENCLOSED, "for", 1, -1) {
            registerType<ElseNode>();
            reversedQualifier = registerType<ReverseQualifierNode>();
            limitQualifier = registerType<LimitQualifierNode>();
            offsetQualifier = registerType<OffsetQualifierNode>();
        }



        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            assert(node.children.size() >= 2 && node.children.front()->type->type == NodeType::Type::ARGUMENTS);
            auto& arguments = node.children.front();
            assert(arguments->children.size() >= 1);
            // Should be move to validation.
            assert(arguments->children[0]->type && arguments->children[0]->type->type == NodeType::Type::OPERATOR && arguments->children[0]->type->symbol == "in");
            assert(arguments->children[0]->children[0]->type && arguments->children[0]->children[0]->type->type == NodeType::Type::VARIABLE);
            // Should be deleted eventually, as they're handled by the parser.
            assert(arguments->children[0]->children.size() == 2);

            auto& variableNode = arguments->children[0]->children[0];
            // Can only ask for single top-level variables. Nothing nested.
            if (variableNode->children.size() != 1)
                return Node();
            string variableName = variableNode->children[0]->getString();

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


            auto iterator = +[](ForLoopContext& forLoopContext) {
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

            auto& resolver = renderer.variableResolver;
            ForLoopContext forLoopContext = { renderer, node, store, nullptr, iterator,  0, "", 0 };


            forLoopContext.length = result.variant.type == Variant::Type::ARRAY ? result.variant.a.size() : resolver.getArraySize(renderer, result.variant.p);

            if (!hasLimit)
                limit = forLoopContext.length;
            else if (limit < 0)
                limit = std::max((int)(limit+forLoopContext.length), 0);

            forLoopContext.idx = start;

            renderer.pushInternalDrop("forloop", { &forLoopContext, +[](Renderer& renderer, const Node& node, Variable store, void* data)->Node {
                ForLoopContext* forLoopContext = (ForLoopContext*)data;
                string property;
                if (node.type) {
                    if (node.children.size() == 2)
                        property = renderer.retrieveRenderedNode(*node.children[1].get(), store).getString();
                } else {
                    property = node.getString();
                }
                if (!property.empty()) {
                    if (property == "index0")
                        return Variant(forLoopContext->idx);
                    if (property == "index")
                        return Variant(forLoopContext->idx+1);
                    if (property == "rindex")
                        return Variant(forLoopContext->length - (forLoopContext->idx+1));
                    if (property == "index0")
                        return Variant(forLoopContext->length - forLoopContext->idx);
                    if (property == "first")
                        return Variant(forLoopContext->idx == 0);
                    if (property == "last")
                        return Variant(forLoopContext->idx == forLoopContext->length-1);
                    if (property == "length")
                        return Variant(forLoopContext->length);
                }
                return Node();
            } });
            if (result.variant.type == Variant::Type::ARRAY) {
                renderer.pushInternalDrop(variableName, { &forLoopContext, +[](Renderer& renderer, const Node& node, Variable store, void* data)->Node {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    return Variant(*(Variant*)forLoopContext.variable);
                } });
                int endIndex = std::min(limit+start-1, (int)forLoopContext.length-1);
                if (reversed) {
                    for (int i = endIndex; i >= start; --i) {
                        forLoopContext.variable = &result.variant.a[i];
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                } else {
                    for (int i = start; i <= endIndex; ++i) {
                        forLoopContext.variable = &result.variant.a[i];
                        if (!forLoopContext.iterator(forLoopContext))
                            break;
                    }
                }
            } else {
                renderer.pushInternalDrop(variableName, { &forLoopContext, +[](Renderer& renderer, const Node& node, Variable store, void* data)->Node {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    return Variant(renderer.getVariable(node, Variable(forLoopContext.variable), 1));
                } });
                resolver.iterate(renderer, result.variant.v, +[](void* variable, void* data) {
                    ForLoopContext& forLoopContext = *static_cast<ForLoopContext*>(data);
                    forLoopContext.variable = variable;
                    return forLoopContext.iterator(forLoopContext);
                }, const_cast<void*>((void*)&forLoopContext), start, limit, reversed);
            }
            renderer.popInternalDrop("forloop");
            renderer.popInternalDrop(variableName);
            if (forLoopContext.idx == 0 && node.children.size() >= 4) {
                // Run the else statement if there is one.
                return renderer.retrieveRenderedNode(*node.children[3].get(), store);
            }
            return Node(forLoopContext.result);
        }


        void compile(Compiler& compiler, const Node& node) const override {
            // Create loop index variable, (and reference to current variable in future), on the stack.
            const Node& group = *node.children[0].get()->children[0]->children[1].get();
            const Node& sequence = group.type && group.type->type == NodeType::Type::GROUP ? *group.children[0].get() : group;

            if (sequence.type && sequence.type->type == NodeType::Type::VARIABLE) {
                compiler.compileBranch(sequence);
                compiler.addPush(0x0);
                compiler.add(OP_MOVNIL, 0x0);
                compiler.addPush(0x0);
                compiler.add(OP_MOVINT, 0x0, 0);
                compiler.addPush(0x0);
                compiler.freeRegister = 0;
                const Node& variable = *node.children[0].get()->children[0]->children[0].get();
                compiler.addDropFrame(variable.children[0].get()->variant.s, +[](Compiler& compiler, Compiler::DropFrameState& state) {
                    int negativeOffset = state.stackPoint - compiler.stackSize;
                    compiler.add(OP_STACK, 0x0, negativeOffset - 2);
                    return 0;
                });
                // Counter
                int topLoop = compiler.add(OP_STACK, 0x1, -1);
                // Variable Context
                compiler.add(OP_STACK, 0x2, -3);
                int iterationInstruction = compiler.add(OP_ITERATE, 0x2, 0x0);
                compiler.add(OP_POP, 0x0, 0x2);
                compiler.add(OP_PUSH, 0x0);
                compiler.add(OP_INC, 0x1);
                compiler.add(OP_PUSH, 0x1);
                compiler.compileBranch(*node.children[1].get());
                compiler.add(OP_JMP, 0x0, topLoop);
                compiler.modify(iterationInstruction, OP_ITERATE, 0x2, compiler.currentOffset());
                compiler.addPop(3);
            } else {
                assert(sequence.type && sequence.children.size() == 2);
                compiler.compileBranch(*sequence.children[1].get());
                compiler.add(OP_INC, compiler.freeRegister - 1);
                compiler.addPush(compiler.freeRegister - 1);
                compiler.compileBranch(*sequence.children[0].get());
                compiler.addPush(compiler.freeRegister - 1);
                compiler.freeRegister = 0;
                const Node& variable = *node.children[0].get()->children[0]->children[0].get();
                compiler.addDropFrame(variable.children[0].get()->variant.s, +[](Compiler& compiler, Compiler::DropFrameState& state) {
                    int negativeOffset = state.stackPoint - compiler.stackSize;
                    compiler.add(OP_STACK, 0x0, negativeOffset - 1);
                    return 0;
                });
                int entryPointJmp = compiler.add(OP_JMP, 0x0, 0x0);
                int topLoop = compiler.add(OP_STACK, 0x0, -1);
                compiler.add(OP_STACK, 0x1, -2);
                compiler.add(OP_INC, 0x0);
                compiler.add(OP_POP, 0x0, 0x1);
                compiler.add(OP_PUSH, 0x0);
                int entryPoint = compiler.add(OP_SUB, 0x1);
                compiler.modify(entryPointJmp, OP_JMP, 0x0, entryPoint);
                int endLoopJmp = compiler.add(OP_JMPFALSE, 0x0, 0x0);
                compiler.compileBranch(*node.children[1].get());
                compiler.add(OP_JMP, 0x0, topLoop);
                compiler.modify(endLoopJmp, OP_JMPFALSE, 0x0, compiler.currentOffset());
                compiler.addPop(2);
            }
        }
    };


    template <class Function>
    struct ArithmeticOperatorNode : OperatorNodeType {
        ArithmeticOperatorNode(const char* symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        double operate(double v1, long long v2) const { return Function()(v1, v2); }
        double operate(double v1, double v2) const { return Function()(v1, v2); }
        double operate(long long v1, double v2) const { return Function()(v1, v2); }
        long long operate(long long v1, long long v2) const { return Function()(v1, v2); }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = getOperand(renderer, node, store, 0);
            Node op2 = getOperand(renderer, node, store, 1);
            switch (op1.variant.type) {
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        break;
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        break;
                        case Variant::Type::STRING:
                            if (symbol != "+")
                                return Node();
                            return Node(op1.variant.getString() + op2.variant.getString());
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
                        case Variant::Type::STRING:
                            if (symbol != "+")
                                return Node();
                            return Node(op1.variant.getString() + op2.variant.getString());
                        break;
                        default:
                        break;
                    }
                break;
                case Variant::Type::STRING:
                    if (symbol != "+")
                        return Node();
                    return Node(op1.variant.getString() + op2.variant.getString());
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            long long divisor = op2.variant.getInt();
            if (divisor == 0)
                return Node();
            return Variant(op1.variant.getInt() % divisor);
        }
    };

    struct UnaryMinusOperatorNode : OperatorNodeType {
        UnaryMinusOperatorNode() : OperatorNodeType("-", Arity::UNARY, 10, Fixness::PREFIX) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = getChild(renderer, node, store, 0);
            if (op1.variant.type == Variant::Type::INT)
                return Variant(op1.variant.i * -1);
            if (op1.variant.type == Variant::Type::FLOAT)
                return Variant(op1.variant.f * -1);
            return Node();
        }
    };

    struct UnaryNegationOperatorNode : OperatorNodeType {
        UnaryNegationOperatorNode() : OperatorNodeType("!", Arity::UNARY, 15, Fixness::PREFIX) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            return Variant(!op1.variant.isTruthy(renderer.context.falsiness));
        }
    };


    template <class Function>
    struct NumericalComparaisonOperatorNode : OperatorNodeType {
        NumericalComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
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

    template <class Function>
    struct QualitativeComparaisonOperatorNode : OperatorNodeType {
        QualitativeComparaisonOperatorNode(const std::string& symbol, int priority) : OperatorNodeType(symbol, Arity::BINARY, priority) { }

        template <class A, class B>
        bool operate(A a, B b) const { return Function()(a, b); }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);

            if (op2.variant.type == Variant::Type::NIL)
                return Node(operate(op1.variant.type, Variant::Type::NIL));
            switch (op1.variant.type) {
                case Variant::Type::NIL:
                    if (op2.variant.type == Variant::Type::NIL)
                        return Variant(operate(nullptr, nullptr));
                    return Variant(!operate(nullptr, nullptr));
                case Variant::Type::BOOL:
                    return Node(Variant(operate(op1.variant.b, op2.variant.isTruthy(renderer.context.falsiness))));
                case Variant::Type::INT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.i)));
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.i, op2.variant.f)));
                        case Variant::Type::STRING:
                            return Node(Variant(operate(op1.variant.i, op2.variant.getInt())));
                        default:
                        break;
                    }
                break;
                case Variant::Type::FLOAT:
                    switch (op2.variant.type) {
                        case Variant::Type::INT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.i)));
                        case Variant::Type::FLOAT:
                            return Node(Variant(operate(op1.variant.f, op2.variant.f)));
                        case Variant::Type::STRING:
                            return Node(Variant(operate(op1.variant.i, op2.variant.getFloat())));
                        default:
                        break;
                    }
                break;
                case Variant::Type::STRING:
                    if (op2.variant.type != Variant::Type::STRING)
                        return Variant(operate(op1.variant.s, op2.variant.getString()));
                    return Variant(operate(op1.variant.s, op2.variant.s));
                case Variant::Type::POINTER:
                    if (op2.variant.type != Variant::Type::POINTER)
                        return Variant(false);
                    return Variant(operate(op1.variant.p, op2.variant.p));
                case Variant::Type::VARIABLE:
                    if (op2.variant.type != Variant::Type::VARIABLE)
                        return Variant(false);
                    return Variant(operate(op1.variant.v.pointer, op2.variant.v.pointer));
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
        AndOperatorNode() : OperatorNodeType("and", Arity::BINARY, 1, Fixness::INFIX, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (!op1.variant.isTruthy(renderer.context.falsiness))
                return Variant(false);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Variant(op2.variant.isTruthy(renderer.context.falsiness));
        }

        bool optimize(Optimizer& optimizer, Node& node, Variable store) const override {
            if (
                (!node.children[0].get()->type && !node.children[0].get()->variant.isTruthy(optimizer.renderer.context.falsiness)) ||
                (!node.children[1].get()->type && !node.children[1].get()->variant.isTruthy(optimizer.renderer.context.falsiness))
            ) {
                node = Variant(false);
                return true;
            }
            if (
                (!node.children[0].get()->type && node.children[0].get()->variant.isTruthy(optimizer.renderer.context.falsiness)) &&
                (!node.children[1].get()->type && node.children[1].get()->variant.isTruthy(optimizer.renderer.context.falsiness))
            ) {
                node = Variant(true);
                return true;
            }
            return false;
        }
    };
    struct OrOperatorNode : OperatorNodeType {
        OrOperatorNode() : OperatorNodeType("or", Arity::BINARY, 1, Fixness::INFIX, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            if (op1.variant.isTruthy(renderer.context.falsiness))
                return Variant(true);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            return Variant(op2.variant.isTruthy(renderer.context.falsiness));
        }

        bool optimize(Optimizer& optimizer, Node& node, Variable store) const override {
            if (
                (!node.children[0].get()->type && node.children[0].get()->variant.isTruthy(optimizer.renderer.context.falsiness)) ||
                (!node.children[1].get()->type && node.children[1].get()->variant.isTruthy(optimizer.renderer.context.falsiness))
            ) {
                node = Variant(true);
                return true;
            }
            if (
                (!node.children[0].get()->type && !node.children[0].get()->variant.isTruthy(optimizer.renderer.context.falsiness)) &&
                (!node.children[1].get()->type && !node.children[1].get()->variant.isTruthy(optimizer.renderer.context.falsiness))
            ) {
                node = Variant(false);
                return true;
            }
            return false;
        }
    };


    struct ContainsOperatorNode : OperatorNodeType {
        ContainsOperatorNode() : OperatorNodeType("contains", Arity::BINARY, 2) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);

            if (op2.variant.type != Variant::Type::STRING)
                return Node();
            switch (op1.variant.type) {
                case Variant::Type::STRING:
                    return Variant(op1.variant.s.find(op2.getString()) != string::npos);
                case Variant::Type::ARRAY:
                    for (size_t i = 0; i < op1.variant.a.size(); ++i) {
                        if (op1.variant.a[i].type == Variant::Type::STRING && op1.variant.a[i].s.find(op2.getString()) != string::npos)
                            return Variant(true);
                    }
                    return Variant(false);
                default:
                    return Node();
            }
        }
    };

    struct RangeOperatorNode : OperatorNodeType {
        RangeOperatorNode() : OperatorNodeType("..", Arity::BINARY, 10) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = renderer.retrieveRenderedNode(*node.children[0].get(), store);
            Node op2 = renderer.retrieveRenderedNode(*node.children[1].get(), store);
            // Requires integers.
            if (op1.variant.type != Variant::Type::INT || op2.variant.type != Variant::Type::INT)
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Node op1 = getOperand(renderer, node, store);
            Node op2 = getArgument(renderer, node, store, 0);
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            long long divisor = argument.variant.getInt();
            if (divisor == 0)
                return Node();
            return Node(Variant(operand.variant.getInt() % divisor));
        }
    };

    struct AbsFilterNode : FilterNodeType {
        AbsFilterNode() : FilterNodeType("abs", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            return Variant(operand.getString() + argument.getString());
        }
    };

    struct camelCaseFilterNode : FilterNodeType {
        camelCaseFilterNode() : FilterNodeType("camelcase", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            // TODO
            return Node();
        }
    };

    struct CapitalizeFilterNode : FilterNodeType {
        CapitalizeFilterNode() : FilterNodeType("capitalize", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            string str = operand.getString();
            str[0] = toupper(str[0]);
            return Node(str);
        }
    };
    struct DowncaseFilterNode : FilterNodeType {
        DowncaseFilterNode() : FilterNodeType("downcase", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
            return Variant(str);
        }
    };
    struct HandleGenericFilterNode : FilterNodeType {
        HandleGenericFilterNode(const string& symbol) : FilterNodeType(symbol, 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument1 = getArgument(renderer, node, store, 1);
            auto argument2 = getArgument(renderer, node, store, 2);
            return Variant(operand.variant.getInt() > 1 ? argument2.getString() : argument1.getString());
        }
    };

    struct PrependFilterNode : FilterNodeType {
        PrependFilterNode() : FilterNodeType("prepend", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            return Variant(argument.getString() + operand.getString());
        }
    };
    struct RemoveFilterNode : FilterNodeType {
        RemoveFilterNode() : FilterNodeType("remove", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argumentPattern = getArgument(renderer, node, store, 0);
            auto argumentReplacement = getArgument(renderer, node, store, 1);
            string accumulator;
            string str = operand.getString();
            string pattern = argumentPattern.getString();
            string replacement = argumentReplacement.getString();
            size_t start = 0, idx;
            while ((idx = str.find(pattern, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                accumulator.append(replacement);
                start = idx + pattern.size();
            }
            accumulator.append(str, start, str.size() - start);
            return Variant(accumulator);
        }
    };
    struct ReplaceFirstFilterNode : FilterNodeType {
        ReplaceFirstFilterNode() : FilterNodeType("replacefirst", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argumentPattern = getArgument(renderer, node, store, 0);
            auto argumentReplacement = getArgument(renderer, node, store, 1);
            string accumulator;
            string str = operand.getString();
            string pattern = argumentPattern.getString();
            string replacement = argumentReplacement.getString();
            size_t start = 0, idx;
            while ((idx = str.find(pattern, start)) != string::npos) {
                if (idx > start)
                    accumulator.append(str, start, idx - start);
                accumulator.append(replacement);
                start = idx + pattern.size();
                break;
            }
            accumulator.append(str, start, str.size() - start);
            return Variant(accumulator);
        }
    };
    struct SliceFilterNode : FilterNodeType {
        SliceFilterNode() : FilterNodeType("slice", 1, 2) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            auto arg2 = getArgument(renderer, node, store, 0);

            if (operand.variant.type != Variant::Type::STRING)
                return Node();
            string str = operand.getString();
            long long offset = std::max(std::min(arg1.variant.getInt(), (long long)str.size()), 0LL);
            long long size = std::min(arg2.variant.getInt(), (long long)(str.size() - offset));
            return Variant(str.substr(offset, size));
        }
    };
    struct SplitFilterNode : FilterNodeType {
        SplitFilterNode() : FilterNodeType("split", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            Variant result { vector<Variant>() };
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            string str = operand.getString();
            size_t start, end;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Node(str.substr(start, end - start + 1));
        }
    };
    struct LStripFilterNode : FilterNodeType {
        LStripFilterNode() : FilterNodeType("lstrip", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            string str = operand.getString();
            size_t start;
            for (start = 0; start < str.size() && isblank(str[start]); ++start);
            return Node(str.substr(start, str.size() - start));
        }
    };
    struct RStripFilterNode : FilterNodeType {
        RStripFilterNode() : FilterNodeType("rstrip", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            string str = operand.getString();
            size_t end;
            for (end = str.size()-1; end > 0 && isblank(str[end]); --end);
            return Node(str.substr(0, end + 1));
        }
    };
    struct StripNewlinesFilterNode : FilterNodeType {
        StripNewlinesFilterNode() : FilterNodeType("strip_newlines", 0, 0) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto characterCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto wordCount = getArgument(renderer, node, store, 0);
            auto customEllipsis = getArgument(renderer, node, store, 1);
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
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            string str = operand.getString();
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::toupper(c); });
            return Variant(str);
        }
    };

    struct ArrayFilterNodeType : FilterNodeType {
        ArrayFilterNodeType(const std::string& symbol, int minArguments = -1, int maxArguments = -1) : FilterNodeType(symbol, minArguments, maxArguments) { }
    };

    struct JoinFilterNode : ArrayFilterNodeType {
        JoinFilterNode() : ArrayFilterNodeType("join", 0, 1) { }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            struct JoinStruct {
                Renderer& renderer;
                string accumulator;
                string joiner;
                int idx;
            };

            JoinStruct joinStruct({ renderer, "", "", 0 });
            auto argument = getArgument(renderer, node, store, 0);
            if (!argument.type)
                joinStruct.joiner = argument.getString();


            renderer.variableResolver.iterate(renderer, operand, +[](void* variable, void* data) {
                JoinStruct& joinStruct = *(JoinStruct*)data;
                string part;
                if (joinStruct.idx++ > 0)
                    joinStruct.accumulator.append(joinStruct.joiner);
                if (joinStruct.renderer.resolveVariableString(part, variable))
                    joinStruct.accumulator.append(part);
                return true;
            }, &joinStruct, 0, -1, false);
            return Node(joinStruct.accumulator);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            if (operand.variant.type == Variant::Type::ARRAY)
                return variantOperate(renderer, node, store, operand.variant);
            else if (operand.variant.type == Variant::Type::VARIABLE)
                return variableOperate(renderer, node, store, operand.variant.v);
            return Node();
        }
    };

    struct ConcatFilterNode : ArrayFilterNodeType {
        ConcatFilterNode() : ArrayFilterNodeType("concat", 1, 1) { }

        void accumulate(Renderer& renderer, Variant& accumulator, Variable v) const {
            renderer.variableResolver.iterate(renderer, v, +[](void* variable, void* data) {
                static_cast<Variant*>(data)->a.push_back(Variant(static_cast<Variable*>(variable)));
                return true;
            }, &accumulator, 0, -1, false);
        }

        void accumulate(Renderer& renderer, Variant& accumulator, const Variant& v) const {
            for (auto it = v.a.begin(); it != v.a.end(); ++it)
                accumulator.a.push_back(*it);
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
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
            Renderer& renderer;
            string property;
            Variant accumulator;
        };

        void accumulate(Renderer& renderer, MapStruct& mapStruct, Variable v) const {
            renderer.variableResolver.iterate(renderer, v, +[](void* variable, void* data) {
                MapStruct& mapStruct = *static_cast<MapStruct*>(data);
                Variable target;
                if (mapStruct.renderer.variableResolver.getDictionaryVariable(mapStruct.renderer, variable, mapStruct.property.data(), target))
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
                    if (mapStruct.renderer.variableResolver.getDictionaryVariable(mapStruct.renderer, const_cast<Variable&>(it->v), mapStruct.property.data(), target))
                        mapStruct.accumulator.a.push_back(target);
                    else
                        mapStruct.accumulator.a.push_back(Variant());
                } else {
                    mapStruct.accumulator.a.push_back(Variant());
                }
            }
        }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            MapStruct mapStruct = { renderer, argument.getString(), nullptr };
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Variant accumulator;
            auto operand = getOperand(renderer, node, store);
            auto& v = operand.variant;
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE:
                    renderer.variableResolver.iterate(renderer, v.v, +[](void* variable, void* data) {
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            Variant accumulator;
            string property;
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            switch (operand.variant.type) {
                case Variant::Type::VARIABLE: {
                    renderer.variableResolver.iterate(renderer, operand.variant.v, +[](void* variable, void* data) {
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
                std::sort(accumulator.a.begin(), accumulator.a.end(), [&property, &renderer](Variant& a, Variant& b) -> bool {
                    if (a.type != Variant::Type::VARIABLE || b.type != Variant::Type::VARIABLE)
                        return false;
                    Variable targetA, targetB;
                    if (!renderer.variableResolver.getDictionaryVariable(renderer, a.v, property.data(), targetA))
                        return false;
                    if (!renderer.variableResolver.getDictionaryVariable(renderer, b.v, property.data(), targetB))
                        return false;
                    return renderer.variableResolver.compare(targetA, targetB) < 0;
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
            Renderer& renderer;
            string property;
            Variant value;
            Variant accumulator;
        };

        void accumulate(WhereStruct& whereStruct, Variable v) const {
            whereStruct.renderer.variableResolver.iterate(whereStruct.renderer, v, +[](void* variable, void* data) {
                WhereStruct& whereStruct = *static_cast<WhereStruct*>(data);
                Variable target;
                if (whereStruct.renderer.variableResolver.getDictionaryVariable(whereStruct.renderer, variable, whereStruct.property.data(), target)) {
                    if (whereStruct.property.empty()) {
                        if (whereStruct.renderer.variableResolver.getTruthy(whereStruct.renderer, target))
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
                    if (whereStruct.renderer.variableResolver.getDictionaryVariable(whereStruct.renderer, const_cast<Variable&>(it->v), whereStruct.property.data(), target)) {
                        if (whereStruct.property.empty()) {
                            if (whereStruct.renderer.variableResolver.getTruthy(whereStruct.renderer, target))
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            auto arg2 = getArgument(renderer, node, store, 1);
            WhereStruct whereStruct = { renderer, arg1.getString(), arg2.variant, nullptr };
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
            std::unordered_set<size_t> hashes;
            Variant accumulator;
        };

        void accumulate(UniqStruct& uniqStruct, Variable v) const {
            uniqStruct.renderer.variableResolver.iterate(uniqStruct.renderer, v, +[](void* variable, void* data) {
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto arg1 = getArgument(renderer, node, store, 0);
            auto arg2 = getArgument(renderer, node, store, 1);
            UniqStruct uniqStruct = { renderer, {}, nullptr };
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
            if (!renderer.variableResolver.getArrayVariable(renderer, operand, 0, v))
                return Node();
            return renderer.parseVariant(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant operand) const {
            if (operand.type != Variant::Type::ARRAY || operand.a.size() == 0)
                return Node();
            return operand.a[0];
        }
    };

    struct FirstDotFilterNode : DotFilterNodeType {
        FirstDotFilterNode() : DotFilterNodeType("first") { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            if (node.type && node.children.size() == 1 && node.children[0]->type && node.children[0]->type->type == NodeType::Type::VARIABLE && node.children[0]->children.size() == 1) {
                pair<void*, Renderer::DropFunction> drop = renderer.getInternalDrop(*node.children[0].get(), store);
                if (drop.second)
                    return drop.second(renderer, Variant("first"), store, drop.first);
            }
            auto operand = getOperand(renderer, node, store);
            switch (operand.variant.type) {
                case Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, operand.variant.v);
                default:
                    return Node();
            }
        }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            Variable v;
            LiquidVariableType type = renderer.variableResolver.getType(renderer, operand);
            if (type == LiquidVariableType::LIQUID_VARIABLE_TYPE_DICTIONARY) {
                if (!renderer.variableResolver.getDictionaryVariable(renderer, operand, "first", v))
                    return Node();
                return renderer.parseVariant(v);
            }
            if (!renderer.variableResolver.getArrayVariable(renderer, operand, 0, v))
                return Node();
            return renderer.parseVariant(v);
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
            long long size = renderer.variableResolver.getArraySize(renderer, operand);
            if (size == -1 || !renderer.variableResolver.getArrayVariable(renderer, operand, size - 1, v))
                return Node();
            return renderer.parseVariant(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant operand) const {
            if (operand.type != Variant::Type::ARRAY || operand.a.size() == 0)
                return Node();
            return operand.a[operand.a.size()-1];
        }
    };


    struct LastDotFilterNode : DotFilterNodeType {
        LastDotFilterNode() : DotFilterNodeType("last") { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            if (node.type && node.type->type == NodeType::Type::VARIABLE && node.children.size() == 1) {
                pair<void*, Renderer::DropFunction> drop = renderer.getInternalDrop(*node.children[0].get(), store);
                if (drop.second)
                    return drop.second(renderer, Variant("first"), store, drop.first);
            }
            auto operand = getOperand(renderer, node, store);
            switch (operand.variant.type) {
                case Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, operand.variant.v);
                default:
                    return Node();
            }
        }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            Variable v;
            LiquidVariableType type = renderer.variableResolver.getType(renderer, operand);
            if (type == LiquidVariableType::LIQUID_VARIABLE_TYPE_DICTIONARY) {
                if (!renderer.variableResolver.getDictionaryVariable(renderer, operand, "last", v))
                    return Node();
                return renderer.parseVariant(v);
            }
            if (!renderer.variableResolver.getArrayVariable(renderer, operand, -1, v))
                return Node();
            return renderer.parseVariant(v);
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
            int idx = argument.variant.i;
            if (!renderer.variableResolver.getArrayVariable(renderer, operand, idx, v))
                return Node();
            return Node(v);
        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
            auto argument = getArgument(renderer, node, store, 0);
            long long idx = argument.variant.i;
            if (operand.type != Variant::Type::ARRAY || idx >= (long long)operand.a.size())
                return Node();
            return operand.a[idx];
        }
    };


    struct SizeFilterNode : FilterNodeType {
        SizeFilterNode() : FilterNodeType("size", 0, 0) { }

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
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
            long long size = renderer.variableResolver.getArraySize(renderer, operand);
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

        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            if (node.type && node.type->type == NodeType::Type::VARIABLE && node.children.size() == 1) {
                pair<void*, Renderer::DropFunction> drop = renderer.getInternalDrop(*node.children[0].get(), store);
                if (drop.second)
                    return drop.second(renderer, Variant("size"), store, drop.first);
            }
            auto operand = getOperand(renderer, node, store);
            switch (operand.variant.type) {
                case Variant::Type::ARRAY:
                    return variantOperate(renderer, node, store, operand.variant);
                case Variant::Type::VARIABLE:
                    return variableOperate(renderer, node, store, operand.variant.v);
                case Variant::Type::STRING:
                    return Variant((long long)operand.variant.s.size());
                default:
                    return Variant((long long)operand.variant.getString().size());
            }
        }

        Node variableOperate(Renderer& renderer, const Node& node, Variable store, Variable operand) const {
            LiquidVariableType type = renderer.variableResolver.getType(renderer, operand);
            switch (type) {
                case LiquidVariableType::LIQUID_VARIABLE_TYPE_DICTIONARY: {
                    Variable v;
                    if (!renderer.variableResolver.getDictionaryVariable(renderer, operand, "size", v))
                        return Node();
                    return Node(v);
                } break;
                case LiquidVariableType::LIQUID_VARIABLE_TYPE_ARRAY: {
                    long long size = renderer.variableResolver.getArraySize(renderer, operand);
                    return size != -1 ? Node(size) : Node();
                } break;
                default:
                    return Variant(renderer.variableResolver.getStringLength(renderer, operand));
                break;
            }

        }

        Node variantOperate(Renderer& renderer, const Node& node, Variable store, const Variant& operand) const {
            if (operand.type != Variant::Type::ARRAY)
                return Node();
            return Node((long long)operand.a.size());
        }
    };



    struct DefaultFilterNode : FilterNodeType {
        DefaultFilterNode() : FilterNodeType("default", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            auto operand = getOperand(renderer, node, store);
            auto argument = getArgument(renderer, node, store, 0);
            if (operand.variant.isTruthy(renderer.context.falsiness))
                return operand;
            return argument;
        }
    };

    struct DateFilterNode : FilterNodeType {
        DateFilterNode() : FilterNodeType("date", 1, 1) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            time_t operand = (time_t)getOperand(renderer, node, store).variant.getInt();
            string argument = getArgument(renderer, node, store, 0).getString();
            struct tm * timeinfo = localtime(&operand);
            static constexpr int MAX_BUFFER_SIZE = 256;
            string buffer;
            buffer.resize(MAX_BUFFER_SIZE);
            buffer.resize(strftime(&buffer[0], MAX_BUFFER_SIZE, argument.c_str(), timeinfo));
            return Variant(move(buffer));
        }
    };

    template <class T>
    void registerStandardDialectFilters(T& context) {

        // Math filters.
        context.template registerType<PlusFilterNode>();
        context.template registerType<MinusFilterNode>();
        context.template registerType<MultiplyFilterNode>();
        context.template registerType<DivideFilterNode>();
        context.template registerType<AbsFilterNode>();
        context.template registerType<AtMostFilterNode>();
        context.template registerType<AtLeastFilterNode>();
        context.template registerType<CeilFilterNode>();
        context.template registerType<FloorFilterNode>();
        context.template registerType<RoundFilterNode>();
        context.template registerType<ModuloFilterNode>();

        // String filters. Those that are HTML speciifc, or would require minor external libraries
        // are left off for now.
        context.template registerType<AppendFilterNode>();
        context.template registerType<camelCaseFilterNode>();
        context.template registerType<CapitalizeFilterNode>();
        context.template registerType<DowncaseFilterNode>();
        context.template registerType<HandleFilterNode>();
        context.template registerType<HandleizeFilterNode>();
        context.template registerType<PluralizeFilterNode>();
        context.template registerType<PrependFilterNode>();
        context.template registerType<RemoveFilterNode>();
        context.template registerType<RemoveFirstFilterNode>();
        context.template registerType<ReplaceFilterNode>();
        context.template registerType<ReplaceFirstFilterNode>();
        context.template registerType<SliceFilterNode>();
        context.template registerType<SplitFilterNode>();
        context.template registerType<StripFilterNode>();
        context.template registerType<LStripFilterNode>();
        context.template registerType<RStripFilterNode>();
        context.template registerType<StripNewlinesFilterNode>();
        context.template registerType<TruncateFilterNode>();
        context.template registerType<TruncateWordsFilterNode>();
        context.template registerType<UpcaseFilterNode>();

        // Array filters.
        context.template registerType<JoinFilterNode>();
        context.template registerType<FirstFilterNode>();
        context.template registerType<LastFilterNode>();
        context.template registerType<ConcatFilterNode>();
        context.template registerType<IndexFilterNode>();
        context.template registerType<MapFilterNode>();
        context.template registerType<ReverseFilterNode>();
        context.template registerType<SizeFilterNode>();
        context.template registerType<SortFilterNode>();
        context.template registerType<WhereFilterNode>();
        context.template registerType<UniqFilterNode>();

        // Other filters.
        context.template registerType<DefaultFilterNode>();
        context.template registerType<DateFilterNode>();
    }

    void StandardDialect::implement(Context& context, bool globalAssignsOnly, bool disallowParentheses, bool assignConditionalOperatorsOnly, bool assignOutputFiltersOnly, bool disableArrayLiterals, EFalsiness falsiness) {
        context.falsiness = falsiness;
        context.disallowArrayLiterals = disableArrayLiterals;
        context.disallowGroupingOutsideAssign = disallowParentheses;

        // Control flow tags.
        TagNodeType* ifNode = static_cast<TagNodeType*>(context.registerType<IfNode>());
        TagNodeType* unlessNode = static_cast<TagNodeType*>(context.registerType<UnlessNode>());
        context.registerType<CaseNode>();

        // Iteration tags.
        context.registerType<ForNode>();
        context.registerType<ForNode::CycleNode>();
        context.registerType<ForNode::InOperatorNode>();
        context.registerType<ForNode::BreakNode>();
        context.registerType<ForNode::ContinueNode>();

        // Variable tags.
        TagNodeType* assignNode = static_cast<TagNodeType*>(globalAssignsOnly ? context.registerType<AssignNode<false>>() : context.registerType<AssignNode<true>>());

        context.registerType<CaptureNode>();
        context.registerType<IncrementNode>();
        context.registerType<DecrementNode>();

        // Other tags.
        context.registerType<CommentNode>();

        // Standard set of operators.
        if (assignConditionalOperatorsOnly) {
            assignNode->registerType<PlusOperatorNode>();
            assignNode->registerType<MinusOperatorNode>();
            assignNode->registerType<UnaryMinusOperatorNode>();
            assignNode->registerType<UnaryNegationOperatorNode>();
            assignNode->registerType<MultiplyOperatorNode>();
            assignNode->registerType<DivideOperatorNode>();
            assignNode->registerType<ModuloOperatorNode>();
            for (TagNodeType* type : { ifNode, unlessNode }) {
                type->registerType<LessThanOperatorNode>();
                type->registerType<LessThanEqualOperatorNode>();
                type->registerType<GreaterThanOperatorNode>();
                type->registerType<GreaterThanEqualOperatorNode>();
                type->registerType<EqualOperatorNode>();
                type->registerType<NotEqualOperatorNode>();
                type->registerType<AndOperatorNode>();
                type->registerType<OrOperatorNode>();
                type->registerType<ContainsOperatorNode>();
            }
        } else {
            context.registerType<PlusOperatorNode>();
            context.registerType<MinusOperatorNode>();
            context.registerType<UnaryMinusOperatorNode>();
            context.registerType<UnaryNegationOperatorNode>();
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
            context.registerType<ContainsOperatorNode>();
        }

        context.registerType<RangeOperatorNode>();

        if (assignOutputFiltersOnly) {
            registerStandardDialectFilters<TagNodeType>(*assignNode);
            registerStandardDialectFilters(*static_cast<ContextualNodeType*>(const_cast<NodeType*>(context.getOutputNodeType())));
        } else {
            registerStandardDialectFilters<Context>(context);
        }

        context.registerType<SizeDotFilterNode>();
        context.registerType<FirstDotFilterNode>();
        context.registerType<LastDotFilterNode>();
    }
}
