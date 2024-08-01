#ifndef LIQUIDPARSER_H
#define LIQUIDPARSER_H

#include "common.h"
#include "lexer.h"

namespace Liquid {

    struct NodeType;
    struct Variable;
    struct FilterNodeType;

    struct Parser {
        const Context& context;

        struct Error : LiquidParserError {
            typedef LiquidParserErrorType Type;

            Error() {
                this->type = Type::LIQUID_PARSER_ERROR_TYPE_NONE;
                details.column = 0;
                details.line = 0;
                details.args[0][0] = 0;
            }
            Error(const Error& error) = default;
            Error(Error&& error) = default;
            operator bool() const { return type != Type::LIQUID_PARSER_ERROR_TYPE_NONE; }

            template <class T>
            Error(T& lexer, Type type, const std::string& arg0 = "", const std::string& arg1 = "", const std::string& arg2 = "", const std::string& arg3 = "", const std::string& arg4 = "") {
                this->type = type;
                details.column = lexer.column;
                details.line = lexer.line;
                strncpy(details.args[0], arg0.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[0][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
                strncpy(details.args[1], arg1.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[1][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
                strncpy(details.args[2], arg2.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[2][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
                strncpy(details.args[3], arg3.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[3][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
                strncpy(details.args[4], arg4.c_str(), LIQUID_ERROR_ARG_MAX_LENGTH-1);
                details.args[4][LIQUID_ERROR_ARG_MAX_LENGTH-1] = 0;
            }

            static string english(LiquidParserError error) {
                char buffer[512];
                switch (error.type) {
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_NONE: break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG:
                        sprintf(buffer, "Unknown tag '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR:
                        sprintf(buffer, "Unknown operator '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER:
                        sprintf(buffer, "Unknown operator, or qualifier '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_OPERAND:
                        sprintf(buffer, "Unexpected operand for qualifier '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS:
                        sprintf(buffer, "Invalid arguments for '%s' on line %lu, column %lu; expected %s, got %s.", error.details.args[0], error.details.line, error.details.column, error.details.args[0], error.details.args[1]);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_QUALIFIER:
                        sprintf(buffer, "Invalid qualifier for '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER:
                        sprintf(buffer, "Unknown filter '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL:
                        sprintf(buffer, "Invalid symbol '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END: {
                        if (error.details.args[0][0])
                            sprintf(buffer, "Unexpected end to block '%s' on line %lu, column %lu.", error.details.args[0], error.details.line, error.details.column);
                        else
                            sprintf(buffer, "Unexpected end to block on line %lu, column %lu.", error.details.line, error.details.column);
                    } break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP:
                        sprintf(buffer, "Unbalanced end to group on line %lu, column %lu.", error.details.line, error.details.column);
                    break;
                    case Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_PARSE_DEPTH_EXCEEDED:
                        sprintf(buffer, "Parse depth exceeded on line %lu, column %lu.", error.details.line, error.details.column);
                    break;
                }
                return string(buffer);
            }
        };



        enum class State {
            NODE,
            ARGUMENT,
            // An error state that only occurs when we need to basically ignore everything until we get out of argument context.
            // Alows us to continue to parse, even if one tag is messed up.
            IGNORE_UNTIL_BLOCK_END,
            // Specifically for the {% liquid %} *rolls eyes*.
            LIQUID_NODE,
            LIQUID_ARGUMENT
        };
        enum class EFilterState {
            UNSET,
            COLON,
            NAME,
            ARGUMENTS,
            QUALIFIERS
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

        // Any more depth than this, and we throw an error.
        unsigned int maximumParseDepth = 100;

        void pushError(const Error& error) {
            errors.push_back(error);
        }

        struct Lexer : Liquid::Lexer<Lexer> {
            Parser& parser;
            typedef Liquid::Lexer<Lexer> SUPER;

            bool newline();
            bool literal(const char* str, size_t len);
            bool dot();
            bool colon();
            bool comma();

            bool endOutputContext();
            bool endTagContext();

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
            string englishDefault;

            Exception(const vector<Parser::Error>& errors) : parserErrors(errors) {
                englishDefault = Parser::Error::english(parserErrors[errors.size()-1]).c_str();
            }
            Exception(const Lexer::Error& error) : lexerError(error) {
                englishDefault = Lexer::Error::english(lexerError);

            }
            const char* what() const noexcept override {
                return englishDefault.c_str();
            }
        };


        Lexer lexer;
        string file;

        Parser(const Context& context) : context(context), lexer(context, *this) { }


        Error validate(const Node& node) const;

        bool pushNode(unique_ptr<Node> node, bool expectingNode = false);
        // Pops the last node in the stack, and then applies it as the last child of the node prior to it.
        bool popNode();
        // Pops nodes until it reaches a node of the given type.
        bool popNodeUntil(NodeType::Type type);
        bool hasNode(NodeType::Type type) const;
        bool hasNode(const NodeType* type) const;

        Node parseArgument(const char* buffer, size_t len);
        Node parseArgument(const std::string& str) { return parseArgument(str.data(), str.size()); }

        // Determines whether or not this is an argument, or a text, and then returns whichever it thinks makes more sense.
        Node parseAppropriate(const char* buffer, size_t len, const std::string& file = "");
        Node parseAppropriate(const std::string& str, const std::string& file = "") { return parseAppropriate(str.data(), str.size()); }

        Node parse(const char* buffer, size_t len, const std::string& file = "");
        Node parse(const string& str, const std::string& file = "") {
            return parse(str.data(), str.size(), file);
        }

        // Unparses the tree into text. Useful when used with optimization.
        void unparse(const Node& node, std::string& target, Parser::State state = Parser::State::NODE);
        std::string unparse(const Node& node) { std::string target; unparse(node, target); return target; }

        const FilterNodeType* getFilterType(const std::string& opName) const;
    };
}

#endif
