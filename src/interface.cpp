#include "interface.h"
#include "dialect.h"
#include "context.h"
#include <memory>

using namespace Liquid;

char* liquidError = nullptr;

LiquidContext liquidCreateContext(ELiquidContextSettings settings) {
    return LiquidContext({ new Context() });
}

void liquidFreeContext(LiquidContext context) {
    delete (Context*)context.context;
}

void liquidImplementStandardDialect(LiquidContext context) {
    StandardDialect::implement(*static_cast<Context*>(context.context));
}

LiquidRenderer liquidCreateRenderer(LiquidContext context) {
    return LiquidRenderer({ new Renderer(*static_cast<Context*>(context.context)) });
}

void liquidFreeRenderer(LiquidRenderer renderer) {
    delete (Renderer*)renderer.renderer;
}

LiquidTemplate liquidCreateTemplate(LiquidContext context, const char* buffer, size_t size, LiquidParserError* error) {
    Parser parser(*static_cast<Context*>(context.context));
    Node tmpl;
    try {
        tmpl = parser.parse(buffer, size);
    } catch (Parser::Exception& exp) {
        if (error)
            *error = exp.parserError;
        return LiquidTemplate({ NULL });
    }
    return LiquidTemplate({ new Node(std::move(tmpl)) });
}

void liquidFreeTemplate(LiquidTemplate tmpl) {
    delete (Node*)tmpl.ast;
}

LiquidTemplateRender liquidRenderTemplate(LiquidRenderer renderer, void* variableStore, LiquidTemplate tmpl, LiquidRenderError* error) {
    std::string* str = new std::string(std::move(static_cast<Renderer*>(renderer.renderer)->render(*static_cast<Node*>(tmpl.ast), Variable({ variableStore }))));
    return LiquidTemplateRender({ str });
}


void liquidRegisterVariableResolver(LiquidContext context, LiquidVariableResolver resolver) {
    Context* ctx = static_cast<Context*>(context.context);
    unique_ptr<NodeType> type = make_unique<Context::VariableNode>();
    static_cast<Context::VariableNode*>(type.get())->resolver = resolver;
    ctx->registerType(move(type));
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

const char* liquidGetError() {
    return liquidError;
}

void liquidClearError() {
    if (liquidError)
        delete[] liquidError;
    liquidError = nullptr;
}

void liquidSetError(const char* message) {
    liquidError = new char[strlen(message)+1];
    strcpy(liquidError, message);
}
