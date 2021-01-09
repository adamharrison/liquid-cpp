#ifndef LIQUIDDIALECTSWEB_H
#define LIQUIDDIALECTSWEB_H
#include "dialect.h"

#if LIQUID_INCLUDE_WEB_DIALECT
    namespace Liquid {
        struct Context;

        struct WebDialect : Dialect {
            static void implement(Context& context);
        };
    }
#endif

#endif

