#ifndef RENDERER_H
#define RENDERER_H

#include "parser.h"

namespace Liquid {
    struct Context;

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

        LiquidRenderErrorType error = LiquidRenderErrorType::LIQUID_RENDERER_ERROR_TYPE_NONE;

        struct Error : LiquidRenderError {
            typedef LiquidRenderErrorType Type;

            Error() {
                column = 0;
                row = 0;
                type = Type::LIQUID_RENDERER_ERROR_TYPE_NONE;
                message[0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            Error(Type type) {
                column = 0;
                row = 0;
                this->type = type;
                message[0] = 0;
            }

            Error(Type type, const std::string& message) {
                column = 0;
                row = 0;
                this->type = type;
                strcpy(this->message, message.data());
            }
        };

        struct Exception : Liquid::Exception {
            Renderer::Error rendererError;
            std::string message;
            Exception(const Renderer::Error& error) : rendererError(error) {
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
                }
                message = buffer;
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

        // Used for the C interface.
        Node returnValue;
        LiquidVariableResolver variableResolver;

        Renderer(const Context& context);
        Renderer(const Context& context, LiquidVariableResolver variableResolver);

        LiquidRenderErrorType render(const Node& ast, Variable store, void (*)(const char* chunk, size_t size, void* data), void* data);
        string render(const Node& ast, Variable store);
        Node retrieveRenderedNode(const Node& node, Variable store);
        std::chrono::duration<unsigned int,std::milli> getRenderedTime() const;

        operator LiquidRenderer() { return LiquidRenderer {this}; }

        void inject(Variable& variable, const Variant& variant);
        Variant parseVariant(Variable variable);
        string getString(const Node& node);



        Variable getVariable(const Node& node, Variable store);
        bool setVariable(const Node& node, Variable store, Variable value);

        const LiquidVariableResolver& getVariableResolver() const { return variableResolver; }
        bool resolveVariableString(string& target, void* variable) const {
            long long length = variableResolver.getStringLength(variable);
            if (length < 0)
                return false;
            target.resize(length);
            if (!variableResolver.getString(variable, const_cast<char*>(target.data())))
                return false;
            return true;
        }
    };
}

#endif
