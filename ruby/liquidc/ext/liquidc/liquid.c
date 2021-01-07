#include <ruby.h>
#include <liquid/liquid.h>

// Global definitions.

static VALUE liquidCParserError, liquidCRendererError, liquidCTemplate;

struct SLiquidCRubyContext {
    LiquidContext context;
    // An array where we store registered functions, so they don't get garbage collected.
    VALUE registeredFunctionArray;
};
typedef struct SLiquidCRubyContext LiquidCRubyContext;

// Variable types.

static LiquidVariableType liquidCgetType(LiquidRenderer renderer, void* variable) {
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


static bool liquidCgetBool(LiquidRenderer renderer, void* variable, bool* target) {
    switch (TYPE(variable)) {
        case T_TRUE:
        case T_FALSE:
            *target = RTEST(variable);
            return true;
        default:
            return false;
    }

}

static bool liquidCgetTruthy(LiquidRenderer renderer, void* variable) {
    return RTEST(variable);
}

static bool liquidCgetString(LiquidRenderer renderer, void* variable, char* target) {
    VALUE value;
    char* str;
    value = (VALUE)variable;
    if (TYPE(value) != T_STRING)
        return false;
    str = StringValueCStr(value);
    strcpy(target, str);
    return true;
}

static long long liquidCgetStringLength(LiquidRenderer renderer, void* variable) {
    return RSTRING_LEN((VALUE)variable);
}

static bool liquidCgetInteger(LiquidRenderer renderer, void* variable, long long* target) {
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

static bool liquidCgetFloat(LiquidRenderer renderer, void* variable, double* target) {
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

static bool liquidCgetDictionaryVariable(LiquidRenderer renderer, void* variable, const char* key, void** target) {
    VALUE value;
    if (TYPE((VALUE)variable) != T_HASH)
        return false;
    value = rb_hash_aref((VALUE)variable, rb_str_new2(key));
    if (TYPE(value) == T_UNDEF)
        return false;
    *target = (void*)value;
    return true;
}

static bool liquidCgetArrayVariable(LiquidRenderer renderer, void* variable, long long idx, void** target) {
    VALUE value;
	if (TYPE((VALUE)variable) != T_ARRAY)
        return false;
    value = rb_ary_entry((VALUE)variable, idx);
    if (TYPE((VALUE)value) == T_UNDEF)
        return false;
    *target = (void*)value;
    return true;
}

static bool liquidCiterate(LiquidRenderer renderer, void* variable, bool liquidCcallback(void* variable, void* data), void* data, int start, int limit, bool reverse) {
    return false;
}

static long long liquidCgetArraySize(LiquidRenderer renderer, void* variable) {
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

static void* liquidCsetArrayVariable(LiquidRenderer renderer, void* variable, long long idx, void* target) {
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
static int liquidCcompare(void* a, void* b) {
    VALUE result;
    result = rb_funcall((VALUE)a, rb_intern("<=>"), 1, (VALUE)b);
    return NUM2INT(result);
}

// Type boilerplate.

void liquidC_free(void* data) {
    if (((LiquidCRubyContext*)data)->context.context)
        liquidFreeContext(((LiquidCRubyContext*)data)->context);
    free(data);
}
void liquidCParser_free(void* data) {
    if (((LiquidParser*)data)->parser)
        liquidFreeParser(*((LiquidParser*)data));
    free(data);
}
void liquidCRenderer_free(void* data) {
    if (((LiquidRenderer*)data)->renderer)
        liquidFreeRenderer(*((LiquidRenderer*)data));
    free(data);
}
void liquidCTemplate_free(void* data) {
    if (((LiquidTemplate*)data)->ast)
        liquidFreeTemplate(*((LiquidTemplate*)data));
    free(data);
}
void liquidCOptimizer_free(void* data) {
    if (((LiquidOptimizer*)data)->optimizer)
        liquidFreeOptimizer(*((LiquidOptimizer*)data));
    free(data);
}

void liquidC_mark(void* self) {
    rb_gc_mark(((LiquidCRubyContext*)self)->registeredFunctionArray);
}

size_t liquidCOptimizer_size(const void* data) { return sizeof(LiquidOptimizer); }
size_t liquidCTemplate_size(const void* data) { return sizeof(LiquidTemplate); }
size_t liquidC_size(const void* data) { return sizeof(LiquidCRubyContext); }
size_t liquidCParser_size(const void* data) { return sizeof(LiquidParser); }
size_t liquidCRenderer_size(const void* data) { return sizeof(LiquidRenderer); }


static const rb_data_type_t liquidC_type = {
    .wrap_struct_name = "LiquidC",
    .function = {
            .dmark = liquidC_mark,
            .dfree = liquidC_free,
            .dsize = liquidC_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

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

static const rb_data_type_t liquidCParser_type = {
    .wrap_struct_name = "Parser",
    .function = {
            .dmark = NULL,
            .dfree = liquidCParser_free,
            .dsize = liquidCParser_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

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
static const rb_data_type_t liquidCOptimizer_type = {
    .wrap_struct_name = "Optimizer",
    .function = {
            .dmark = NULL,
            .dfree = liquidCOptimizer_free,
            .dsize = liquidCOptimizer_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE liquidC_alloc(VALUE self) {
    LiquidCRubyContext* data = (LiquidCRubyContext*)malloc(sizeof(LiquidCRubyContext));
    data->context.context = NULL;
    data->registeredFunctionArray = rb_ary_new();
    return TypedData_Wrap_Struct(self, &liquidC_type, data);
}
VALUE liquidCParser_alloc(VALUE self) {
    LiquidParser* data = (LiquidParser*)malloc(sizeof(LiquidParser));
    data->parser = NULL;
    return TypedData_Wrap_Struct(self, &liquidCParser_type, data);
}
VALUE liquidCRenderer_alloc(VALUE self) {
    LiquidRenderer* data = (LiquidRenderer*)malloc(sizeof(LiquidRenderer));
    data->renderer = NULL;
    return TypedData_Wrap_Struct(self, &liquidCRenderer_type, data);
}
VALUE liquidCTemplate_alloc(VALUE self) {
    LiquidTemplate* data = (LiquidTemplate*)malloc(sizeof(LiquidTemplate));
    data->ast = NULL;
    return TypedData_Wrap_Struct(self, &liquidCTemplate_type, data);
}
VALUE liquidCOptimizer_alloc(VALUE self) {
    LiquidOptimizer* data = (LiquidOptimizer*)malloc(sizeof(LiquidOptimizer));
    data->optimizer = NULL;
    return TypedData_Wrap_Struct(self, &liquidCOptimizer_type, data);
}

// Actual implementations.
VALUE liquidC_m_initialize(int argc, VALUE* argv, VALUE self) {
    LiquidCRubyContext* context;
    char* str;
    TypedData_Get_Struct(self, LiquidCRubyContext, &liquidC_type, context);
    context->context = liquidCreateContext();
    if (argc > 0) {
        str = StringValueCStr(argv[0]);
        if (strcmp(str, "strict") == 0)
            liquidImplementStrictStandardDialect(context->context);
        else
            liquidImplementPermissiveStandardDialect(context->context);
    }
    else
        liquidImplementStrictStandardDialect(context->context);
    return self;
}

VALUE liquidCRenderer_m_initialize(VALUE self, VALUE contextValue) {
    LiquidRenderer* renderer;
    LiquidCRubyContext* context;
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
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, renderer);
    TypedData_Get_Struct(contextValue, LiquidCRubyContext, &liquidC_type, context);
    *renderer = liquidCreateRenderer(context->context);
    liquidRegisterVariableResolver(*renderer, resolver);
    return self;
}

VALUE liquidCRendererSetStrictVariables(VALUE self, VALUE strict) {
    LiquidRenderer* renderer;
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, renderer);
    liquidRendererSetStrictVariables(*renderer, RTEST(strict));
    return self;
}
VALUE liquidCRendererSetStrictFilters(VALUE self, VALUE strict) {
    LiquidRenderer* renderer;
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, renderer);
    liquidRendererSetStrictFilters(*renderer, RTEST(strict));
    return self;
}

VALUE liquidCParser_m_initialize(VALUE self, VALUE incomingContext) {
    LiquidParser* parser;
    LiquidCRubyContext* context;
    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);
    TypedData_Get_Struct(incomingContext, LiquidCRubyContext, &liquidC_type, context);
    *parser = liquidCreateParser(context->context);
    return self;
}

VALUE liquidCOptimizer_m_initialize(VALUE self, VALUE incomingRenderer) {
    LiquidOptimizer* optimizer;
    LiquidRenderer* renderer;
    TypedData_Get_Struct(self, LiquidOptimizer, &liquidCOptimizer_type, optimizer);
    TypedData_Get_Struct(incomingRenderer, LiquidRenderer, &liquidCRenderer_type, renderer);
    *optimizer = liquidCreateOptimizer(*renderer);
    return self;
}

#define PACK_EXCEPTION(rubytype, error, package, english, exception) \
    exception = rb_obj_alloc(rubytype);\
    package[0] = LL2NUM(error.type);\
    package[1] = rb_str_new2(error.details.message);\
    package[2] = rb_ary_new2(5);\
    rb_ary_push(package[2], error.details.args[0]);\
    rb_ary_push(package[2], error.details.args[1]);\
    rb_ary_push(package[2], error.details.args[2]);\
    rb_ary_push(package[2], error.details.args[3]);\
    rb_ary_push(package[2], error.details.args[4]);\
    package[3] = LL2NUM(error.details.line);\
    package[4] = LL2NUM(error.details.column);\
    package[5] = rb_str_new2(error.details.file);\
    package[6] = rb_str_new2(english);\
    rb_obj_call_init(exception, 7, package);

VALUE method_liquidCTemplateRender(VALUE self, VALUE stash, VALUE tmpl) {
    VALUE str, exceptionInit[7], exception;
    LiquidTemplate* liquidTemplate;
    LiquidRenderer* liquidRenderer;
    LiquidRendererError error;
    LiquidTemplateRender result;
    char buffer[512];

    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, liquidRenderer);
    TypedData_Get_Struct(tmpl, LiquidTemplate, &liquidCTemplate_type, liquidTemplate);
    Check_Type(stash, T_HASH);
    result = liquidRendererRenderTemplate(*liquidRenderer, (void*)stash, *liquidTemplate, &error);
    if (error.type != LIQUID_RENDERER_ERROR_TYPE_NONE) {
        liquidGetRendererErrorMessage(error, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCRendererError, error, exceptionInit, buffer, exception);
        rb_exc_raise(exception);
        return self;
    }
    str = rb_str_new(liquidTemplateRenderGetBuffer(result), liquidTemplateRenderGetSize(result));
    liquidFreeTemplateRender(result);
    return str;
}

VALUE method_liquidCParserParse(VALUE self, VALUE text) {
    LiquidParser* parser;
    LiquidTemplate tmpl;
    char* textBody;
    int textLength;
    LiquidLexerError lexerError;
    LiquidParserError parserError;
    VALUE result, arg, exception;
    VALUE exceptionInit[7];
    char buffer[512];

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);

    Check_Type(text, T_STRING);
    textBody = StringValueCStr(text);
    textLength = RSTRING_LEN(text);

    tmpl = liquidParserParseTemplate(*parser, textBody, textLength, &lexerError, &parserError);
    if (lexerError.type) {
        liquidGetLexerErrorMessage(lexerError, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCParserError, lexerError, exceptionInit, buffer, exception);
        rb_exc_raise(exception);
        return self;
    }
    if (parserError.type) {
        liquidGetParserErrorMessage(parserError, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCParserError, parserError, exceptionInit, buffer, exception);
        rb_exc_raise(exception);
        return self;
    }
    result = rb_obj_alloc(liquidCTemplate);
    arg = LL2NUM((long long)tmpl.ast);
    rb_obj_call_init(result, 1, &arg);
    return result;
}

VALUE method_liquidCParserWarnings(VALUE self) {
    LiquidParser* parser;
    LiquidParserWarning warning;
    VALUE warnings, exception, exceptionInit[7];
    size_t warningCount, i;
    char buffer[512];

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);
    warningCount = liquidGetParserWarningCount(*parser);
    warnings = rb_ary_new2(warningCount);
    for (i = 0; i < warningCount; ++i) {
        warning = liquidGetParserWarning(*parser, i);
        liquidGetParserErrorMessage(warning, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCParserError, warning, exceptionInit, buffer, exception);
        rb_ary_push(warnings, exception);
    }
    return warnings;
}


VALUE method_liquidCRendererWarnings(VALUE self) {
    LiquidRenderer* renderer;
    LiquidRendererWarning warning;
    VALUE warnings, exception, exceptionInit[7];
    size_t warningCount, i;
    char buffer[512];

    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, renderer);
    warningCount = liquidGetRendererWarningCount(*renderer);
    warnings = rb_ary_new2(warningCount);
    for (i = 0; i < warningCount; ++i) {
        warning = liquidGetRendererWarning(*renderer, i);
        liquidGetRendererErrorMessage(warning, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCRendererError, warning, exceptionInit, buffer, exception);
        rb_ary_push(warnings, exception);
    }
    return warnings;
}

VALUE liquidCTemplate_m_initialize(VALUE self, VALUE incomingTemplate) {
    LiquidTemplate* tmpl;
    Check_Type(incomingTemplate, T_FIXNUM);
    TypedData_Get_Struct(self, LiquidTemplate, &liquidCTemplate_type, tmpl);
    tmpl->ast = (void*)NUM2LL(incomingTemplate);
    return self;
}

VALUE liquidCError_m_initialize(VALUE self, VALUE type, VALUE message, VALUE args, VALUE line, VALUE column, VALUE file, VALUE english) {
    rb_iv_set(self, "@type", type);
    rb_iv_set(self, "@message", message);
    rb_iv_set(self, "@line", line);
    rb_iv_set(self, "@column", column);
    rb_iv_set(self, "@args", args);
    rb_iv_set(self, "@file", file);
    rb_iv_set(self, "@english", english);
    rb_call_super(1, &english);
    return self;
}

VALUE method_liquidCOptimizer_optimize(VALUE self, VALUE stash, VALUE incomingTemplate) {
    LiquidOptimizer* optimizer;
    LiquidTemplate* tmpl;
    TypedData_Get_Struct(self, LiquidOptimizer, &liquidCOptimizer_type, optimizer);
    TypedData_Get_Struct(incomingTemplate, LiquidTemplate, &liquidCTemplate_type, tmpl);
    liquidOptimizeTemplate(*optimizer, *tmpl, (void*)stash);
    return self;
}

static void liquidCRenderTag(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE* arguments;
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments = malloc(sizeof(VALUE)*(argMax+4));
    arguments[0] = LL2NUM((long long)renderer.renderer);
    arguments[1] = LL2NUM((long long)node.node);
    arguments[2] = (VALUE)variableStore;
    liquidGetChild((void**)&arguments[3], renderer, node, variableStore, 1);
    for (i = 0; i < argMax; ++i)
        liquidGetArgument((void**)&arguments[i+4], renderer, node, variableStore, i);
    result = rb_funcallv(renderProc, rb_intern("call"), 4+argMax, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
    free(arguments);
}

static void liquidCRenderFilter(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, operand;
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments = rb_ary_new2(argMax+4);
    rb_ary_push(arguments, LL2NUM((long long)renderer.renderer));
    rb_ary_push(arguments, LL2NUM((long long)node.node));
    rb_ary_push(arguments, (VALUE)variableStore);
    liquidFilterGetOperand((void**)&operand, renderer, node, variableStore);
    rb_ary_push(arguments, operand);
    for (i = 0; i < argMax; ++i) {
        VALUE argument;
        liquidGetArgument((void**)&argument, renderer, node, variableStore, i);
        rb_ary_push(arguments, argument);
    }
    result = rb_proc_call(renderProc, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
}


static void liquidCRenderBinaryInfixOperator(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, argument;
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments = rb_ary_new2(argMax+3);
    rb_ary_push(arguments, LL2NUM((long long)renderer.renderer));
    rb_ary_push(arguments, LL2NUM((long long)node.node));
    rb_ary_push(arguments, (VALUE)variableStore);
    for (i = 0; i < argMax; ++i) {
        liquidGetArgument((void**)&argument, renderer, node, variableStore, i);
        rb_ary_push(arguments, argument);
    }
    result = rb_proc_call(renderProc, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
}

static void liquidCRenderDotFilter(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, operand;
    renderProc = (VALUE)data;

    liquidFilterGetOperand((void**)&operand, renderer, node, variableStore);

    arguments = rb_ary_new2(4);
    rb_ary_push(arguments, LL2NUM((long long)renderer.renderer));
    rb_ary_push(arguments, LL2NUM((long long)node.node));
    rb_ary_push(arguments, (VALUE)variableStore);
    rb_ary_push(arguments, operand);

    result = rb_proc_call(renderProc, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
}

VALUE method_liquidC_registerTag(VALUE self, VALUE symbol, VALUE type, VALUE minArguments, VALUE maxArguments, VALUE optimization, VALUE renderFunction) {
    LiquidCRubyContext* context;
    const char* csymbol;
    TypedData_Get_Struct(self, LiquidCRubyContext, &liquidC_type, context);
    Check_Type(symbol, T_STRING);
    csymbol = StringValueCStr(symbol);
    liquidRegisterTag(context->context, csymbol, NUM2LL(type), NUM2LL(minArguments), NUM2LL(maxArguments), NUM2LL(optimization), liquidCRenderTag, (void*)renderFunction);
    rb_ary_push(context->registeredFunctionArray, renderFunction);
    return Qnil;
}

VALUE method_liquidC_registerFilter(VALUE self, VALUE symbol, VALUE minArguments, VALUE maxArguments, VALUE optimization, VALUE renderFunction) {
    LiquidCRubyContext* context;
    const char* csymbol;
    TypedData_Get_Struct(self, LiquidCRubyContext, &liquidC_type, context);
    Check_Type(symbol, T_STRING);
    csymbol = StringValueCStr(symbol);
    liquidRegisterFilter(context->context, csymbol, NUM2LL(minArguments), NUM2LL(maxArguments), NUM2LL(optimization), liquidCRenderFilter, (void*)renderFunction);
    rb_ary_push(context->registeredFunctionArray, renderFunction);
    return Qnil;
}

VALUE method_liquidC_registerOperator(VALUE self, VALUE symbol, VALUE priority, VALUE optimization, VALUE renderFunction) {
    LiquidCRubyContext* context;
    const char* csymbol;
    TypedData_Get_Struct(self, LiquidCRubyContext, &liquidC_type, context);
    Check_Type(symbol, T_STRING);
    csymbol = StringValueCStr(symbol);
    liquidRegisterOperator(context->context, csymbol, LIQUID_OPERATOR_ARITY_BINARY, LIQUID_OPERATOR_FIXNESS_INFIX, (int)NUM2LL(priority), (int)NUM2LL(optimization), liquidCRenderBinaryInfixOperator, (void*)renderFunction);
    rb_ary_push(context->registeredFunctionArray, renderFunction);
    return Qnil;
}

VALUE method_liquidC_registerDotFilter(VALUE self, VALUE symbol, VALUE optimization, VALUE renderFunction) {
    LiquidCRubyContext* context;
    const char* csymbol;
    TypedData_Get_Struct(self, LiquidCRubyContext, &liquidC_type, context);
    Check_Type(symbol, T_STRING);
    csymbol = StringValueCStr(symbol);
    liquidRegisterDotFilter(context->context, csymbol, NUM2LL(optimization), liquidCRenderDotFilter, (void*)renderFunction);
    rb_ary_push(context->registeredFunctionArray, renderFunction);
    return Qnil;
}



void Init_liquidc() {
	VALUE liquidC, liquidCRenderer, liquidCOptimizer, liquidCParser, liquidCError;
	liquidC = rb_define_class("LiquidC", rb_cData);
	liquidCRenderer = rb_define_class_under(liquidC, "Renderer", rb_cData);
	liquidCOptimizer = rb_define_class_under(liquidC, "Optimizer", rb_cData);
	liquidCTemplate = rb_define_class_under(liquidC, "Template", rb_cData);
	liquidCParser = rb_define_class_under(liquidC, "Parser", rb_cData);

	liquidCError = rb_define_class_under(liquidC, "Error", rb_eStandardError);
    rb_define_attr(liquidCError, "type", 1, 0);
    rb_define_attr(liquidCError, "line", 1, 0);
    rb_define_attr(liquidCError, "column", 1, 0);
    rb_define_attr(liquidCError, "message", 1, 0);
    rb_define_attr(liquidCError, "args", 1, 0);
    rb_define_attr(liquidCError, "english", 1, 0);
    rb_define_attr(liquidCError, "file", 1, 0);
    rb_define_method(liquidCError, "initialize", liquidCError_m_initialize, 7);

	liquidCParserError = rb_define_class_under(liquidCParser, "Error", liquidCError);
	liquidCRendererError = rb_define_class_under(liquidCRenderer, "Error", rb_eStandardError);

	rb_define_alloc_func(liquidC, liquidC_alloc);
	rb_define_method(liquidC, "initialize", liquidC_m_initialize, -1);
	rb_define_method(liquidC, "registerTag", method_liquidC_registerTag, 6);
	rb_define_method(liquidC, "registerFilter", method_liquidC_registerFilter, 5);
    rb_define_method(liquidC, "registerOperator", method_liquidC_registerOperator, 4);
    rb_define_method(liquidC, "registerDotFilter", method_liquidC_registerDotFilter, 3);


	rb_define_alloc_func(liquidCParser, liquidCParser_alloc);
    rb_define_method(liquidCParser, "initialize", liquidCParser_m_initialize, 1);
    rb_define_method(liquidCParser, "parse", method_liquidCParserParse, 1);
    rb_define_method(liquidCParser, "warnings", method_liquidCParserWarnings, 0);

	rb_define_alloc_func(liquidCRenderer, liquidCRenderer_alloc);
    rb_define_method(liquidCRenderer, "initialize", liquidCRenderer_m_initialize, 1);
    rb_define_method(liquidCRenderer, "setStrictVariables", liquidCRendererSetStrictVariables, 1);
    rb_define_method(liquidCRenderer, "setStrictFilters", liquidCRendererSetStrictFilters, 1);
    rb_define_method(liquidCRenderer, "render", method_liquidCTemplateRender, 2);
    rb_define_method(liquidCRenderer, "warnings", method_liquidCRendererWarnings, 0);

    rb_define_alloc_func(liquidCOptimizer, liquidCOptimizer_alloc);
    rb_define_method(liquidCOptimizer, "initialize", liquidCOptimizer_m_initialize, 1);
    rb_define_method(liquidCOptimizer, "optimize", method_liquidCOptimizer_optimize, 2);

	rb_define_alloc_func(liquidCTemplate, liquidCTemplate_alloc);
	rb_define_method(liquidCTemplate, "initialize", liquidCTemplate_m_initialize, 1);
}
