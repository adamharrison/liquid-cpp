#ifndef RENDERER_H
#define RENDERER_H

#include "context.h"
#include "parser.h"

namespace Liquid {
    template <class Context>
    struct Renderer {
        const Context& context;


        Renderer(const Context& context) : context(context) { }
    };
}

#endif
