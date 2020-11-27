#include <ruby.h>
#include "../src/interface.h"

VALUE liquidC = Qnil;


static LiquidVariableType liquidCgetType(void* variable) {
    switch (TYPE(variable)) {
        case T_FIXNUM:
        case T_BIGNUM:
            return LIQUID_VARIABLE_TYPE_INT;
        case T_STRING:
            return LIQUID_VARIABLE_TYPE_STRING;
        case T_ARRAY:
            return LIQUID_VARIABLE_TYPE_ARRAY;
        case T_HASH:
            return LIQUID_VARIABLE_TYPE_DICTIONARY;
        case T_FLOAT:
            return LIQUID_VARIABLE_TYPE_FLOAT;
        case T_TRUE:
        case T_FALSE:
            return LIQUID_VARIABLE_TYPE_BOOL;
        case T_NIL:
        case T_UNDEF:
            return LIQUID_VARIABLE_TYPE_NIL;
        default:
            return LIQUID_VARIABLE_TYPE_OTHER;
    }
}


static bool liquidCgetBool(void* variable, bool* target) {
    switch (TYPE(variable)) {
        case T_TRUE:
        case T_FALSE:
            *target = RTEST(variable);
            return true;
        default:
            return false;
    }

}

static bool liquidCgetTruthy(void* variable) {
    return RTEST(variable);
}

static bool liquidCgetString(void* variable, char* target) {
    VALUE value;
    char* str;
    value = (VALUE)variable;
    if (TYPE(value) != T_STRING)
        return false;
    str = StringValueCStr(value);
    strcpy(target, str);
    return true;
}

static long long liquidCgetStringLength(void* variable) {
    return RSTRING_LEN((VALUE)variable);
}

static bool liquidCgetInteger(void* variable, long long* target) {
    VALUE value;
    value = (VALUE)variable;
    switch (TYPE(value)) {
        case T_FIXNUM:
        case T_BIGNUM:
        case T_FLOAT:
            *target = NUM2LL(value);
            return true;
    }
    return false;
}

static bool liquidCgetFloat(void* variable, double* target) {
    VALUE value;
    value = (VALUE)variable;
    switch (TYPE(value)) {
        case T_FIXNUM:
        case T_BIGNUM:
        case T_FLOAT:
            *target = NUM2DBL(value);
            return true;
    }
    return false;
}

static bool liquidCgetDictionaryVariable(void* variable, const char* key, void** target) {
    VALUE value;
    if (TYPE((VALUE)variable) != T_HASH)
        return false;
    value = rb_hash_aref((VALUE)variable, rb_str_new2(key));
    if (TYPE(value) == T_UNDEF)
        return false;
    *target = (void*)value;
    return true;
}

static bool liquidCgetArrayVariable(void* variable, size_t idx, void** target) {
    VALUE value;
	if (TYPE((VALUE)variable) != T_ARRAY)
        return false;
    value = rb_ary_entry((VALUE)variable, idx);
    if (TYPE((VALUE)value) == T_UNDEF)
        return false;
    *target = (void*)value;
    return true;
}

static bool liquidCiterate(void* variable, bool liquidCcallback(void* variable, void* data), void* data, int start, int limit, bool reverse) {
    return false;
}

static long long liquidCgetArraySize(void* variable) {
    VALUE value = (VALUE)variable;
    if (TYPE(value) != T_ARRAY)
        return -1;
    return RARRAY_LEN(value);
}

static void* liquidCsetDictionaryVariable(LiquidRenderer renderer, void* variable, const char* key, void* target) {
    if (TYPE((VALUE)variable) != T_HASH)
        return NULL;
    rb_hash_aset((VALUE)variable, rb_str_new2(key), (VALUE)target);
    return target;
}

static void* liquidCsetArrayVariable(LiquidRenderer renderer, void* variable, size_t idx, void* target) {
    if (TYPE(variable) != T_ARRAY)
        return NULL;
    rb_ary_store((VALUE)variable, idx, (VALUE)target);
    return target;
}

static void* liquidCcreateHash(LiquidRenderer renderer) {
    return (void*)rb_hash_new();
}

static void* liquidCcreateArray(LiquidRenderer renderer) {
    return (void*)rb_ary_new();
}

static void* liquidCcreateFloat(LiquidRenderer renderer, double value) {
    return (void*)DBL2NUM(value);
}
static void* liquidCcreateBool(LiquidRenderer renderer, bool value) {
    return (void*)Qtrue;
}
static void* liquidCcreateInteger(LiquidRenderer renderer, long long value) {
    return (void*)Qfalse;
}
static void* liquidCcreateString(LiquidRenderer renderer, const char* str) {
    return (void*)rb_str_new2(str);
}
static void* liquidCcreatePointer(LiquidRenderer renderer, void* value) {
    return NULL;
}
static void* liquidCcreateNil(LiquidRenderer renderer) {
    return (void*)Qnil;
}
static void* liquidCcreateClone(LiquidRenderer renderer, void* value) {
    return (void*)value;
}
static void liquidCfreeVariable(LiquidRenderer renderer, void* value) {

}
static int liquidCcompare(LiquidRenderer renderer, void* a, void* b) {
    VALUE result;
    result = rb_funcall((VALUE)a, rb_intern("<=>"), 1, (VALUE)b);
    return NUM2INT(result);
}


void liquidC_free(void* data) {
    if (((LiquidContext*)data)->context)
        liquidFreeContext(*((LiquidContext*)data));
    free(data);
}

size_t liquidC_size(const void* data) {
    return sizeof(LiquidContext);
}


static const rb_data_type_t liquidC_type = {
    .wrap_struct_name = "LiquidC",
    .function = {
            .dmark = NULL,
            .dfree = liquidC_free,
            .dsize = liquidC_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};


VALUE liquidC_alloc(VALUE self) {
    LiquidContext* data = (LiquidContext*)malloc(sizeof(LiquidContext));
    data->context = NULL;
    return TypedData_Wrap_Struct(self, &liquidC_type, data);
}

VALUE liquidGlobalContext;

VALUE liquidC_m_initialize(VALUE self) {
        LiquidContext* context;
        LiquidVariableResolver resolver = {
            liquidCgetType,
            liquidCgetBool,
            liquidCgetTruthy,
            liquidCgetString,
            liquidCgetStringLength,
            liquidCgetInteger,
            liquidCgetFloat,
            liquidCgetDictionaryVariable,
            liquidCgetArrayVariable,
            liquidCiterate,
            liquidCgetArraySize,
            liquidCsetDictionaryVariable,
            liquidCsetArrayVariable,
            liquidCcreateHash,
            liquidCcreateArray,
            liquidCcreateFloat,
            liquidCcreateBool,
            liquidCcreateInteger,
            liquidCcreateString,
            liquidCcreatePointer,
            liquidCcreateNil,
            liquidCcreateClone,
            liquidCfreeVariable,
            liquidCcompare
        };
        TypedData_Get_Struct(self, LiquidContext, &liquidC_type, context);
        *context = liquidCreateContext(LIQUID_CONTEXT_SETTINGS_DEFAULT);
        if (liquidGetError()) {
            rb_raise(rb_eTypeError, "LiquidC init failure: %s.", liquidGetError());
            return self;
        }
        liquidRegisterVariableResolver(*context, resolver);
        liquidImplementStandardDialect(*context);

        liquidGlobalContext = self;

        return self;
}

VALUE liquidC_registerTag(VALUE self, VALUE name) {
    return Qnil;
}

VALUE liquidC_registerFilter(VALUE self, VALUE name) {
    return Qnil;
}

VALUE liquidC_registerOperator(VALUE self, VALUE name) {
    return Qnil;
}

VALUE liquidC_getSingleton(VALUE klass, VALUE name) {
    return liquidGlobalContext;
}



void liquidCRenderer_free(void* data) {
    if (((LiquidRenderer*)data)->renderer)
        liquidFreeRenderer(*((LiquidRenderer*)data));
    free(data);
}

size_t liquidCRenderer_size(const void* data) {
    return sizeof(LiquidRenderer);
}


static const rb_data_type_t liquidCRenderer_type = {
    .wrap_struct_name = "Renderer",
    .function = {
            .dmark = NULL,
            .dfree = liquidCRenderer_free,
            .dsize = liquidCRenderer_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};


VALUE liquidCRenderer_alloc(VALUE self) {
    LiquidRenderer* data = (LiquidRenderer*)malloc(sizeof(LiquidRenderer));
    data->renderer = NULL;
    return TypedData_Wrap_Struct(self, &liquidCRenderer_type, data);
}


VALUE liquidCRenderer_m_initialize(VALUE self, VALUE contextValue) {
    LiquidRenderer* renderer;
    LiquidContext* context;
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, renderer);
    TypedData_Get_Struct(contextValue, LiquidContext, &liquidC_type, context);
    *renderer = liquidCreateRenderer(*context);
    if (liquidGetError()) {
        rb_raise(rb_eTypeError, "LiquidC Renderer init failure: %s.", liquidGetError());
        return self;
    }
    return self;
}



void liquidCTemplate_free(void* data) {
    if (((LiquidTemplate*)data)->ast)
        liquidFreeTemplate(*((LiquidTemplate*)data));
    free(data);
}

size_t liquidCTemplate_size(const void* data) {
    return sizeof(LiquidTemplate);
}


static const rb_data_type_t liquidCTemplate_type = {
    .wrap_struct_name = "Template",
    .function = {
            .dmark = NULL,
            .dfree = liquidCTemplate_free,
            .dsize = liquidCTemplate_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};


VALUE liquidCTemplate_alloc(VALUE self) {
    LiquidTemplate* data = (LiquidTemplate*)malloc(sizeof(LiquidTemplate));
    data->ast = NULL;
    return TypedData_Wrap_Struct(self, &liquidCTemplate_type, data);
}

VALUE liquidCTemplate_m_initialize(VALUE self, VALUE contextValue, VALUE tmplContents) {
    char* tmplBody;
    size_t length;
    LiquidTemplate* tmpl;
    LiquidContext* context;
    TypedData_Get_Struct(self, LiquidTemplate, &liquidCTemplate_type, tmpl);
    TypedData_Get_Struct(contextValue, LiquidContext, &liquidC_type, context);

    Check_Type(tmplContents, T_STRING);
    tmplBody = StringValueCStr(tmplContents);
    length = RSTRING_LEN(tmplContents);

    *tmpl = liquidCreateTemplate(*context, tmplBody, length);

    if (liquidGetError()) {
        rb_raise(rb_eTypeError, "LiquidC Template init failure: %s.", liquidGetError());
        return self;
    }
    return self;
}



VALUE method_liquidCTemplateRender(VALUE self, VALUE stash, VALUE tmpl) {
    VALUE str;
    LiquidTemplate* liquidTemplate;
    LiquidRenderer* liquidRenderer;
    LiquidTemplateRender result;
    Check_Type(stash, T_HASH);
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, liquidRenderer);
    TypedData_Get_Struct(tmpl, LiquidTemplate, &liquidCTemplate_type, liquidTemplate);
    result = liquidRenderTemplate(*liquidRenderer, (void*)stash, *liquidTemplate);
    str = rb_str_new(liquidTemplateRenderGetBuffer(result), liquidTemplateRenderGetSize(result));
    liquidFreeTemplateRender(result);
    return str;
}

/* Drop in replacement for reuglar liquid. */
VALUE liquidC_DIR(VALUE klass) {
	VALUE liquid, tmpl;
	liquid = rb_define_module("Liquid");
	tmpl = rb_define_class_under(liquid, "Tempalte", rb_cData);
}


void Init_LiquidC() {
	VALUE liquidC, liquidCRenderer, liquidCTemplate;
	liquidC = rb_define_class("LiquidC", rb_cData);
	liquidCRenderer = rb_define_class_under(liquidC, "Renderer", rb_cData);
	liquidCTemplate = rb_define_class_under(liquidC, "Template", rb_cData);

	rb_define_alloc_func(liquidC, liquidC_alloc);
	rb_define_method(liquidC, "initialize", liquidC_m_initialize, 0);
	rb_define_method(liquidC, "registerTag", liquidC_registerTag, 1);
	rb_define_method(liquidC, "registerFilter", liquidC_registerFilter, 1);
    rb_define_method(liquidC, "registerOperator", liquidC_registerOperator, 1);
    // Specifically to conform to the insane Shopify paradigm.
	rb_define_singleton_method(liquidC, "getSingleton", liquidC_getSingleton, 0);
	rb_define_singleton_method(liquidC, "DIR", liquidC_DIR, 0);

	rb_define_alloc_func(liquidCRenderer, liquidCRenderer_alloc);
    rb_define_method(liquidCRenderer, "initialize", liquidCRenderer_m_initialize, 1);
    rb_define_method(liquidCRenderer, "render", method_liquidCTemplateRender, 2);

	rb_define_alloc_func(liquidCTemplate, liquidCTemplate_alloc);
	rb_define_method(liquidCTemplate, "initialize", liquidCTemplate_m_initialize, 2);
}
