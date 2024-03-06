#include <ruby.h>
#include <ruby/encoding.h>
#include <liquid/liquid.h>

// Global definitions.

static VALUE liquidCParserError, liquidCRendererError, liquidCTemplate, liquidCProgram;

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
    if (TYPE((VALUE)variable) != T_STRING)
        return -1;
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
    VALUE result;
    if (TYPE((VALUE)variable) != T_HASH)
        return false;
    result = rb_hash_lookup2((VALUE)variable, rb_str_new2(key), Qundef);
    if (result == Qundef)
        return false;
    *target = (void*)result;
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
    VALUE value = (VALUE)variable;
    VALUE array_value;
    if (TYPE(value) != T_ARRAY)
        return -1;

    int length = RARRAY_LEN(value);
    if (length > limit)
        length = limit;
    length = length - 1;
    if (length < 0)
        length += RARRAY_LEN(value)+1;
    if (length >= 0) {
        if (reverse) {
            start = RARRAY_LEN(value) - start;
            for (size_t i = length; i >= start; --i) {
                array_value = rb_ary_entry((VALUE)variable, i);
                if (TYPE((VALUE)array_value))
                    liquidCcallback(array_value, data);
            }
        } else {
            for (size_t i = start; i <= length; ++i) {
                array_value = rb_ary_entry((VALUE)variable, i);
                if (TYPE((VALUE)array_value)){
                    liquidCcallback(array_value, data);
                }
            }
        }
    }
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
    return (void*)(value ? Qtrue : Qfalse);
}
static void* liquidCcreateInteger(LiquidRenderer renderer, long long value) {
    return (void*)LL2NUM(value);
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



void liquidC_free(void* data) {
    if (((LiquidCRubyContext*)data)->context.context)
        liquidFreeContext(((LiquidCRubyContext*)data)->context);
    free(data);
}
void liquidC_mark(void* self) {
    rb_gc_mark(((LiquidCRubyContext*)self)->registeredFunctionArray);
}
size_t liquidC_size(const void* data) { return sizeof(LiquidCRubyContext); }
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

VALUE liquidC_alloc(VALUE self) {
    LiquidCRubyContext* data = (LiquidCRubyContext*)malloc(sizeof(LiquidCRubyContext));
    data->context.context = NULL;
    data->registeredFunctionArray = rb_ary_new();
    return TypedData_Wrap_Struct(self, &liquidC_type, data);
}

// Type boilerplate.
#define LIQUID_STRUCT_BOILERPLATE(class, internal)\
void liquidC##class##_free(void* data) {\
    if (((Liquid##class*)data)->internal)\
        liquidFree##class(*((Liquid##class*)data));\
    free(data);\
}\
size_t liquidC##class##_size(const void* data) { return sizeof(Liquid##class); }\
static const rb_data_type_t liquidC##class##_type = {\
    .wrap_struct_name = #class,\
    .function = {\
            .dmark = NULL,\
            .dfree = liquidC##class##_free,\
            .dsize = liquidC##class##_size,\
    },\
    .data = NULL,\
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,\
};\
VALUE liquidC##class##_alloc(VALUE self) {\
    Liquid##class* data = (Liquid##class*)malloc(sizeof(Liquid##class));\
    data->internal = NULL;\
    return TypedData_Wrap_Struct(self, &liquidC##class##_type, data);\
}


LIQUID_STRUCT_BOILERPLATE(Parser, parser);
LIQUID_STRUCT_BOILERPLATE(Renderer, renderer);
LIQUID_STRUCT_BOILERPLATE(Template, ast);
LIQUID_STRUCT_BOILERPLATE(Optimizer, optimizer);
LIQUID_STRUCT_BOILERPLATE(Compiler, compiler);
LIQUID_STRUCT_BOILERPLATE(Program, program);

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
    liquidRendererSetCustomData(*renderer, (void*)self);
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

VALUE liquidCCompiler_m_initialize(VALUE self, VALUE incomingContext) {
    LiquidCompiler* compiler;
    LiquidCRubyContext* context;
    TypedData_Get_Struct(self, LiquidCompiler, &liquidCCompiler_type, compiler);
    TypedData_Get_Struct(incomingContext, LiquidCRubyContext, &liquidC_type, context);
    *compiler = liquidCreateCompiler(context->context);
    return self;
}


#define PACK_EXCEPTION_LENGTH 6
#define PACK_EXCEPTION(rubytype, error, package, english, exception) \
    exception = rb_obj_alloc(rubytype);\
    package[0] = LL2NUM(error.type);\
    package[1] = rb_ary_new2(LIQUID_ERROR_ARGS_MAX);\
    for (j = 0; j < LIQUID_ERROR_ARGS_MAX; ++j)\
        rb_ary_push(package[1], rb_str_new2(error.details.args[j]));\
    package[2] = LL2NUM(error.details.line);\
    package[3] = LL2NUM(error.details.column);\
    package[4] = rb_str_new2(error.details.file);\
    package[5] = rb_str_new2(english);\
    rb_obj_call_init(exception, 6, package);

VALUE method_liquidCTemplateRender(VALUE self, VALUE stash, VALUE tmpl) {
    VALUE str, exceptionInit[PACK_EXCEPTION_LENGTH], exception;
    LiquidTemplate* liquidTemplate;
    LiquidProgram* liquidProgram;
    LiquidRenderer* liquidRenderer;
    LiquidRendererError error;
    LiquidTemplateRender templateResult;
    LiquidProgramRender programResult;
    char buffer[512];
    int j, enc;
    bool program;
    Check_Type(stash, T_HASH);
    TypedData_Get_Struct(self, LiquidRenderer, &liquidCRenderer_type, liquidRenderer);
    program = RBASIC_CLASS(tmpl) == liquidCProgram;
    if (program) {
        TypedData_Get_Struct(tmpl, LiquidProgram, &liquidCProgram_type, liquidProgram);
        programResult = liquidRendererRunProgram(*liquidRenderer, (void*)stash, *liquidProgram, &error);
    } else {
        TypedData_Get_Struct(tmpl, LiquidTemplate, &liquidCTemplate_type, liquidTemplate);
        templateResult = liquidRendererRenderTemplate(*liquidRenderer, (void*)stash, *liquidTemplate, &error);
    }
    if (error.type != LIQUID_RENDERER_ERROR_TYPE_NONE) {
        liquidGetRendererErrorMessage(error, buffer, sizeof(buffer));
        PACK_EXCEPTION(liquidCRendererError, error, exceptionInit, buffer, exception);
        rb_exc_raise(exception);
        return self;
    }
    if (program)  {
        str = rb_str_new(programResult.str, programResult.len);
    } else {
        str = rb_str_new(liquidTemplateRenderGetBuffer(templateResult), liquidTemplateRenderGetSize(templateResult));
        liquidFreeTemplateRender(templateResult);
    }
    enc = rb_enc_find_index("UTF-8");
    rb_enc_associate_index(str, enc);
    return str;
}

VALUE method_liquidCParserParseTemplate(int argc, VALUE* argv, VALUE self) {
    LiquidParser* parser;
    LiquidTemplate tmpl;
    char* fileBody;
    char* textBody;
    int textLength;
    LiquidLexerError lexerError;
    LiquidParserError parserError;
    VALUE result, arg, exception;
    VALUE exceptionInit[PACK_EXCEPTION_LENGTH];
    char buffer[512];
    int j;

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);

    Check_Type(argv[0], T_STRING);
    textBody = StringValueCStr(argv[0]);
    textLength = RSTRING_LEN(argv[0]);
    if (argc > 1) {
        Check_Type(argv[1], T_STRING);
        fileBody = StringValueCStr(argv[1]);
    } else
        fileBody = NULL;


    tmpl = liquidParserParseTemplate(*parser, textBody, textLength, fileBody ? fileBody : "", &lexerError, &parserError);
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

VALUE method_liquidCParserParseAppropriate(int argc, VALUE* argv, VALUE self) {
    LiquidParser* parser;
    LiquidTemplate tmpl;
    char* fileBody;
    char* textBody;
    int textLength;
    LiquidLexerError lexerError;
    LiquidParserError parserError;
    VALUE result, arg, exception;
    VALUE exceptionInit[PACK_EXCEPTION_LENGTH];
    char buffer[512];
    int j;

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);

    Check_Type(argv[0], T_STRING);
    textBody = StringValueCStr(argv[0]);
    textLength = RSTRING_LEN(argv[0]);
    if (argc > 1) {
        Check_Type(argv[1], T_STRING);
        fileBody = StringValueCStr(argv[1]);
    } else
        fileBody = NULL;

    tmpl = liquidParserParseAppropriate(*parser, textBody, textLength, fileBody ? fileBody : "", &lexerError, &parserError);
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


VALUE method_liquidCParserParseArgument(int argc, VALUE* argv, VALUE self) {
    LiquidParser* parser;
    LiquidTemplate tmpl;
    char* textBody;
    int textLength;
    LiquidLexerError lexerError;
    LiquidParserError parserError;
    VALUE result, arg, exception;
    VALUE exceptionInit[PACK_EXCEPTION_LENGTH];
    char buffer[512];
    int j;

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);

    Check_Type(argv[0], T_STRING);
    textBody = StringValueCStr(argv[0]);
    textLength = RSTRING_LEN(argv[0]);

    tmpl = liquidParserParseArgument(*parser, textBody, textLength, &lexerError, &parserError);
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



VALUE method_liquidCParserUnparseTemplate(VALUE self, VALUE incomingTemplate) {
    LiquidParser* parser;
    LiquidTemplate* tmpl;
    char buffer[100*1024];
    int size, enc;
    VALUE str;

    TypedData_Get_Struct(self, LiquidParser, &liquidCParser_type, parser);
    TypedData_Get_Struct(incomingTemplate, LiquidTemplate, &liquidCTemplate_type, tmpl);

    size = liquidParserUnparseTemplate(*parser, *tmpl, buffer, sizeof(buffer));
    str = rb_str_new(buffer, size);
    enc = rb_enc_find_index("UTF-8");
    rb_enc_associate_index(str, enc);
    return str;
}



VALUE method_liquidCParserWarnings(VALUE self) {
    LiquidParser* parser;
    LiquidParserWarning warning;
    VALUE warnings, exception, exceptionInit[PACK_EXCEPTION_LENGTH];
    size_t warningCount, i;
    char buffer[512];
    int j;

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
    VALUE warnings, exception, exceptionInit[PACK_EXCEPTION_LENGTH];
    size_t warningCount, i;
    char buffer[512];
    int j;

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

VALUE liquidCError_m_initialize(VALUE self, VALUE type, VALUE args, VALUE line, VALUE column, VALUE file, VALUE english) {
    rb_iv_set(self, "@type", type);
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

VALUE method_liquidCCompiler_compile(VALUE self, VALUE incomingTemplate) {
    LiquidCompiler* compiler;
    LiquidTemplate* tmpl;
    LiquidProgram program;
    VALUE result, arg;

    TypedData_Get_Struct(self, LiquidCompiler, &liquidCCompiler_type, compiler);
    TypedData_Get_Struct(incomingTemplate, LiquidTemplate, &liquidCTemplate_type, tmpl);

    program = liquidCompilerCompileTemplate(*compiler, *tmpl);

    result = rb_obj_alloc(liquidCProgram);
    arg = LL2NUM((long long)program.program);
    rb_obj_call_init(result, 1, &arg);
    return result;
}

VALUE method_liquidCCompiler_decompile(VALUE self, VALUE incomingProgram) {
    LiquidCompiler* compiler;
    LiquidProgram* program;
    char buffer[100*1024];
    int size, enc;
    VALUE str;

    TypedData_Get_Struct(self, LiquidCompiler, &liquidCCompiler_type, compiler);
    TypedData_Get_Struct(incomingProgram, LiquidProgram, &liquidCProgram_type, program);

    size = liquidCompilerDisassembleProgram(*compiler, *program, buffer, sizeof(buffer));
    str = rb_str_new(buffer, size);
    enc = rb_enc_find_index("UTF-8");
    rb_enc_associate_index(str, enc);
    return str;
}

VALUE liquidCProgram_m_initialize(VALUE self, VALUE incomingProgram) {
    LiquidProgram* program;
    Check_Type(incomingProgram, T_FIXNUM);
    TypedData_Get_Struct(self, LiquidProgram, &liquidCProgram_type, program);
    program->program = (void*)NUM2LL(incomingProgram);
    return self;
}


static void liquidCRenderTag(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments[5];
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments[0] = (VALUE)liquidRendererGetCustomData(renderer);
    arguments[1] = LL2NUM((long long)node.node);
    arguments[2] = (VALUE)variableStore;
    liquidGetChild((void**)&arguments[3], renderer, node, variableStore, 1);
    arguments[4] = rb_ary_new2(argMax);
    for (i = 0; i < argMax; ++i) {
        VALUE argument;
        liquidGetArgument((void**)&argument, renderer, node, variableStore, i);
        rb_ary_push(arguments[4], argument);
    }
    result = rb_funcallv(renderProc, rb_intern("call"), 4+argMax, arguments);
    StringValue(result);
    liquidRendererSetReturnValueString(renderer, RSTRING_PTR(result), RSTRING_LEN(result));
}

static void liquidCRenderFilter(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, operand, arry, argument;
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments = rb_ary_new2(argMax+4);
    rb_ary_push(arguments, (VALUE)liquidRendererGetCustomData(renderer));
    rb_ary_push(arguments, LL2NUM((long long)node.node));
    rb_ary_push(arguments, (VALUE)variableStore);
    liquidFilterGetOperand((void**)&operand, renderer, node, variableStore);
    rb_ary_push(arguments, operand);
    arry = rb_ary_new2(argMax);
    for (i = 0; i < argMax; ++i) {
        liquidGetArgument((void**)&argument, renderer, node, variableStore, i);
        rb_ary_push(arry, argument);
    }
    rb_ary_push(arguments, arry);
    result = rb_proc_call(renderProc, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
}


static void liquidCRenderBinaryInfixOperator(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, argument, arry;
    int argMax, i;
    renderProc = (VALUE)data;

    argMax = liquidGetArgumentCount(node);
    arguments = rb_ary_new2(argMax+3);
    rb_ary_push(arguments, (VALUE)liquidRendererGetCustomData(renderer));
    rb_ary_push(arguments, LL2NUM((long long)node.node));
    rb_ary_push(arguments, (VALUE)variableStore);
    arry = rb_ary_new2(argMax);
    for (i = 0; i < argMax; ++i) {
        liquidGetArgument((void**)&argument, renderer, node, variableStore, i);
        rb_ary_push(arry, argument);
    }
    rb_ary_push(arguments, arry);
    result = rb_proc_call(renderProc, arguments);
    liquidRendererSetReturnValueVariable(renderer, (void*)result);
}

static void liquidCRenderDotFilter(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data) {
    VALUE result, renderProc;
    VALUE arguments, operand;
    renderProc = (VALUE)data;

    liquidFilterGetOperand((void**)&operand, renderer, node, variableStore);

    arguments = rb_ary_new2(4);
    rb_ary_push(arguments, (VALUE)liquidRendererGetCustomData(renderer));
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



void Init_liquidcpp() {
    VALUE liquidC, liquidCRenderer, liquidCOptimizer, liquidCParser, liquidCCompiler, liquidCError;
    liquidC = rb_define_class("LiquidCPP", rb_cData);
    liquidCRenderer = rb_define_class_under(liquidC, "Renderer", rb_cData);
    liquidCOptimizer = rb_define_class_under(liquidC, "Optimizer", rb_cData);
    liquidCTemplate = rb_define_class_under(liquidC, "Template", rb_cData);
    liquidCParser = rb_define_class_under(liquidC, "Parser", rb_cData);
    liquidCCompiler = rb_define_class_under(liquidC, "Compiler", rb_cData);
    liquidCProgram = rb_define_class_under(liquidC, "Program", rb_cData);

    liquidCError = rb_define_class_under(liquidC, "Error", rb_eStandardError);
    rb_define_attr(liquidCError, "type", 1, 0);
    rb_define_attr(liquidCError, "line", 1, 0);
    rb_define_attr(liquidCError, "column", 1, 0);
    rb_define_attr(liquidCError, "args", 1, 0);
    rb_define_attr(liquidCError, "english", 1, 0);
    rb_define_attr(liquidCError, "file", 1, 0);
    rb_define_method(liquidCError, "initialize", liquidCError_m_initialize, 6);

    liquidCParserError = rb_define_class_under(liquidCParser, "Error", liquidCError);
    liquidCRendererError = rb_define_class_under(liquidCRenderer, "Error", rb_eStandardError);

    rb_define_alloc_func(liquidC, liquidC_alloc);
    rb_define_method(liquidC, "initialize", liquidC_m_initialize, -1);
    rb_define_const(liquidC, "OPTIMIZATION_SCHEME_NONE", LL2NUM(LIQUID_OPTIMIZATION_SCHEME_NONE));
    rb_define_const(liquidC, "OPTIMIZATION_SCHEME_FULL", LL2NUM(LIQUID_OPTIMIZATION_SCHEME_FULL));
    rb_define_method(liquidC, "registerTag", method_liquidC_registerTag, 6);
    rb_define_const(liquidC, "TAG_TYPE_ENCLOSING", LL2NUM(LIQUID_TAG_TYPE_ENCLOSING));
    rb_define_const(liquidC, "TAG_TYPE_FREE", LL2NUM(LIQUID_TAG_TYPE_FREE));
    rb_define_method(liquidC, "registerFilter", method_liquidC_registerFilter, 5);
    rb_define_method(liquidC, "registerOperator", method_liquidC_registerOperator, 4);
    rb_define_method(liquidC, "registerDotFilter", method_liquidC_registerDotFilter, 3);

    rb_define_alloc_func(liquidCParser, liquidCParser_alloc);
    rb_define_method(liquidCParser, "initialize", liquidCParser_m_initialize, 1);
    rb_define_method(liquidCParser, "parseTemplate", method_liquidCParserParseTemplate, -1);
    rb_define_method(liquidCParser, "parseArgument", method_liquidCParserParseArgument, -1);
    rb_define_method(liquidCParser, "parseAppropriate", method_liquidCParserParseAppropriate, -1);
    rb_define_method(liquidCParser, "unparseTemplate", method_liquidCParserUnparseTemplate, 1);
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

    rb_define_alloc_func(liquidCCompiler, liquidCCompiler_alloc);
    rb_define_method(liquidCCompiler, "initialize", liquidCCompiler_m_initialize, 1);
    rb_define_method(liquidCCompiler, "compileTemplate", method_liquidCCompiler_compile, 1);
    rb_define_method(liquidCCompiler, "decompileProgram", method_liquidCCompiler_decompile, 1);

    rb_define_alloc_func(liquidCProgram, liquidCProgram_alloc);
    rb_define_method(liquidCProgram, "initialize", liquidCProgram_m_initialize, 1);

    rb_define_alloc_func(liquidCTemplate, liquidCTemplate_alloc);
    rb_define_method(liquidCTemplate, "initialize", liquidCTemplate_m_initialize, 1);
}
