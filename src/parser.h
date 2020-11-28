#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"

namespace Liquid {

    struct NodeType;
    struct Variable;

    struct Parser {
        const Context& context;

        struct Error : LiquidParserError {
            typedef LiquidParserErrorType Type;

            std::string message;

            Error() {
                type = Type::LIQUID_PARSER_ERROR_TYPE_NONE;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;

            template <class T>
            Error(T& lexer, Type type) {
                column = lexer.column;
                row = lexer.row;
                this->type = type;
            }
            template <class T>
            Error(T& lexer, Type type, const std::string& message) : message(message) {
                this->type = type;
                column = lexer.column;
                row = lexer.row;

            }
            Error(Type type) {
                column = 0;
                row = 0;
                this->type = type;
            }

            Error(Type type, const std::string& message) : message(message) {
                column = 0;
                row = 0;
                this->type = type;
            }
        };



        enum class State {
            NODE,
            ARGUMENT
        };
        enum class EFilterState {
            UNSET,
            COLON,
            NAME,
            ARGUMENTS
        };


        enum class EBlockType {
            NONE,
            INTERMEDIATE,
            END
        };

        State state;
        EFilterState filterState;
        EBlockType blockType;
        vector<unique_ptr<Node>> nodes;
        vector<Error> errors;

        bool treatUnknownFiltersAsErrors = false;

        void pushError(const Error& error) {
            errors.push_back(error);
        }

        struct Lexer : Liquid::Lexer<Lexer> {
            Parser& parser;
            typedef Liquid::Lexer<Lexer> SUPER;

            bool literal(const char* str, size_t len);
            bool dot();
            bool colon();
            bool comma();

            bool startOutputBlock(bool suppress);
            bool endOutputBlock(bool suppress);
            bool endControlBlock(bool suppress);

            bool startVariableDereference();
            bool endVariableDereference();
            bool string(const char* str, size_t len);
            bool integer(long long i);
            bool floating(double f);

            bool openParenthesis();
            bool closeParenthesis();

            Lexer(const Context& context, Parser& parser) : Liquid::Lexer<Lexer>(context), parser(parser) { }
        };


        struct Exception : Liquid::Exception {
            Parser::Error parserError;
            Lexer::Error lexerError;
            std::string message;
            Exception(const Parser::Error& error) : parserError(error) {
                char buffer[512];
                switch (parserError.type) {
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_NONE: break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG:
                        sprintf(buffer, "Unknown tag '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR:
                        sprintf(buffer, "Unknown operator '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER:
                        sprintf(buffer, "Unknown operator, or qualifier '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL:
                        sprintf(buffer, "Invalid symbol '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END: {
                        if (!error.message.empty())
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.row, error.column);
                    } break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP:
                        sprintf(buffer, "Unbalanced end to group on line %lu, column %lu.", error.row, error.column);
                    break;
                }
                message = buffer;
            }

            Exception(const Lexer::Error& error) : lexerError(error) {
                char buffer[512];
                switch (lexerError.type) {
                    case Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_NONE: break;
                    case Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_UNEXPECTED_END:
                        if (!error.message.empty())
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.message.data(), error.row, error.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.row, error.column);
                    break;
                }
                message = buffer;
            }

            const char* what() const noexcept {
               return message.data();
            }
        };


        Lexer lexer;

        Parser(const Context& context) : context(context), lexer(context, *this) { }


        Error validate(const Node& node) const;

        // Pops the last node in the stack, and then applies it as the last child of the node prior to it.
        bool popNode();
        // Pops nodes until it reaches a node of the given type.
        bool popNodeUntil(int type);

        Node parse(const char* buffer, size_t len);
        Node parse(const string& str) {
            return parse(str.data(), str.size());
        }
    };
}

#endif
