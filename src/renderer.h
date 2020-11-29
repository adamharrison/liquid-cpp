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

        // If set, this will stop rendering with an error if the limits here, in bytes are breached for this renderer.
        unsigned int maximumMemoryUsage = 0;
        // If set, this will stop rendering with an error if the limits here, in milisecnods, are breached for this renderer.
        unsigned int maximumRenderingTime = 0;

        Renderer(const Context& context) : context(context) { }

        std::chrono::system_clock::time_point renderStartTime;

        void render(const Node& ast, Variable store, void (*)(const char* chunk, size_t size, void* data), void* data);
        string render(const Node& ast, Variable store);
        Node retrieveRenderedNode(const Node& node, Variable store);
        std::chrono::duration<unsigned int,std::milli> getRenderedTime() const;

        operator LiquidRenderer() { return LiquidRenderer {this}; }

        void inject(Variable& variable, const Variant& variant);
        Variant parseVariant(Variable variable);
        string getString(const Node& node);
    };
}

#endif
