#include "renderer.h"
#include "context.h"

namespace Liquid {
    struct Context;


    void Renderer::render(const Parser::Node& ast, Variable& store, void (*callback)(const char* chunk, size_t size, void* data), void* data) {
        Parser::Node node = ast.type->render(*this, ast, store);
        assert(node.type == nullptr);
        auto s = node.getString();
        callback(s.data(), s.size(), data);
    }

    string Renderer::render(const Parser::Node& ast, Variable& store) {
        string accumulator;
        render(ast, store, +[](const char* chunk, size_t size, void* data){
            string* accumulator = (string*)data;
            accumulator->append(chunk, size);
        }, &accumulator);
        return accumulator;
    }

    Parser::Node Renderer::retrieveRenderedNode(const Parser::Node& node, Variable& store) {
        if (node.type)
            return node.type->render(*this, node, store);
        return node;
    }
}
