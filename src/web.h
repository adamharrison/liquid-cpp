#ifndef DIALECTSWEB_H
#define DIALECTSWEB_H
#include "dialect.h"

namespace Liquid {
    struct Context;

    struct WebDialect :Dialect {
        static void implement(Context& context);
    };
};

#endif

