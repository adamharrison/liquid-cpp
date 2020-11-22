#include <ruby.h>
#include "../src/interface.h"

VALUE liquidC = Qnil;

void liquidC_free(void* data) {
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
    return TypedData_Wrap_Struct(self, &liquidC_type, data);
}

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
    if (TYPE(variable) != T_STRING)
        return false;
    char* str = StringValueCStr(variable);
    strcpy(target, str);
    return true;
}
static long long liquidCgetStringLength(void* variable) {
    return RSTRING_LEN(variable);
}
static bool liquidCgetInteger(void* variable, long long* target) {
    switch (TYPE(variable)) {
        case T_FIXNUM:
        case T_BIGNUM:
        case T_FLOAT:
            *target = NUM2LL(variable);
            return true;
    }
    return false;
}
static bool liquidCgetFloat(void* variable, double* target) {
    switch (TYPE(variable)) {
        case T_FIXNUM:
        case T_BIGNUM:
        case T_FLOAT:
            *target = NUM2DBL(variable);
            return true;
    }
    return false;
}
static bool liquidCgetDictionaryVariable(void* variable, const char* key, bool createOnNotExists, void** target) {
    if (TYPE(variable) != T_HASH)
        return false
    VALUE value = rb_hash_aref(variable, rb_intern(key));
    if (TYPE(value) == T_UNDEFINED) {
        if (!createOnNotExists)
            return false;
        rb_hash_aset(
    } else {

    }
    return true;
}

static bool liquidCgetArrayVariable(void* variable, size_t idx, bool createOnNotExists, void** target) {

}

static bool liquidCiterate(void* variable, bool liquidCcallback(void* variable, void* data), void* data, int start, int limit, bool reverse) {

}

static long long liquidCgetArraySize(void* variable) {

}

static void liquidCsetVariable(void* variable, const void* value) {

}

static void liquidCsetFloat(void* variable, double value) {

}

static void liquidCsetBool(void* variable, bool value) {

}

static void liquidCsetInteger(void* variable, long long value) {

}

static void liquidCsetString(void* variable, const char* str) {

}

static void liquidCsetPointer(void* variable, void* value) {

}

static void liquidCsetNil(void* variable) {

}

static int liquidCcompare(void* a, void* b) {

}

VALUE liquidC_m_initialize(VALUE self, VALUE name) {
        LiquidContext context;
        TypedData_Get_Struct(self, void*, &liquidC_type, data);
        *data = liquidCreateContext(StringValueCStr(name));
        if (!*data) {
                rb_raise(rb_eTypeError, "SearchCore initalization error: %s", scGetError());
                return self;
        }

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
            liquidCliquidCcallback,
            liquidCgetArraySize,
            liquidCsetVariable,
            liquidCsetFloat,
            liquidCsetBool,
            liquidCsetInteger,
            liquidCsetString,
            liquidCsetPointer,
            liquidCsetNil,
            liquidCcompare,
        };

        liquidRegisterVariableResolver(context,resolver);

        return self;
}


VALUE liquidC_m_initialize(VALUE self) {
        void** data;
        TypedData_Get_Struct(self, void*, &liquidC_type, data);
        *data = scOpenReader(StringValueCStr(name));
        if (!*data) {
            rb_raise(rb_eTypeError, "SearchCore initalization error: %s", scGetError());
            return self;
        }
}


VALUE liquidC_renderer_m_initialize(VALUE self, VALUE context) {
        void** data;
        VALUE array;
        struct SCProductUpdate results;
        TypedData_Get_Struct(self, void*, &writer_type, data);
        results = scUpdateProducts(*data, StringValueCStr(productPath));
        if (results.updatedProducts == -1) {
                rb_raise(rb_eTypeError, "SearchCore updateProducts error: %s", scGetError());
                return -1;
        }
        array = rb_ary_new();
        rb_ary_push(array, INT2NUM(results.updatedProducts));
        rb_ary_push(array, INT2NUM(results.updatedVariants));
        return array;
}



VALUE liquidCRenderer_m_initialize(VALUE self, VALUE productPath) {
        void** data;
        VALUE array;
        struct SCProductUpdate results;
        TypedData_Get_Struct(self, void*, &liquidC_type, data);
        results = scUpdateProducts(*data, StringValueCStr(productPath));
        if (results.updatedProducts == -1) {
                rb_raise(rb_eTypeError, "SearchCore updateProducts error: %s", scGetError());
                return -1;
        }
        array = rb_ary_new();
        rb_ary_push(array, INT2NUM(results.updatedProducts));
        rb_ary_push(array, INT2NUM(results.updatedVariants));
        return array;
}


void Init_LiquidC() {
	VALUE liquidCRenderer, liquidCTemplate;
	liquidC = rb_define_module("LiquidC");
	liquidCRenderer = rb_define_class_under(LiquidC, "Renderer", rb_cData);
	liquidCTemplate = rb_define_class_under(LiquidC, "Template", rb_cData);

	rb_define_alloc_func(LiquidC, liquidC_alloc);
	rb_define_method(liquidC, "initialize", liquidC_m_initialize, 0);
	rb_define_method(liquidC, "registerTag", liquidC_registerTag, 1);
	rb_define_method(liquidC, "registerFilter", liquidC_registerFilter, 1);
    rb_define_method(liquidC, "registerOperator", liquidC_registerOperator, 1);
    // Specifically to conform to the insane Shopify paradigm.
	rb_define_singleton_method(liquidC, "getFirst", liquidC_getFirst, 0);

	rb_define_alloc_func(liquidCRenderer, liquidCRenderer_alloc);
    rb_define_method(liquidCRenderer, "initialize", liquidCRenderer_m_initialize, 1);
    rb_define_method(liquidCRenderer, "render", method_liquidCTemplateRender, 2);

	rb_define_alloc_func(liquidCTemplate, liquidCTemplate_alloc);
	rb_define_method(liquidCTemplate, "initialize", liquidCTemplate_m_initialize, 1);
}
