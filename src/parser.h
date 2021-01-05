#ifndef LIQUIDPARSER_H
#define LIQUIDPARSER_H

#include "common.h"
#include "lexer.h"

namespace Liquid {

    struct NodeType;
    struct Variable;

    struct Parser {
        const Context& context;

        struct Error : LiquidParserError {
            typedef LiquidParserErrorType Type;

            Error() {
                this->type = Type::LIQUID_PARSER_ERROR_TYPE_NONE;
                details.column = 0;
                details.row = 0;
                details.message[0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;
            operator bool() const { return type != Type::LIQUID_PARSER_ERROR_TYPE_NONE; }

            template <class T>
            Error(T& lexer, Type type, const std::string& message = "") {
                this->type = type;
                details.column = lexer.column;
                details.row = lexer.row;
                strcpy(details.message, message.data());
            }

            static string english(LiquidParserError error) {
                char buffer[512];
                switch (error.type) {
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_NONE: break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG:
                        sprintf(buffer, "Unknown tag '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR:
                        sprintf(buffer, "Unknown operator '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER:
                        sprintf(buffer, "Unknown operator, or qualifier '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_OPERAND:
                        sprintf(buffer, "Unexpected operand for qualifier '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS:
                        sprintf(buffer, "Invalid arguments for '%s' on line %lu, column %lu; expected %d, got %d.", error.details.message, error.details.row, error.details.column, 0, 0);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL:
                        sprintf(buffer, "Invalid symbol '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END: {
                        if (error.details.message[0])
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.details.message, error.details.row, error.details.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.details.row, error.details.column);
                    } break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP:
                        sprintf(buffer, "Unbalanced end to group on line %lu, column %lu.", error.details.row, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_PARSE_DEPTH_EXCEEDED:
                        sprintf(buffer, "Parse depth exceeded on line %lu, column %lu.", error.details.row, error.details.column);
                    break;
                }
                return string(buffer);
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
        // Any more depth than this, and we throw an error.
        unsigned int maximumParseDepth = 100;

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
            vector<Parser::Error> parserErrors;
            Lexer::Error lexerError;
            Exception(const vector<Parser::Error>& errors) : parserErrors(errors) { }
            Exception(const Lexer::Error& error) : lexerError(error) { }
            const char* what() const noexcept {
                if (lexerError)
                    return Lexer::Error::english(lexerError).c_str();
                if (parserErrors.size() > 0)
                    return Parser::Error::english(parserErrors[0]).c_str();
                return nullptr;
            }
        };


        Lexer lexer;

        Parser(const Context& context) : context(context), lexer(context, *this) { }


        Error validate(const Node& node) const;

        bool pushNode(unique_ptr<Node> node, bool expectingNode = false);
        // Pops the last node in the stack, and then applies it as the last child of the node prior to it.
        bool popNode();
        // Pops nodes until it reaches a node of the given type.
        bool popNodeUntil(NodeType::Type type);

        Node parse(const char* buffer, size_t len);
        Node parse(const string& str) {
            return parse(str.data(), str.size());
        }
    };
}

#endif
