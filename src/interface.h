#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
    /* ## Liquid Interface

    This is the primary C interface to the LiquidCPP module.

    An example usage would go like this.

        // The liquid context represents all registered tags, operators, filters, and a way to resolve variables.
        LiquidContext context = liquidCreateContext(LIQUID_SETTINGS_DEFAULT);

        // Very few of the bits of liquid are part of the 'core'; instead, they are implemented as dialects. In order to
        // stick in most of the default bits of Liquid, you can ask the context to implement the standard dialect, but this
        // is not necessary. The only default tag available is {% raw %}, as this is less a tag, and more a lexing hint. No
        // filters, or operators, other than the unary - operator, are available by default; they are are all part of the liquid standard dialect.
        liquidImplementStandardDialect(context);
        // In addition, dialects can be layered. Implementing one dialects does not forgo implementating another; and dialects
        // can override one another; whichever dialect was applied last will apply its proper tags, operators, and filters.
        // Currently, there is no way to deregsiter a tag, operator, or filter once registered.

        // If no LiquidVariableResolver is specified; an internal default is used that won't read anything you pass in, but will funciton for {% assign %}, {% capture %} and other tags.
        LiquidVariableResolver resolver = {
            ...
        };
        liquidRegisterVariableResolver(resolver);

        const char exampleFile[] = "{% if a > 1 %}123423{% else %}sdfjkshdfjkhsdf{% endif %}";
        LiquidTemplate tmpl = liquidCreateTemplate(context, exampleFile, sizeof(exampleFile)-1);
        if (liquidGetError()) {
            fprintf(stderr, "Error parsing template: %s", liquidGetError());
            exit(-1);
        }
        // This object should be thread-local.
        LiquidRenderer renderer = liquidCreateRenderer(context);
        // Use something that works with your language here; as resolved by the LiquidVariableResolver above.
        void* variableStore = ...;

        int size;
        char* result = liquidRenderTemplate(variableStore, tmpl, &size);
        fprintf(stdout, "%s", result);

        // All resources, unless otherwise specified, must be free explicitly.
        liquidFreeTemplateRender(result);
        liquidFreeRenderer(renderer);
        liquidFreeTemplate(tmpl);
        liquidFreeContext(context);



    */

    enum ETagType {
        TAG_TYPE_ENCLOSING,
        TAG_TYPE_FREE
    };

    struct LiquidContext {
        void* context;
    };
    struct LiquidRenderer {
        void* renderer;
    };
    struct LiquidTemplate {
        void* ast;
    };
    struct LiquidNode {
        void* node;
    };

    struct LiquidTemplateRender {
        void* internal;
    };

    enum ELiquidSettings {
        LIQUID_SETTINGS_DEFAULT = 0,
        // Can do {% assign a[1] = ... %}.
        LIQUID_SETTINGS_EXTENDED_ASSIGNMENT_SYNTAX = (1 << 1),
        // Can use parentheses, operators, and everything in all expressions, not just assignments.
        LIQUID_SETTINGS_EXTENDED_EXPRESSION_SYNTAX = (1 << 2),

    };

    enum ELiquidVariableType {
        LIQUID_VARIABLE_TYPE_NIL,
        LIQUID_VARIABLE_TYPE_FLOAT,
        LIQUID_VARIABLE_TYPE_INT,
        LIQUID_VARIABLE_TYPE_STRING,
        LIQUID_VARIABLE_TYPE_ARRAY,
        LIQUID_VARIABLE_TYPE_BOOL,
        LIQUID_VARIABLE_TYPE_DICTIONARY,
        LIQUID_VARIABLE_TYPE_OTHER
    };

    // Convenience function to register a custom variable type.
    // Ownership model looks thusly:
    // Calling create creates a newly allocated pointer. In all cases, one of the two things must happen:
    //  1. It must be set as an array element, or a hash element.
    //  2. It must be freed with freeVariable.
    // For languages where the variables are garbage collected, like perl and ruby; freeVariable will be a no-op.
    // Whenever getArrayVariable or getDictionaryVariable are called, a pointer is given, but no allocaitons are made.
    struct LiquidVariableResolver {
        ELiquidVariableType (*getType)(void* variable);
        bool (*getBool)(void* variable, bool* target);
        bool (*getTruthy)(void* variable);
        bool (*getString)(void* variable, char* target);
        long long (*getStringLength)(void* variable);
        bool (*getInteger)(void* variable, long long* target);
        bool (*getFloat)(void* variable, double* target);
        bool (*getDictionaryVariable)(void* variable, const char* key, void** target);
        bool (*getArrayVariable)(void* variable, size_t idx, void** target);
        bool (*iterate)(void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse);
        long long (*getArraySize)(void* variable);
        void* (*setDictionaryVariable)(void* variable, const char* key, void* target);
        void* (*setArrayVariable)(void* variable, size_t idx, void* target);
        void* (*createHash)();
        void* (*createArray)();
        void* (*createFloat)(double value);
        void* (*createBool)(bool value);
        void* (*createInteger)(long long value);
        void* (*createString)(const char* str);
        void* (*createPointer)(void* value);
        void* (*createNil)();
        void* (*createClone)(void* value);
        void (*freeVariable)(void* value);
        int (*compare)(void* a, void* b);
    };

    LiquidContext liquidCreateContext(ELiquidSettings settings);
    void liquidFreeContext(LiquidContext context);
    void liquidImplementStandardDialect(LiquidContext context);
    LiquidRenderer liquidCreateRenderer(LiquidContext context);
    void liquidFreeRenderer(LiquidRenderer context);

    void liquidFilterGetOperand(void* targetVariable, LiquidRenderer renderer, LiquidNode filter, void* variableStore);
    void liquidGetArgument(void* targetVariable, LiquidRenderer renderer, LiquidNode node, void* variableStore, int idx);
    typedef void* (*LiquidRenderFunction)(LiquidRenderer renderer, LiquidNode node, void* variableStore);

    // Passing -1 to min/maxArguments means no min or max.
    void liquidRegisterTag(LiquidContext context, const char* symbol, ETagType type, int minArguments, int maxArguments, LiquidRenderFunction renderFunction);
    void liquidRegisterFilter(LiquidContext context, const char* symbol, int minArguments, int maxArguments, LiquidRenderFunction renderFunction);
    void liquidRegisterOperator(LiquidContext context, const char* symbol, int priority, LiquidRenderFunction renderFunction);
    void liquidRegisterVariableResolver(LiquidContext context, LiquidVariableResolver resolver);

    LiquidTemplate liquidCreateTemplate(LiquidContext context, const char* buffer, size_t size);
    void liquidFreeTemplate(LiquidTemplate tmpl);

    LiquidTemplateRender liquidRenderTemplate(LiquidRenderer renderer, void* variableStore, LiquidTemplate tmpl);
    void liquidFreeTemplateRender(LiquidTemplateRender render);

    const char* liquidTemplateRenderGetBuffer(LiquidTemplateRender render);
    size_t liquidTemplateRenderGetSize(LiquidTemplateRender render);

    const char* liquidGetError();
    void liquidClearError();
    void liquidSetError(const char* message);

#ifdef __cplusplus
}
#endif

#endif
