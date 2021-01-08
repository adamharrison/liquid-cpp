#ifndef LIQUIDRENDERER_H
#define LIQUIDRENDERER_H

#include "parser.h"

namespace Liquid {
    struct Context;
    struct ContextBoundaryNode;

    // One renderer per thread; though many renderers can be instantiated.
    struct Renderer {
        const Context& context;

        enum class Control {
            NONE,
            BREAK,
            CONTINUE,
            EXIT
        };
        // The current state of the break. Allows us to have break/continue statements.
        Control control = Control::NONE;
        // In order to have a more genericized version of forloop drops, that are not affected by assigns.
        typedef Node (*DropFunction)(Renderer& renderer, const Node& node, Variable store, void* data);
        std::unordered_map<std::string, std::vector<std::pair<void*, DropFunction>>> internalDrops;
        std::pair<void*, DropFunction> getInternalDrop(const Node& node, Variable store);
        std::pair<void*, DropFunction> getInternalDrop(const std::string& str);
        void pushInternalDrop(const std::string& key, std::pair<void*, DropFunction> func);
        void popInternalDrop(const std::string& key);

        LiquidRendererErrorType error = LiquidRendererErrorType::LIQUID_RENDERER_ERROR_TYPE_NONE;

        struct Error : LiquidRendererError {
            typedef LiquidRendererErrorType Type;

            Error() {
                type = Type::LIQUID_RENDERER_ERROR_TYPE_NONE;
                details.column = 0;
                details.line = 0;
                details.args[0][0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            Error(Type type, const Node& node, const std::string& message = "") {
                details.column = node.column;
                details.line = node.line;
                this->type = type;
                strncpy(details.args[0], message.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[0][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
            }

            static string english(const LiquidRendererError& rendererError) {
                char buffer[512];
                switch (rendererError.type) {
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_NONE: break;
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_MEMORY:
                        sprintf(buffer, "Exceeded memory.");
                    break;
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_TIME:
                        sprintf(buffer, "Exceeded rendering time.");
                    break;
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_EXCEEDED_DEPTH:
                        sprintf(buffer, "Exceeded stack depth.");
                    break;
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_VARIABLE:
                        sprintf(buffer, "Unknown variable '%s'.", rendererError.details.args[0]);
                    break;
                    case Renderer::Error::Type::LIQUID_RENDERER_ERROR_TYPE_UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s'.", rendererError.details.args[0]);
                    break;
                }
                return string(buffer);
            }
        };

        struct Exception : Liquid::Exception {
            Renderer::Error rendererError;
            std::string message;
            Exception(const Renderer::Error& error) : rendererError(error) {
                message = Error::english(error);
            }

            const char* what() const noexcept {
               return message.data();
            }
        };


        // If set, this will stop rendering with an error if the limits here, in bytes are breached for this renderer. This is checked any time
        // the variable resolver is used.
        unsigned int maximumMemoryUsage = 0;
        // If set, this will stop rendering with an error if the limits here, in milisecnods, are breached for this renderer.
        // This is checked between all concatenations.
        unsigned int maximumRenderingTime = 0;
        // How many concatenation nodes are allowed at any given time. This roughly corresponds to the amount of nested tags. In non-malicious code
        // this will probably rarely exceed 100.
        unsigned int maximumRenderingDepth = 100;

        unsigned int currentMemoryUsage;
        std::chrono::system_clock::time_point renderStartTime;
        unsigned int currentRenderingDepth;

        bool logUnknownFilters = false;
        bool logUnknownVariables = false;

        bool internalRender = false;

        ContextBoundaryNode* nodeContext = nullptr;

        // Done so we don't repeat unknown errors if they're inloops.
        unordered_set<const Node*> unknownErrors;
        void pushUnknownVariableWarning(const Node& node, int offset, Variable store);
        void pushUnknownFilterWarning(const Node& node, Variable store);


        // Used for the C interface.
        Node returnValue;
        void* customData;
        LiquidVariableResolver variableResolver;

        Renderer(const Context& context);
        Renderer(const Context& context, LiquidVariableResolver variableResolver);

        vector<Error> errors;
        LiquidRendererErrorType render(const Node& ast, Variable store, void (*)(const char* chunk, size_t size, void* data), void* data);
        string render(const Node& ast, Variable store);
        // Retrieves a rendered node, if possible. If the node in question has a nodetype that is PARTIAL optimized, Has the potential to return node with
        // a type still attached; otherwise, will always be a variant node.
        Node retrieveRenderedNode(const Node& node, Variable store) {
            if (node.type)
                return node.type->render(*this, node, store);
            return node;
        }
        std::chrono::duration<unsigned int,std::milli> getRenderedTime() const;

        operator LiquidRenderer() { return LiquidRenderer {this}; }

        void inject(Variable& variable, const Variant& variant);
        Variant parseVariant(Variable variable);
        string getString(const Node& node);



        Variable getVariable(const Node& node, Variable store, size_t offset = 0);
        bool setVariable(const Node& node, Variable store, Variable value, size_t offset = 0);

        const LiquidVariableResolver& getVariableResolver() const { return variableResolver; }
        bool resolveVariableString(string& target, void* variable) {
            long long length = variableResolver.getStringLength(LiquidRenderer { this }, variable);
            if (length < 0)
                return false;
            target.resize(length);
            if (!variableResolver.getString(LiquidRenderer { this }, variable, const_cast<char*>(target.data())))
                return false;
            return true;
        }
    };
}

#endif
