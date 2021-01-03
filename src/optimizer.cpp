#include "optimizer.h"
#include "renderer.h"

namespace Liquid {

    void Optimizer::optimize(Node& ast, Variable store) {
        bool hasAnyNonRendered = false;
        if (!ast.type)
            return;
        for (size_t i = 0; i < ast.children.size(); ++i) {
            if (ast.children[i]->type)
                optimize(*ast.children[i].get(), store);
            if (ast.children[i]->type) {
                if (ast.children[i]->type->type == NodeType::Type::ARGUMENTS) {
                    for (auto& child : ast.children[i]->children) {
                        if (child->type)
                            hasAnyNonRendered = true;
                    }
                } else {
                    hasAnyNonRendered = true;
                }
            }
        }

        if (hasAnyNonRendered) {
            if (ast.type->optimization == LIQUID_OPTIMIZATION_SCHEME_PARTIAL)
                ast.type->optimize(*this, ast, store);
        } else if (ast.type->optimization != LIQUID_OPTIMIZATION_SCHEME_NONE) {
            ast.type->optimize(*this, ast, store);
        }
    }
}
