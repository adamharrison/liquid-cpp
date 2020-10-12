#ifndef RENDERER_H
#define RENDERER_H

#include "context.h"
#include "parser.h"

namespace Liquid {
    template <class Context>
    struct Renderer {
        const Context& context;

        Renderer(const Context& context) : context(context) { }

        void render(const typename Context::DefaultVariable& variableStore, const typename Parser<Context>::Node& ast, void (*)(const char* chunk, size_t size, void* data), void* data) {

        }

        string render(const typename Context::DefaultVariable& variableStore, const typename Parser< Context>::Node& ast) {
            string accumulator;
            render(variableStore, ast, +[](const char* chunk, size_t size, void* data){
                string* accumulator = (string*)data;
                accumulator->append(chunk, size);
            }, this);
            return accumulator;
        }
    };
}

#endif
