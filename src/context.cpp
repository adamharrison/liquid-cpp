#include "context.h"

namespace Liquid {
    void Context::render(const Parser::Node& ast, Variable& store, void (*callback)(const char* chunk, size_t size, void* data), void* data) {
        Parser::Node node = ast.type->render(*this, ast, store);
        assert(node.type == nullptr);
        auto s = node.getString();
        callback(s.data(), s.size(), data);
    }

};
