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
    if (!SvROK((SV*)variable) || SvTYPE(SvRV((SV*)variable)) != SVt_PVAV)
        return -1;
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
    SvREFCNT_inc((SV*)value);
}

int lpCompare(void* a, void* b) {
    dTHX;
    return sv_cmp((SV*)a, (SV*)b);
}

void lpSetReturnValue(PerlInterpreter* my_perl, LiquidRenderer renderer, SV* sv) {
    switch (lpGetType((void*)sv)) {
        case LIQUID_VARIABLE_TYPE_STRING: {
            STRLEN len;
            const char *s = SvPV(sv, len);
            liquidRendererSetReturnValueString(renderer, s, len);
        } break;
        case LIQUID_VARIABLE_TYPE_BOOL: {
            bool b;
            lpGetBool(sv, &b);
            liquidRendererSetReturnValueBool(renderer, b);
        } break;
        case LIQUID_VARIABLE_TYPE_NIL: {
            liquidRendererSetReturnValueNil(renderer);
        } break;
        case LIQUID_VARIABLE_TYPE_FLOAT: {
            double f;
            lpGetFloat(sv, &f);
            liquidRendererSetReturnValueBool(renderer, f);
        } break;
        case LIQUID_VARIABLE_TYPE_INT: {
            long long i;
            lpGetInteger(sv, &i);
            liquidRendererSetReturnValueInteger(renderer, i);
        } break;
        default:{
            liquidRendererSetReturnValueVariable(renderer, SvREFCNT_inc(sv));
        } break;
    }
}

static void lpRenderEnclosingTag(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    dTHX;
    SV* callback = (SV*)data;

    dSP;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

    EXTEND(SP, 4);
    // Wrapped closure should look like:
    // ($package, $renderer, $hash)
    // the only thing we supply is the render, node, and hash.
    PUSHs(sv_2mortal(newSViv(PTR2IV(renderer.renderer))));
    PUSHs(sv_2mortal(newSViv(PTR2IV(node.node))));
    PUSHs((SV*)variableStore);

    SV* child = NULL;
    liquidGetChild((void**)&child, renderer, node, variableStore, 1);
    PUSHs(child);


    PUTBACK;

    int count = call_sv(SvRV(callback), G_SCALAR);

    SPAGAIN;

    if (count > 0) {
        SV *sv = POPs;
        lpSetReturnValue(my_perl, renderer, sv);
    } else {
        liquidRendererSetReturnValueNil(renderer);
    }

    FREETMPS;
    LEAVE;
}


static void lpRenderFreeTag(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    dTHX;
    SV* callback = (SV*)data;

    dSP;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

    EXTEND(SP, 3);
    // Wrapped closure should look like:
    // ($package, $renderer, $hash)
    // the only thing we supply is the render, node, and hash.
    PUSHs(sv_2mortal(newSViv(PTR2IV(renderer.renderer))));
    PUSHs(sv_2mortal(newSViv(PTR2IV(node.node))));
    PUSHs((SV*)variableStore);

    PUTBACK;

    int count = call_sv(SvRV(callback), G_SCALAR);

    SPAGAIN;

    if (count > 0) {
        SV *sv = POPs;
        lpSetReturnValue(my_perl, renderer, sv);
    } else {
        liquidRendererSetReturnValueNil(renderer);
    }

    FREETMPS;
    LEAVE;
}



static void lpRenderFilter(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    dTHX;
    SV* callback = (SV*)data;


    dSP;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);


    int argMax = liquidGetArgumentCount(node);

    EXTEND(SP, 4+argMax);
    // Wrapped closure should look like:
    // ($package, $renderer, $hash)
    // the only thing we supply is the render, node, and hash.
    PUSHs(sv_2mortal(newSViv(PTR2IV(renderer.renderer))));
    PUSHs(sv_2mortal(newSViv(PTR2IV(node.node))));
    PUSHs((SV*)variableStore);

    SV* operand = NULL;
    liquidFilterGetOperand((void**)&operand, renderer, node, variableStore);
    PUSHs(operand);


    for (int i = 0; i < argMax; ++i) {
        SV* arg = NULL;
        liquidGetArgument((void**)&arg, renderer, node, variableStore, i);
        PUSHs(arg);
    }

    PUTBACK;

    int count = call_sv(callback, G_SCALAR);

    SPAGAIN;

    if (count > 0) {
        SV *sv = POPs;
        lpSetReturnValue(my_perl, renderer, sv);
    } else {
        liquidRendererSetReturnValueNil(renderer);
    }

    FREETMPS;
    LEAVE;
}


static void lpRenderBinaryInfixOperator(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    dTHX;
    SV* callback = (SV*)data;
    SV* op1 = NULL;
    SV* op2 = NULL;

    liquidGetChild((void**)&op1, renderer, node, variableStore, 0);
    liquidGetChild((void**)&op2, renderer, node, variableStore, 1);


    dSP;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

    EXTEND(SP, 5);
    // Wrapped closure should look like:
    // ($package, $renderer, $hash)
    // the only thing we supply is the render, node, and hash.
    PUSHs(sv_2mortal(newSViv(PTR2IV(renderer.renderer))));
    PUSHs(sv_2mortal(newSViv(PTR2IV(node.node))));
    PUSHs((SV*)variableStore);
    PUSHs(sv_2mortal((SV*)op1));
    PUSHs(sv_2mortal((SV*)op2));
    PUTBACK;

    int count = call_sv(callback, G_ARRAY);

    SPAGAIN;

    if (count > 0) {
        SV *sv = POPs;
        lpSetReturnValue(my_perl, renderer, sv);
    } else {
        liquidRendererSetReturnValueNil(renderer);
    }

    FREETMPS;
    LEAVE;
}

MODULE = WWW::Shopify::Liquid::XS		PACKAGE = WWW::Shopify::Liquid::XS

void*
createContext()
    CODE:
        LiquidContext context = liquidCreateContext();
        RETVAL = context.context;
    OUTPUT:
        RETVAL

void
freeContext(context)
    void* context;
    CODE:
        liquidFreeContext(*(LiquidContext*)&context);

void
implementPermissiveStandardDialect(context)
    void* context;
    CODE:
        liquidImplementPermissiveStandardDialect(*(LiquidContext*)&context);

void
implementStrictStandardDialect(context)
    void* context;
    CODE:
        liquidImplementStrictStandardDialect(*(LiquidContext*)&context);

void*
createRenderer(context)
    void* context;
    CODE:
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
        LiquidRenderer renderer = liquidCreateRenderer(*(LiquidContext*)&context);
        liquidRegisterVariableResolver(renderer, resolver);
        RETVAL = renderer.renderer;
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
                RETVAL = NULL;
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

int
registerTag(context, symbol, type, minArguments, maxArguments, renderFunction)
    void* context;
    const char* symbol;
    const char* type;
    int minArguments;
    int maxArguments;
    SV* renderFunction;
    CODE:
        enum ETagType eType = strcmp(type, "free") == 0 ? LIQUID_TAG_TYPE_FREE : LIQUID_TAG_TYPE_ENCLOSING;
        if (SvROK(renderFunction) && SvTYPE(SvRV(renderFunction)) == SVt_PVCV) {
            liquidRegisterTag(*(LiquidContext*)&context, symbol, eType, minArguments, maxArguments, (eType == LIQUID_TAG_TYPE_FREE ? lpRenderFreeTag : lpRenderEnclosingTag), SvREFCNT_inc(renderFunction));
            RETVAL = 0;
        } else {
            RETVAL = -1;
        }
    OUTPUT:
        RETVAL

int
registerFilter(context, symbol, minArguments, maxArguments, renderFunction)
    void* context;
    const char* symbol;
    int minArguments;
    int maxArguments;
    SV* renderFunction;
    CODE:
        if (SvROK(renderFunction) && SvTYPE(SvRV(renderFunction)) == SVt_PVCV) {
            liquidRegisterFilter(*(LiquidContext*)&context, symbol, minArguments, maxArguments, lpRenderFilter, SvREFCNT_inc(renderFunction));
            RETVAL = 0;
        } else {
            RETVAL = -1;
        }
    OUTPUT:
        RETVAL

int
registerOperator(context, symbol, arity, fixness, priority, renderFunction)
    void* context;
    const char* symbol;
    const char* arity;
    const char* fixness;
    int priority;
    SV* renderFunction;
    CODE:
        if (SvROK(renderFunction) && SvTYPE(SvRV(renderFunction)) == SVt_PVCV) {
            LiquidOperatorArity eArity = strcmp(arity, "unary") == 0 ? LIQUID_OPERATOR_ARITY_UNARY : (strcmp(arity, "binary") == 0 ? LIQUID_OPERATOR_ARITY_BINARY : (strcmp(arity, "nonary") == 0 ? LIQUID_OPERATOR_ARITY_NONARY : LIQUID_OPERATOR_ARITY_NARY));
            LiquidOperatorFixness eFixness = (strcmp(fixness, "prefix") == 0 ? LIQUID_OPERATOR_FIXNESS_PREFIX : (strcmp(fixness, "affix") == 0 ? LIQUID_OPERATOR_FIXNESS_AFFIX : LIQUID_OPERATOR_FIXNESS_INFIX));
            assert(eArity == LIQUID_OPERATOR_ARITY_BINARY);
            assert(eFixness == LIQUID_OPERATOR_FIXNESS_INFIX);
            liquidRegisterOperator(*(LiquidContext*)&context, symbol, eArity, eFixness, priority, lpRenderBinaryInfixOperator, SvREFCNT_inc(renderFunction));
            RETVAL = 0;
        } else {
            RETVAL = -1;
        }
    OUTPUT:
        RETVAL
