#include "interface.h"
#include "dialect.h"
#include "context.h"
#include "optimizer.h"
#include <memory>

using namespace Liquid;

LiquidContext liquidCreateContext() {
    return LiquidContext({ new Context() });
}

void liquidFreeContext(LiquidContext context) {
    delete (Context*)context.context;
}

void liquidImplementStrictStandardDialect(LiquidContext context) {
    StandardDialect::implementStrict(*static_cast<Context*>(context.context));
}
void liquidImplementPermissiveStandardDialect(LiquidContext context) {
    StandardDialect::implementPermissive(*static_cast<Context*>(context.context));
}
#ifdef LIQUID_INCLUDE_WEB_DIALECT
    void liquidImplementWebDialect(LiquidContext context) {
        WebDialect::implement(*static_cast<Context*>(context.context));
    }
#endif

LiquidRenderer liquidCreateRenderer(LiquidContext context) {
    return LiquidRenderer({ new Renderer(*static_cast<Context*>(context.context)) });
}

void liquidFreeRenderer(LiquidRenderer renderer) {
    delete (Renderer*)renderer.renderer;
}

LiquidParser liquidCreateParser(LiquidContext context) {
    return LiquidParser({ new Parser(*static_cast<Context*>(context.context)) });
}
void liquidFreeParser(LiquidParser parser) {
    delete (Parser*)parser.parser;
}

LiquidOptimizer liquidCreateOptimizer(LiquidRenderer renderer) {
    return LiquidOptimizer({ new Optimizer(*static_cast<Renderer*>(renderer.renderer)) });
}

void liquidOptimizeTemplate(LiquidOptimizer optimizer, LiquidTemplate tmpl, void* variableStore) {
    static_cast<Optimizer*>(optimizer.optimizer)->optimize(*static_cast<Node*>(tmpl.ast), Variable({ variableStore }));
}

void liquidFreeOptimizer(LiquidOptimizer optimizer) {
    delete (Optimizer*)optimizer.optimizer;
}

LiquidTemplate liquidParserParseTemplate(LiquidParser parser, const char* buffer, size_t size, LiquidLexerError* lexerError, LiquidParserError* parserError) {
    Node tmpl;
    if (lexerError)
        lexerError->type = LiquidLexerErrorType::LIQUID_LEXER_ERROR_TYPE_NONE;
    if (parserError)
        parserError->type = LiquidParserErrorType::LIQUID_PARSER_ERROR_TYPE_NONE;
    try {
        tmpl = static_cast<Parser*>(parser.parser)->parse(buffer, size);
    } catch (Parser::Exception& exp) {
        if (parserError)
            *parserError = exp.parserErrors[0];
        return LiquidTemplate({ NULL });
    }
    return LiquidTemplate({ new Node(std::move(tmpl)) });
}

size_t liquidGetParserWarningCount(LiquidParser parser) {
    return static_cast<Parser*>(parser.parser)->errors.size();
}

LiquidParserWarning liquidGetParserWarning(LiquidParser parser, size_t index) {
    return static_cast<Parser*>(parser.parser)->errors[index];
}

void liquidFreeTemplate(LiquidTemplate tmpl) {
    delete (Node*)tmpl.ast;
}

LiquidTemplateRender liquidRendererRenderTemplate(LiquidRenderer renderer, void* variableStore, LiquidTemplate tmpl, LiquidRendererError* error) {
    if (error)
        error->type = LIQUID_RENDERER_ERROR_TYPE_NONE;
    std::string* str;
    try {
        str = new std::string(std::move(static_cast<Renderer*>(renderer.renderer)->render(*static_cast<Node*>(tmpl.ast), Variable({ variableStore }))));
    } catch (Renderer::Exception& exp) {
        if (error)
            *error = exp.rendererError;
        return LiquidTemplateRender({ NULL });
    }
    return LiquidTemplateRender({ str });
}

size_t liquidGetRendererWarningCount(LiquidRenderer renderer) {
    return static_cast<Renderer*>(renderer.renderer)->errors.size();
}

LiquidRendererWarning liquidGetRendererWarning(LiquidRenderer renderer, size_t idx) {
    return static_cast<Renderer*>(renderer.renderer)->errors[idx];
}

void liquidWalkTemplate(LiquidTemplate tmpl, LiquidWalkTemplateFunction callback, void* data) {
    static_cast<Node*>(tmpl.ast)->walk([tmpl, callback, data](const Node& node) {
        callback(tmpl, LiquidNode { const_cast<Node*>(&node) }, data);
    });
}

void* liquidRegisterTag(LiquidContext context, const char* symbol, enum ELiquidTagType type, int minArguments, int maxArguments, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data) {
    Context* ctx = static_cast<Context*>(context.context);
    unique_ptr<NodeType> registeredType = make_unique<TagNodeType>((TagNodeType::Composition)type, symbol, minArguments, maxArguments, optimization);
    registeredType->userRenderFunction = renderFunction;
    registeredType->userData = data;
    return ctx->registerType(move(registeredType));
}
void* liquidRegisterFilter(LiquidContext context, const char* symbol, int minArguments, int maxArguments, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data) {
    Context* ctx = static_cast<Context*>(context.context);
    unique_ptr<NodeType> registeredType = make_unique<FilterNodeType>(symbol, minArguments, maxArguments, optimization);
    registeredType->userRenderFunction = renderFunction;
    registeredType->userData = data;
    return ctx->registerType(move(registeredType));
}

void* liquidRegisterOperator(LiquidContext context, const char* symbol, enum ELiquidOperatorArity arity, enum ELiquidOperatorFixness fixness, int priority, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data) {
    Context* ctx = static_cast<Context*>(context.context);
    unique_ptr<NodeType> registeredType = make_unique<OperatorNodeType>(symbol, (OperatorNodeType::Arity)arity, priority, (OperatorNodeType::Fixness)fixness, optimization);
    registeredType->userRenderFunction = renderFunction;
    registeredType->userData = data;
    return ctx->registerType(move(registeredType));
}

void* liquidRegisterDotFilter(LiquidContext context, const char* symbol, LiquidOptimizationScheme optimization, LiquidRenderFunction renderFunction, void* data) {
    Context* ctx = static_cast<Context*>(context.context);
    unique_ptr<NodeType> registeredType = make_unique<DotFilterNodeType>(symbol, optimization);
    registeredType->userRenderFunction = renderFunction;
    registeredType->userData = data;
    return ctx->registerType(move(registeredType));
}

void* liquidNodeTypeGetUserData(void* nodeType) {
    return static_cast<NodeType*>(nodeType)->userData;
}


void liquidFilterGetOperand(void** targetVariable, LiquidRenderer lRenderer, LiquidNode filter, void* variableStore) {
    Renderer& renderer = *static_cast<Renderer*>(lRenderer.renderer);
    const Node& node = *static_cast<Node*>(filter.node);
    Node op = static_cast<const FilterNodeType*>(node.type)->getOperand(renderer, node, variableStore);
    if (!op.type) {
        Variable v;
        renderer.inject(v, op.variant);
        *targetVariable = v;
    }
}


void liquidGetArgument(void** targetVariable, LiquidRenderer lRenderer, LiquidNode parent, void* variableStore, int idx) {
    Renderer& renderer = *static_cast<Renderer*>(lRenderer.renderer);
    const Node& node = *static_cast<Node*>(parent.node);

    Node arg = static_cast<const NodeType*>(node.type)->getArgument(renderer, node, variableStore, idx);
    if (!arg.type) {
        Variable v;
        renderer.inject(v, arg.variant);
        *targetVariable = v;
    }
}

int liquidGetArgumentCount(LiquidNode parent) {
    const Node& node = *static_cast<Node*>(parent.node);
    return static_cast<const NodeType*>(node.type)->getArgumentCount(node);
}


void liquidGetChild(void** targetVariable, LiquidRenderer lRenderer, LiquidNode parent, void* variableStore, int idx) {
    Renderer& renderer = *static_cast<Renderer*>(lRenderer.renderer);
    const Node& node = *static_cast<Node*>(parent.node);

    Node child = static_cast<const NodeType*>(node.type)->getChild(renderer, node, variableStore, idx);
    if (!child.type) {
        Variable v;
        renderer.inject(v, child.variant);
        *targetVariable = v;
    }
}

int liquidGetChildCount(LiquidNode parent) {
    const Node& node = *static_cast<Node*>(parent.node);
    return static_cast<const NodeType*>(node.type)->getChildCount(node);
}

void liquidRendererSetCustomData(LiquidRenderer renderer, void* data) {
    static_cast<Renderer*>(renderer.renderer)->customData = data;
}

void* liquidRendererGetCustomData(LiquidRenderer renderer) {
    return static_cast<Renderer*>(renderer.renderer)->customData;
}


void liquidRendererSetReturnValueString(LiquidRenderer renderer, const char* s, int length) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = move(Variant(string(s, length)));
}

void liquidRendererSetReturnValueNil(LiquidRenderer renderer) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = Variant();
}

void liquidRendererSetReturnValueBool(LiquidRenderer renderer, bool b) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = Variant(b);
}

void liquidRendererSetReturnValueInteger(LiquidRenderer renderer, long long i) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = Variant(i);
}

void liquidRendererSetReturnValueFloat(LiquidRenderer renderer, double f) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = Variant(f);
}

void liquidRendererSetReturnValueVariable(LiquidRenderer renderer, void* variable) {
    static_cast<Renderer*>(renderer.renderer)->returnValue = Variant(Variable{variable});
}

void liquidRegisterVariableResolver(LiquidRenderer renderer, LiquidVariableResolver resolver) {
    Renderer* rdr = static_cast<Renderer*>(renderer.renderer);
    rdr->variableResolver = resolver;
}

void liquidFreeTemplateRender(LiquidTemplateRender render) {
    delete (std::string*)(render.internal);
}

const char* liquidTemplateRenderGetBuffer(LiquidTemplateRender render) {
    return static_cast<std::string*>(render.internal)->data();
}

size_t liquidTemplateRenderGetSize(LiquidTemplateRender render) {
    return static_cast<std::string*>(render.internal)->size();
}

const char* liquidGetLexerErrorMessage(LiquidLexerError error) {
    string s = Lexer<Parser>::Error::english(error);
    return s.c_str();
}
const char* liquidGetParserErrorMessage(LiquidParserError error) {
    string s = Parser::Error::english(error);
    return s.c_str();
}
const char* liquidGetRendererErrorMessage(LiquidRendererError error) {
    string s = Renderer::Error::english(error);
    return s.c_str();
}
