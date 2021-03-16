#ifndef LIQUIDOPTIMIZER_H
#define LIQUIDOPTIMIZER_H

#include "common.h"

namespace Liquid {
    struct Renderer;
    struct Node;

    struct Optimizer {
        Renderer& renderer;

        Optimizer(Renderer& renderer);
        void optimize(Node& ast, Variable store);
    };
}

#endif
