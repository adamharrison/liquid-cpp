#include "context.h"
#include "optimizer.h"
#include "compiler.h"

namespace Liquid {

    bool NodeType::optimize(Optimizer& optimizer, Node& node, Variable store) const {
        node = render(optimizer.renderer, node, store);
        return true;
    }

    bool Context::VariableNode::optimize(Optimizer& optimizer, Node& node, Variable store) const {
        Variable storePointer = optimizer.renderer.getVariable(node, store);
        if (!storePointer.exists())
            return false;
        node = Node(optimizer.renderer.parseVariant(storePointer));
        return true;
    }

    Node NodeType::render(Renderer& renderer, const Node& node, Variable store) const {
        if (!userRenderFunction)
            return Node();
        userRenderFunction(LiquidRenderer{&renderer}, LiquidNode{const_cast<Node*>(&node)}, store, userData);
        return renderer.returnValue;
    }


    void NodeType::compile(Compiler& compiler, const Node& node) const {
        if (!userCompileFunction) {
            for (auto& child : node.children)
                compiler.compileBranch(*child.get());
            compiler.add(OP_CALL, 0x0, (long long)userRenderFunction);
        } else
            userCompileFunction(LiquidCompiler{&compiler}, LiquidNode{const_cast<Node*>(&node)}, userData);
    }



    Node FilterNodeType::getOperand(Renderer& renderer, const Node& node, Variable store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }
    Node DotFilterNodeType::getOperand(Renderer& renderer, const Node& node, Variable store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }

    Node NodeType::getArgument(Renderer& renderer, const Node& node, Variable store, int idx) const {
        int offset = node.type->type == NodeType::Type::TAG ? 0 : 1;
        if (idx >= (int)node.children[offset]->children.size())
            return Node();
        assert(node.children[offset]->type->type == NodeType::Type::ARGUMENTS);
        return renderer.retrieveRenderedNode(*node.children[offset]->children[idx].get(), store);
    }
    int NodeType::getArgumentCount(const Node& node) const {
        int offset = node.type->type == NodeType::Type::TAG ? 0 : 1;
        assert(node.children[offset]->type->type == NodeType::Type::ARGUMENTS);
        return node.children[offset]->children.size();
    }

    Node NodeType::getChild(Renderer& renderer, const Node& node, Variable store, int idx) const {
        if (idx >= (int)node.children.size())
            return Node();
        return renderer.retrieveRenderedNode(*node.children[idx].get(), store);
    }
    int NodeType::getChildCount(const Node& node) const {
        return node.children.size();
    }


    Node Context::ConcatenationNode::render(Renderer& renderer, const Node& node, Variable store) const {
        if (++renderer.currentRenderingDepth > renderer.maximumRenderingDepth) {
            --renderer.currentRenderingDepth;
            renderer.error = LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_DEPTH;
            return Node();
        }
        if (node.children.size() == 1) {
            --renderer.currentRenderingDepth;
            return renderer.retrieveRenderedNode(*node.children.front().get(), store);
        }
        string s;
        for (auto& child : node.children) {
            s.append(renderer.retrieveRenderedNode(*child.get(), store).getString());
            if (renderer.error != LIQUID_RENDERER_ERROR_TYPE_NONE)
                return Node();
            if (renderer.control != Renderer::Control::NONE) {
                --renderer.currentRenderingDepth;
                return Node(move(s));
            }
        }
        --renderer.currentRenderingDepth;
        return Node(move(s));
    }

    bool Context::ConcatenationNode::optimize(Optimizer& optimizer, Node& node, Variable store) const {
        if (++optimizer.renderer.currentRenderingDepth > optimizer.renderer.maximumRenderingDepth) {
            --optimizer.renderer.currentRenderingDepth;
            return false;
        }
        string s;
        vector<unique_ptr<Node>> newChildren;
        for (auto& child : node.children) {
            if (child->type) {
                if (!s.empty())
                    newChildren.push_back(make_unique<Node>(Variant(move(s))));
                newChildren.push_back(move(child));
            } else
                s.append(child->getString());
        }
        if (newChildren.size() == 0) {
            node = Variant(move(s));
        } else {
            newChildren.push_back(make_unique<Node>(Variant(move(s))));
            node.children = move(newChildren);
        }
        --optimizer.renderer.currentRenderingDepth;
        return true;
    }

    void OperatorNodeType::compile(Compiler& compiler, const Node& node) const {
        compiler.freeRegister = 0;
        for (auto& child : node.children) {
            compiler.compileBranch(*child.get());
            compiler.add(OP_PUSH, 0x0);
            compiler.freeRegister = 0;
        }
        compiler.add(OP_CALL, 0x0, node.children.size());
    }

    void Context::ConcatenationNode::compile(Compiler& compiler, const Node& node) const {
        for (auto& child : node.children) {
            if (child->type) {
                compiler.compileBranch(*child.get());
                compiler.add(OP_OUTPUT, 0x0);
            } else {
                assert(child->variant.type == Variant::Type::STRING);
                int offset = compiler.add(child->variant.s.data(), child->variant.s.size());
                compiler.add(OP_MOVSTR, 0x0, offset);
                compiler.add(OP_OUTPUT, 0x0);
            }
        }
    }

    void Context::ArgumentNode::compile(Compiler& compiler, const Node& node) const {
        for (auto& child : node.children) {
            compiler.compileBranch(*child.get());
            compiler.add(OP_PUSH, 0x0);
        }
    }


    void Context::OutputNode::compile(Compiler& compiler, const Node& node) const {
        assert(node.children.size() == 1);
        compiler.compileBranch(*node.children[0].get()->children[0].get());
    }


    void Context::VariableNode::compile(Compiler& compiler, const Node& node) const {
        int target = 0x0;
        for (size_t i = 0; i < node.children.size(); ++i) {
            compiler.compileBranch(*node.children[i].get());
            compiler.add(OP_RESOLVE, 0x0, target);
            if (i < node.children.size() -1) {
                compiler.add(OP_MOV, 0x0, 0x1);
                target = 0x1;
            }
        }
    }
}
