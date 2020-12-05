#ifndef DIALECT_H
#define DIALECT_H
#include "common.h"

namespace Liquid {
    struct Context;

    // Standard dialect of liquid, that implements core Liquid as described here:
    // https://shopify.dev/docs/themes/liquid/reference.
    // There are a number of non-standard features we can invoke which make life easier if you're actually writing Liquid.
    // As such, you can either enable/disable features one at a time, or call `implementStrict`, or `implementPermissive` for the
    // defaults that are most/least/permissive.

    // The only tags missing from the standard set of non-web tags is {% include %}.
    struct StandardDialect {

        static void implement(
            Context& context,
            // If true, makes it so that {% assign %} can only target whole variable names; no derefernces allowed. So no {% assign a.b = 1 %}.
            bool globalAssignsOnly,
            // If true, ignores parentheses, and mostly ignores order of operations.
            bool disallowParentheses,
            // If true, disables all operators outside of the {% assign %} tag, or the {% if %} and {% unless %} tags as in Shopify.
            bool assignOperatorsOnly,
            // Determines whether something is truthy/falsy.
            // In strict; this is set to FALSY_NIL, which emuilates ruby; meaning only NIL, and the false boolean are false.
            // In permissive; this is set to FALSY_EMPTY_STRING | FALSY_NIL | FALSY_0, which emulates perl, where all those are treated as false (the sane option, really).
            EFalsiness falsiness
        );

        static void implementStrict(Context& context) {
            implement(
                context,
                true,
                true,
                true,
                FALSY_NIL
            );
        }

        static void implementPermissive(Context& context) {
            implement(
                context,
                false,
                false,
                false,
                (EFalsiness)(FALSY_NIL | FALSY_0 | FALSY_EMPTY_STRING)
            );
        }
    };
};

#endif
