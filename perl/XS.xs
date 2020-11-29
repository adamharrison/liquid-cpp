#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "../src/liquid.h"

LiquidVariableType lpGetType(void* variable) {
    if (SvROK((SV*)variable)) {
        if (SvTYPE(SvRV((SV*)variable)) == SVt_PVAV)
            return LIQUID_VARIABLE_TYPE_ARRAY;
        if (SvTYPE(SvRV((SV*)variable)) == SVt_PVHV)
            return LIQUID_VARIABLE_TYPE_DICTIONARY;
        return LIQUID_VARIABLE_TYPE_OTHER;
    } else {
        if (SvIOKp((SV*)variable))
            return LIQUID_VARIABLE_TYPE_INT;
        if (SvNOKp((SV*)variable))
            return LIQUID_VARIABLE_TYPE_FLOAT;
        if (SvPOKp((SV*)variable))
            return LIQUID_VARIABLE_TYPE_STRING;
    }
    return LIQUID_VARIABLE_TYPE_NIL;
}

bool lpGetBool(void* variable, bool* target) {
    dTHX;
    *target = SvTRUE((SV*)variable);
    return true;
}

bool lpGetTruthy(void* variable) {
    dTHX;
    return SvTRUE((SV*)variable);
}

bool lpGetString(void* variable, char* target) {
    dTHX;
    STRLEN len;
    char* ptr;
    SV* sv = (SV*)variable;
    if (SvROK(sv)) {
        if (sv_isobject(sv)) {
            ptr = SvPV(sv, len);
        } else {
            ptr = SvPV(SvRV(sv), len);
        }
    } else
        ptr = SvPV(sv, len);
    memcpy(target, ptr, len);
    target[len] = 0;
    return true;
}

long long lpGetStringLength(void* variable) {
    dTHX;
    STRLEN len;
    char* ptr;
    SV* sv = (SV*)variable;
    if (SvROK(sv)) {
        if (sv_isobject(sv)) {
            ptr = SvPV(sv, len);
        } else {
            ptr = SvPV(SvRV(sv), len);
        }
    } else
        ptr = SvPV(sv, len);
    return len;
}

bool lpGetInteger(void* variable, long long* target) {
    dTHX;
    *target = (long long)SvNV((SV*)variable);
    return true;
}

bool lpGetFloat(void* variable, double* target) {
    dTHX;
    *target = SvNV((SV*)variable);
    return false;
}

bool lpGetDictionaryVariable(void* variable, const char* key, void** target) {
    dTHX;
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVHV)
        return false;
    HV* hv = (HV*)SvRV((SV*)variable);
    SV** retrieved = hv_fetch(hv, key, strlen(key), 0);
    if (!retrieved)
        return false;
    *target = *retrieved;
    return true;
}

bool lpGetArrayVariable(void* variable, size_t idx, void** target) {
    dTHX;
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVAV)
        return false;
    AV* av = (AV*)SvRV((SV*)variable);
    SV** sv = av_fetch(av, idx, 0);
    if (!sv)
        return false;
    *target = *sv;
    return true;
}

bool lpIterate(void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse) {
    dTHX;
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVAV)
        return false;
    AV* av = (AV*)SvRV((SV*)variable);
    int length = av_top_index(av)+1;
    if (length > limit)
        length = limit;
    length = length - 1;
    if (length < 0)
        length += av_top_index(av)+1;

    if (reverse) {
        start = av_top_index(av) - start;
        for (size_t i = length; i >= start; --i) {
            SV** sv = av_fetch(av, i, 0);
            if (sv)
                callback(*sv, data);
        }
    } else {
        for (size_t i = start; i <= length; ++i) {
            SV** sv = av_fetch(av, i, 0);
            if (sv)
                callback(*sv, data);
        }
    }
    return false;
}

long long lpGetArraySize(void* variable) {
    dTHX;
    if (SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVAV)
        return false;
    AV* av = (AV*)SvRV((SV*)variable);
    return av_top_index(av)+1;
}

void* lpSetDictionaryVariable(LiquidRenderer renderer, void* variable, const char* key, void* target) {
    dTHX;
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVHV)
        return NULL;
    HV* hv = (HV*)SvRV((SV*)variable);
    SV** retrieved = hv_store(hv, key, strlen(key), target, 0);
    if (!retrieved)
        return NULL;
    return *retrieved;
}

void* lpSetArrayVariable(LiquidRenderer renderer, void* variable, size_t idx, void* target) {
    dTHX;
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVAV)
        return NULL;
    AV* av = (AV*)SvRV((SV*)variable);
    SV** retrieved = av_store(av, idx, target);
    if (!retrieved)
        return NULL;
    return *retrieved;
}

void* lpCreateHash(LiquidRenderer renderer) {
    dTHX;
    return (void*)newRV_inc((SV*)newHV());
}

void* lpCreateArray(LiquidRenderer renderer) {
    dTHX;
    return (void*)newRV_inc((SV*)newAV());
}

void* lpCreateFloat(LiquidRenderer renderer, double value) {
    dTHX;
    return (void*)newSVnv(value);
}

void* lpCreateBool(LiquidRenderer renderer, bool value) {
    dTHX;
    return (void*)newSViv(value ? 1 : 0);
}

void* lpCreateInteger(LiquidRenderer renderer, long long value) {
    dTHX;
    return (void*)newSViv(value);
}

void* lpCreateString(LiquidRenderer renderer, const char* str) {
    dTHX;
    return (void*)newSVpvn(str,strlen(str));
}

void* lpCreatePointer(LiquidRenderer renderer, void* value) {
    dTHX;
    return (void*)newSV(0);
}

void* lpCreateNil(LiquidRenderer renderer) {
    dTHX;
    return (void*)newSV(0);
}

void* lpCreateClone(LiquidRenderer renderer, void* value) {
    dTHX;
    return newSVsv(value);
}

void lpFreeVariable(LiquidRenderer renderer, void* value) {
    dTHX;

}

int lpCompare(void* a, void* b) {
    dTHX;
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
createTemplate(context, str, error)
    void* context;
    char* str;
    SV* error;
    CODE:
        LiquidParserError parserError;
        RETVAL = liquidCreateTemplate(*(LiquidContext*)&context, str, strlen(str), &parserError).ast;
        if (SvROK(error)) {
            if (parserError.type) {
                AV* av = (AV*)SvRV(error);
                av_push(av, newSVnv(parserError.type));
                av_push(av, newSVnv(parserError.row));
                av_push(av, newSVnv(parserError.column));
                av_push(av, newSVpvn(parserError.message, strlen(parserError.message)));
            }
        }
    OUTPUT:
        RETVAL

void
freeTemplate(tmpl)
    void* tmpl;
    CODE:
        liquidFreeTemplate(*(LiquidTemplate*)&tmpl);

SV*
renderTemplate(renderer, store, tmpl, error)
    void* renderer;
    SV* store;
    void* tmpl;
    SV* error;
    CODE:
        LiquidRenderError renderError;
        LiquidTemplateRender render = liquidRenderTemplate(*(LiquidRenderer*)&renderer, store, *(LiquidTemplate*)&tmpl, &renderError);
        if (SvROK(error)) {
            if (renderError.type) {
                AV* av = (AV*)SvRV(error);
                av_push(av, newSVnv(renderError.type));
                av_push(av, newSVnv(renderError.row));
                av_push(av, newSVnv(renderError.column));
                av_push(av, newSVpvn(renderError.message, strlen(renderError.message)));
            }
        }
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
