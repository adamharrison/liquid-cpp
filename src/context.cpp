#include "context.h"

namespace Liquid {

    Parser::Node FilterNodeType::getOperand(Renderer& renderer, const Parser::Node& node, Variable& store) const {
        return renderer.retrieveRenderedNode(*node.children[0].get(), store);
    }

    Parser::Node FilterNodeType::getArgument(Renderer& renderer, const Parser::Node& node, Variable& store, int idx) const {
        return renderer.retrieveRenderedNode(*node.children[1]->children[idx].get(), store);
    }

    Parser::Node Context::ConcatenationNode::render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
        string s;
        for (auto& child : node.children) {
            s.append(renderer.retrieveRenderedNode(*child.get(), store).getString());
            if (renderer.control != Renderer::Control::NONE)
                return Parser::Node(s);
        }
        return Parser::Node(s);
    }
};
