#ifndef DIALECT_H
#define DIALECT_H

namespace Liquid {
    struct Context;

    // Standard dialect of liquid, that implements core Liquid as described herE:
    // https://shopify.dev/docs/themes/liquid/reference
    struct StandardDialect {
        static void implement(Context& context);
    };
};

#endif
