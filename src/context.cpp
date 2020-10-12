#include "context.h"

namespace Liquid {

    struct IfNode {
        static constexpr const char* name = "if";
        static constexpr bool free = false;
    };
    struct OutputNode {

    };


    DefaultContext::DefaultContext() {
        registerType<IfNode>();
    }
};
