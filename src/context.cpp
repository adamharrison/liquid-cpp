#include "context.h"

namespace Liquid {

    Node FilterNodeType::getOperand(Renderer& renderer, const Node& node, Variable store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }
    Node DotFilterNodeType::getOperand(Renderer& renderer, const Node& node, Variable store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }


    Node FilterNodeType::getArgument(Renderer& renderer, const Node& node, Variable store, int idx) const {
        if (idx >= (int)node.children[1]->children.size())
            return Node();
        return renderer.retrieveRenderedNode(*node.children[1]->children[idx].get(), store);
    }

    Node Context::ConcatenationNode::render(Renderer& renderer, const Node& node, Variable store) const {
        string s;
        if (++renderer.currentRenderingDepth > renderer.maximumRenderingDepth) {
            --renderer.currentRenderingDepth;
            renderer.error = LIQUID_RENDER_ERROR_TYPE_EXCEEDED_DEPTH;
            return Node();
        }
        for (auto& child : node.children) {
            s.append(renderer.retrieveRenderedNode(*child.get(), store).getString());
            if (renderer.error != LIQUID_RENDER_ERROR_TYPE_NONE)
                return Node();
            if (renderer.control != Renderer::Control::NONE) {
                --renderer.currentRenderingDepth;
                return Node(s);
            }
        }
        --renderer.currentRenderingDepth;
        return Node(s);
    }

};
