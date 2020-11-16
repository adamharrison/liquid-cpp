#include "context.h"

namespace Liquid {
    void Context::render(const Parser::Node& ast, Variable& store, void (*callback)(const char* chunk, size_t size, void* data), void* data) {
        Parser::Node node = ast.type->render(*this, ast, store);
        assert(node.type == nullptr);
        auto s = node.getString();
        callback(s.data(), s.size(), data);
    }

    Parser::Node FilterNodeType::getOperand(const Context& context, const Parser::Node& node, Variable& store) const {
        return context.retrieveRenderedNode(*node.children[0].get(), store);
    }

    Parser::Node FilterNodeType::getArgument(const Context& context, const Parser::Node& node, Variable& store, int idx) const {
        return context.retrieveRenderedNode(*node.children[1]->children[idx].get(), store);
    }
};
