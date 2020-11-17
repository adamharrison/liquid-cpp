#ifndef RENDERER_H
#define RENDERER_H

#include "parser.h"

namespace Liquid {
    struct Context;

    // One renderer per thread; though many renderers can be instantiated.
    struct Renderer {
        const Context& context;

        enum class Control {
            NONE,
            BREAK,
            CONTINUE
        };
        // The current state of the break. Allows us to have break/continue statements.
        Control control = Control::NONE;

        Renderer(const Context& context) : context(context) { }

        void render(const Parser::Node& ast, Variable& store, void (*)(const char* chunk, size_t size, void* data), void* data);
        string render(const Parser::Node& ast, Variable& store);
        Parser::Node retrieveRenderedNode(const Parser::Node& node, Variable& store);
    };
}

#endif
