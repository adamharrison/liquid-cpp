#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "../src/liquid.h"

LiquidVariableType lpGetType(void* variable) {
    return LIQUID_VARIABLE_TYPE_NIL;
}

bool lpGetBool(void* variable, bool* target) {
    return false;
}

bool lpGetTruthy(void* variable) {
    return false;
}

bool lpGetString(void* variable, char* target) {
    return false;
}

long long lpGetStringLength(void* variable) {
    return 0;
}

bool lpGetInteger(void* variable, long long* target) {
    return false;
}

bool lpGetFloat(void* variable, double* target) {
    return false;
}

bool lpGetDictionaryVariable(void* variable, const char* key, void** target) {
    return false;
}

bool lpGetArrayVariable(void* variable, size_t idx, void** target) {
    return false;
}

bool lpIterate(void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse) {
    return false;
}

long long lpGetArraySize(void* variable) {
    return -1;
}

void* lpSetDictionaryVariable(LiquidRenderer renderer, void* variable, const char* key, void* target) {
    return NULL;
}

void* lpSetArrayVariable(LiquidRenderer renderer, void* variable, size_t idx, void* target) {
    return NULL;
}

void* lpCreateHash(LiquidRenderer renderer) {
    return NULL;
}

void* lpCreateArray(LiquidRenderer renderer) {
    return NULL;
}

void* lpCreateFloat(LiquidRenderer renderer, double value) {
    return NULL;
}

void* lpCreateBool(LiquidRenderer renderer, bool value) {
    return NULL;
}

void* lpCreateInteger(LiquidRenderer renderer, long long value) {
    return NULL;
}

void* lpCreateString(LiquidRenderer renderer, const char* str) {
    return NULL;
}

void* lpCreatePointer(LiquidRenderer renderer, void* value) {
    return NULL;
}

void* lpCreateNil(LiquidRenderer renderer) {
    return NULL;
}

void* lpCreateClone(LiquidRenderer renderer, void* value) {
    return NULL;
}

void lpFreeVariable(LiquidRenderer renderer, void* value) {
;
}

int lpCompare(void* a, void* b) {
    return 0;
}


MODULE = WWW::Shopify::Liquid::XS		PACKAGE = WWW::Shopify::Liquid::XS

void*
createContext()
    CODE:
        LiquidContext context = liquidCreateContext(LIQUID_CONTEXT_SETTINGS_DEFAULT);
        LiquidVariableResolver resolver = {
            lpGetType,
            lpGetBool,
            lpGetTruthy,
            lpGetString,
            lpGetStringLength,
            lpGetInteger,
            lpGetFloat,
            lpGetDictionaryVariable,
            lpGetArrayVariable,
            lpIterate,
            lpGetArraySize,
            lpSetDictionaryVariable,
            lpSetArrayVariable,
            lpCreateHash,
            lpCreateArray,
            lpCreateFloat,
            lpCreateBool,
            lpCreateInteger,
            lpCreateString,
            lpCreatePointer,
            lpCreateNil,
            lpCreateClone,
            lpFreeVariable,
            lpCompare
        };
        liquidRegisterVariableResolver(context, resolver);
        RETVAL = context.context;
    OUTPUT:
        RETVAL

void
freeContext(context)
    void* context;
    CODE:
        liquidFreeContext(*(LiquidContext*)&context);

void
implementStandardDialect(context)
    void* context;
    CODE:
        liquidImplementStandardDialect(*(LiquidContext*)&context);

void*
createRenderer(context)
    void* context;
    CODE:
        RETVAL = liquidCreateRenderer(*(LiquidContext*)&context).renderer;
    OUTPUT:
        RETVAL

void
freeRenderer(renderer)
    void* renderer;
    CODE:
        liquidFreeRenderer(*(LiquidRenderer*)&renderer);

void*
createTemplate(context, str)
    void* context;
    char* str;
    CODE:
        RETVAL = liquidCreateTemplate(*(LiquidContext*)&context, str, strlen(str)).ast;
    OUTPUT:
        RETVAL

void
freeTemplate(tmpl)
    void* tmpl;
    CODE:
        liquidFreeTemplate(*(LiquidTemplate*)&tmpl);

SV*
renderTemplate(renderer, store, tmpl)
    void* renderer;
    SV* store;
    void* tmpl;
    CODE:
        LiquidTemplateRender render = liquidRenderTemplate(*(LiquidRenderer*)&renderer, store, *(LiquidTemplate*)&tmpl);
        RETVAL = newSVpvn(liquidTemplateRenderGetBuffer(render), liquidTemplateRenderGetSize(render));
        liquidFreeTemplateRender(render);
    OUTPUT:
        RETVAL

SV*
getError()
    CODE:
        const char* error = liquidGetError();
        if (error)
            RETVAL = newSVpvn(error, strlen(error));
        else
            RETVAL = newSV(0);
    OUTPUT:
        RETVAL
