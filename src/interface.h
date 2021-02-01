#ifndef LIQUIDINTERFACE_H
#define LIQUIDINTERFACE_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    #define LIQUID_ERROR_ARG_MAX_LENGTH 32
    #define LIQUID_ERROR_FILE_MAX_LENGTH 256
    #define LIQUID_ERROR_ARGS_MAX 5

    typedef enum ELiquidLexerErrorType {
        LIQUID_LEXER_ERROR_TYPE_NONE,
        LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END
    } LiquidLexerErrorType;

    typedef enum ELiquidParserErrorType {
        LIQUID_PARSER_ERROR_TYPE_NONE,
        LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END,
        // Self-explamatory.
        LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG,
        LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR,
        LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER,
        LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER,
        LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_OPERAND,
        LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS,
        // Weird symbol in weird place.
        LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL,
        // Was expecting somthing else, i.e. {{ i + }}; was expecting a number there.
        LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP,
        LIQUID_PARSER_ERROR_TYPE_PARSE_DEPTH_EXCEEDED
    } LiquidParserErrorType;


    typedef struct SLiquidErrorDetails {
        size_t line;
        size_t column;
        char file[LIQUID_ERROR_FILE_MAX_LENGTH];
        char args[LIQUID_ERROR_ARGS_MAX][LIQUID_ERROR_ARG_MAX_LENGTH];
    } LiquidErrorDetails;

    typedef struct SLiquidLexerError {
        LiquidLexerErrorType type;
        LiquidErrorDetails details;
    } LiquidLexerError;

    typedef struct SLiquidParserError {
        LiquidParserErrorType type;
        LiquidErrorDetails details;
    } LiquidParserError, LiquidParserWarning;

    typedef enum ELiquidRendererErrorType {
        LIQUID_RENDERER_ERROR_TYPE_NONE,
        LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_MEMORY,
        LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_TIME,
        LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_DEPTH,
        LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_VARIABLE,
        LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_FILTER
    } LiquidRendererErrorType;

    typedef struct SLiquidRendererError {
        LiquidRendererErrorType type;
        LiquidErrorDetails details;
    } LiquidRendererError, LiquidRendererWarning;

    typedef enum ELiquidOptimizationScheme {
        LIQUID_OPTIMIZATION_SCHEME_SHIELD,
        LIQUID_OPTIMIZATION_SCHEME_NONE,
        LIQUID_OPTIMIZATION_SCHEME_PARTIAL,
        LIQUID_OPTIMIZATION_SCHEME_FULL
    } LiquidOptimizationScheme;

    typedef enum ELiquidTagType {
        LIQUID_TAG_TYPE_ENCLOSING,
        LIQUID_TAG_TYPE_FREE
    } LiquidTagType;

    typedef enum ELiquidOperatorArity {
        LIQUID_OPERATOR_ARITY_NONARY,
        LIQUID_OPERATOR_ARITY_UNARY,
        LIQUID_OPERATOR_ARITY_BINARY,
        LIQUID_OPERATOR_ARITY_NARY
    } LiquidOperatorArity;

    typedef enum ELiquidOperatorFixness {
        LIQUID_OPERATOR_FIXNESS_PREFIX,
        LIQUID_OPERATOR_FIXNESS_INFIX,
        LIQUID_OPERATOR_FIXNESS_AFFIX
    } LiquidOperatorFixness;

    typedef struct SLiquidContext { void* context; } LiquidContext;
    typedef struct SLiquidRenderer { void* renderer; } LiquidRenderer;
    typedef struct SLiquidParser { void* parser; } LiquidParser;
    typedef struct SLiquidTemplate { void* ast; } LiquidTemplate;
    typedef struct SLiquidOptimizer { void* optimizer; } LiquidOptimizer;
    typedef struct SLiquidCompiler { void* compiler; } LiquidCompiler;
    typedef struct SLiquidProgram { void* program; } LiquidProgram;
    typedef struct SLiquidNode { void* node; } LiquidNode;
    typedef struct SLiquidTemplateRender { void* internal; } LiquidTemplateRender;

    typedef enum ELiquidVariableType {
        LIQUID_VARIABLE_TYPE_NIL,
        LIQUID_VARIABLE_TYPE_FLOAT,
        LIQUID_VARIABLE_TYPE_INT,
        LIQUID_VARIABLE_TYPE_STRING,
        LIQUID_VARIABLE_TYPE_ARRAY,
        LIQUID_VARIABLE_TYPE_BOOL,
        LIQUID_VARIABLE_TYPE_DICTIONARY,
        LIQUID_VARIABLE_TYPE_OTHER
    } LiquidVariableType;

    // Convenience function to register a custom variable type.
    // Ownership model looks thusly:
    // Calling create creates a newly allocated pointer. In all cases, one of the two things must happen:
    //  1. It must be set as an array element, or a hash element.
    //  2. It must be freed with freeVariable.
    // For languages where the variables are garbage collected, like perl and ruby; freeVariable will be a no-op.
    // Whenever getArrayVariable or getDictionaryVariable are called, a pointer is given, but no allocaitons are made.
    typedef struct SLiquidVariableResolver {
        LiquidVariableType (*getType)(LiquidRenderer renderer, void* variable);
        bool (*getBool)(LiquidRenderer renderer, void* variable, bool* target);
        bool (*getTruthy)(LiquidRenderer renderer, void* variable);
        bool (*getString)(LiquidRenderer renderer, void* variable, char* target);
        long long (*getStringLength)(LiquidRenderer renderer, void* variable);
        bool (*getInteger)(LiquidRenderer renderer, void* variable, long long* target);
        bool (*getFloat)(LiquidRenderer renderer, void* variable, double* target);
        bool (*getDictionaryVariable)(LiquidRenderer renderer, void* variable, const char* key, void** target);
        bool (*getArrayVariable)(LiquidRenderer renderer, void* variable, long long idx, void** target);
        bool (*iterate)(LiquidRenderer renderer, void* variable, bool (*callback)(void* variable, void* data), void* data, int start, int limit, bool reverse);
        long long (*getArraySize)(LiquidRenderer renderer, void* variable);
        void* (*setDictionaryVariable)(LiquidRenderer renderer, void* variable, const char* key, void* target);
        void* (*setArrayVariable)(LiquidRenderer renderer, void* variable, long long idx, void* target);
        void* (*createHash)(LiquidRenderer renderer);
        void* (*createArray)(LiquidRenderer renderer);
        void* (*createFloat)(LiquidRenderer renderer, double value);
        void* (*createBool)(LiquidRenderer renderer, bool value);
        void* (*createInteger)(LiquidRenderer renderer, long long value);
        void* (*createString)(LiquidRenderer renderer, const char* str);
        void* (*createPointer)(LiquidRenderer renderer, void* value);
        void* (*createNil)(LiquidRenderer renderer);
        void* (*createClone)(LiquidRenderer renderer, void* value);
        void (*freeVariable)(LiquidRenderer renderer, void* value);
        int (*compare)(void* a, void* b);
    } LiquidVariableResolver;

    LiquidContext liquidCreateContext();
    const char* liquidGetContextError(LiquidContext context);
    void liquidFreeContext(LiquidContext context);
    void liquidImplementStrictStandardDialect(LiquidContext context);
    void liquidImplementPermissiveStandardDialect(LiquidContext context);
    #ifdef LIQUID_INCLUDE_WEB_DIALECT
        void liquidImplementWebDialect(LiquidContext context);
    #endif
    #ifdef LIQUID_INCLUDE_RAPIDJSON_VARIABLE
        void liquidImplementJSONStringVariable();
    #endif

    LiquidRenderer liquidCreateRenderer(LiquidContext context);
    void liquidRendererSetStrictVariables(LiquidRenderer renderer, bool strict);
    void liquidRendererSetStrictFilters(LiquidRenderer renderer, bool strict);
    void liquidRendererSetCustomData(LiquidRenderer renderer, void* data);
    void* liquidRendererGetCustomData(LiquidRenderer renderer);
    void liquidRendererSetReturnValueNil(LiquidRenderer renderer);
    void liquidRendererSetReturnValueBool(LiquidRenderer renderer, bool b);
    void liquidRendererSetReturnValueString(LiquidRenderer renderer, const char* s, int length);
    void liquidRendererSetReturnValueInteger(LiquidRenderer renderer, long long i);
    void liquidRendererSetReturnValueFloat(LiquidRenderer renderer, double f);
    void liquidRendererSetReturnValueVariable(LiquidRenderer renderer, void* variable);
    size_t liquidGetRendererWarningCount(LiquidRenderer renderer);
    LiquidRendererWarning liquidGetRendererWarning(LiquidRenderer renderer, size_t index);
    void liquidFreeRenderer(LiquidRenderer renderer);

    LiquidParser liquidCreateParser(LiquidContext context);
    size_t liquidGetParserWarningCount(LiquidParser parser);
    LiquidParserWarning liquidGetParserWarning(LiquidParser parser, size_t index);
    void liquidFreeParser(LiquidParser parser);

    LiquidTemplate liquidParserParseTemplate(LiquidParser parser, const char* buffer, size_t size, const char* file, LiquidLexerError* lexer, LiquidParserError* error);
    LiquidTemplate liquidParserParseArgument(LiquidParser parser, const char* buffer, size_t size, LiquidLexerError* lexer, LiquidParserError* error);
    LiquidTemplate liquidParserParseAppropriate(LiquidParser parser, const char* buffer, size_t size, const char* file, LiquidLexerError* lexer, LiquidParserError* error);
    void liquidFreeTemplate(LiquidTemplate tmpl);

    LiquidOptimizer liquidCreateOptimizer(LiquidRenderer renderer);
    void liquidOptimizeTemplate(LiquidOptimizer optimizer, LiquidTemplate tmpl, void* variableStore);
    void liquidFreeOptimizer(LiquidOptimizer optimizer);

    LiquidTemplateRender liquidRendererRenderTemplate(LiquidRenderer renderer, void* variableStore, LiquidTemplate tmpl, LiquidRendererError* error);
    void* liquidRendererRenderArgument(LiquidRenderer renderer, void* variableStore, LiquidTemplate argument, LiquidRendererError* error);
    typedef void (*LiquidWalkTemplateFunction)(LiquidTemplate tmpl, const LiquidNode node, void* data);
    void liquidWalkTemplate(LiquidTemplate tmpl, LiquidWalkTemplateFunction callback, void* data);
    void liquidFreeTemplateRender(LiquidTemplateRender render);

    const char* liquidTemplateRenderGetBuffer(LiquidTemplateRender render);
    size_t liquidTemplateRenderGetSize(LiquidTemplateRender render);

    void liquidGetLexerErrorMessage(LiquidLexerError error, char* buffer, size_t maxSize);
    void liquidGetParserErrorMessage(LiquidParserError error, char* buffer, size_t maxSize);
    void liquidGetRendererErrorMessage(LiquidRendererError error, char* buffer, size_t maxSize);

    void liquidFilterGetOperand(void** targetVariable, LiquidRenderer renderer, LiquidNode filter, void* variableStore);
    void liquidGetArgument(void** targetVariable, LiquidRenderer renderer, LiquidNode node, void* variableStore, int idx);
    void liquidGetChild(void** targetVariable, LiquidRenderer renderer, LiquidNode node, void* variablestore, int idx);
    int liquidGetArgumentCount(LiquidNode node);
    int liquidGetChildCount(LiquidNode node);
    typedef void (*LiquidRenderFunction)(LiquidRenderer renderer, LiquidNode node, void* variableStore, void* data);
    typedef void (*LiquidCompileFunction)(LiquidCompiler compiler, LiquidNode node, void* data);
    void liquidRegisterVariableResolver(LiquidRenderer renderer, LiquidVariableResolver resolver);

    // Passing -1 to min/maxArguments means no min or max.
    void* liquidRegisterTag(LiquidContext context, const char* symbol, enum ELiquidTagType type, int minArguments, int maxArguments, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data);
    void* liquidRegisterFilter(LiquidContext context, const char* symbol, int minArguments, int maxArguments, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data);
    void* liquidRegisterDotFilter(LiquidContext context, const char* symbol, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data);
    void* liquidRegisterOperator(LiquidContext context, const char* symbol, enum ELiquidOperatorArity arity, enum ELiquidOperatorFixness fixness, int priority, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data);

    void* liquidNodeTypeGetUserData(void* nodeType);
#ifdef __cplusplus
}
#endif

#endif
