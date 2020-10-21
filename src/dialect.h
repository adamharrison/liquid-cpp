#ifndef DIALECT_H
#define DIALECT_H

namespace Liquid {
    struct Context;

    struct StandardDialect {
        static void implement(Context& context);
    };
};

#endif
