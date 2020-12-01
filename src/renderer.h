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

        LiquidRenderErrorType error = LiquidRenderErrorType::LIQUID_RENDER_ERROR_TYPE_NONE;

        struct Error : LiquidRenderError {
            typedef LiquidRenderErrorType Type;

            Error() {
                column = 0;
                row = 0;
                type = Type::LIQUID_RENDER_ERROR_TYPE_NONE;
                message[0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            template <class T>
            Error(Type type) {
                column = 0;
                row = 0;
                this->type = type;
                message[0] = 0;
            }
            template <class T>
            Error(Type type, const std::string& message) {
                column = 0;
                row = 0;
                this->type = type;
                strcpy(this->message, message.data());
            }
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



        Renderer(const Context& context) : context(context) { }

        LiquidRenderErrorType render(const Node& ast, Variable store, void (*)(const char* chunk, size_t size, void* data), void* data);
        string render(const Node& ast, Variable store);
        Node retrieveRenderedNode(const Node& node, Variable store);
        std::chrono::duration<unsigned int,std::milli> getRenderedTime() const;

        operator LiquidRenderer() { return LiquidRenderer {this}; }

        void inject(Variable& variable, const Variant& variant);
        Variant parseVariant(Variable variable);
        string getString(const Node& node);
    };
}

#endif
