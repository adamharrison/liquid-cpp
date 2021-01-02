#include "context.h"
#include "parser.h"

namespace Liquid {

    Parser::Error Parser::validate(const Node& node) const {
        /*if (node.type)
            return node.type->validate(context, node);*/
        return Error();
    }

    bool Parser::Lexer::colon() {
        if (parser.filterState == Parser::EFilterState::COLON) {
            parser.filterState = Parser::EFilterState::ARGUMENTS;
            parser.nodes.back()->children.push_back(nullptr);
            return true;
        }
        if (parser.nodes.back()->type && parser.nodes.back()->type->type == NodeType::Type::QUALIFIER) {
            if (static_cast<const TagNodeType::QualifierNodeType*>(parser.nodes.back()->type)->arity == TagNodeType::QualifierNodeType::Arity::NONARY) {
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_OPERAND, parser.nodes.back()->type->symbol));
                return false;
            }
            parser.nodes.back()->children.push_back(nullptr);
            return true;
        }
        if (parser.nodes.back()->type != context.getVariableNodeType() || parser.nodes.back()->children.size() != 1) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ":"));
            return false;
        }
        return true;
    }

    bool Parser::Lexer::comma() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ","));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (parser.nodes.size() > 2 && parser.nodes[parser.nodes.size()-2]->type && parser.nodes[parser.nodes.size()-2]->type->type == NodeType::Type::ARRAY_LITERAL) {
                    if (!parser.popNode())
                        return false;
                    parser.nodes.back()->children.push_back(nullptr);
                    return true;
                }
                if (!parser.popNodeUntil(NodeType::Type::ARGUMENTS)) {
                    parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ","));
                    return false;
                }
                parser.nodes.back()->children.push_back(nullptr);
                return true;
            break;
        }
        return true;
    }

    bool Parser::Lexer::dot() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "."));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.nodes.back()->type || parser.nodes.back()->type->type != NodeType::Type::VARIABLE) {
                    parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "."));
                    return false;
                }
                parser.nodes.back()->children.push_back(nullptr);
                return true;
            break;
        }
        return true;
    }


    bool Parser::Lexer::startVariableDereference() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.nodes.back()->type || parser.nodes.back()->type->type != NodeType::Type::VARIABLE) {
                    // In this case, we're looking an array literal. Check to see if those are enabled.
                    if (!parser.context.allowArrayLiterals) {
                        parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "["));
                        return false;
                    }
                    return parser.pushNode(make_unique<Node>(context.getArrayLiteralNodeType()), true);
                }
                parser.nodes.back()->children.push_back(nullptr);
                return parser.pushNode(make_unique<Node>(context.getGroupDereferenceNodeType()), true);
            break;
        }
        return true;
    }

    bool Parser::Lexer::endVariableDereference() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "]"));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                int i;
                for (i = parser.nodes.size()-1; i > 0; --i) {
                    if (parser.nodes[i]->type) {
                        if (parser.nodes[i]->type->type == NodeType::Type::GROUP_DEREFERENCE || parser.nodes[i]->type->type == NodeType::Type::ARRAY_LITERAL)
                            break;
                    }
                }
                // Array literal ending. If we're a 0 length array, remove the expeaction of an element.
                if (i >= 0 && parser.nodes[i]->type->type == NodeType::Type::ARRAY_LITERAL) {
                    if (i == (int)parser.nodes.size() - 1 && parser.nodes[i]->children.size() == 1 && parser.nodes[i]->children[0].get() == nullptr)
                        parser.nodes[i]->children.resize(0);
                    if (!parser.popNodeUntil(NodeType::Type::ARRAY_LITERAL)) {
                        parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP));
                        return false;
                    }
                    return true;
                }
                if (!parser.popNodeUntil(NodeType::Type::GROUP_DEREFERENCE)) {
                    parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP));
                    return false;
                }
                parser.popNode();
            } break;
        }
        return true;
    }

    bool Parser::Lexer::string(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                return parser.pushNode(move(make_unique<Node>(Variant(std::string(str, len)))));
            break;
        }
        return true;
    }

    bool Parser::Lexer::integer(long long i) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                return parser.pushNode(move(make_unique<Node>(Variant(i))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::floating(double f) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                return parser.pushNode(move(make_unique<Node>(Variant(f))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::literal(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE: {
                switch (this->state) {
                    case SUPER::State::CONTROL: {
                        if (len > 3 && strncmp(str, "end", 3) == 0) {
                            std::string typeName = std::string(&str[3], len - 3);
                            const TagNodeType* type = static_cast<const TagNodeType*>(SUPER::context.getTagType(typeName));

                            if (!type || type->composition != TagNodeType::Composition::ENCLOSED) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG, std::string(str, len)));
                                return false;
                            }
                            if (!parser.popNodeUntil(NodeType::Type::TAG) || parser.nodes.back()->type != type) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END, std::string(str, len)));
                                return false;
                            }
                            parser.state = Parser::State::ARGUMENT;
                            parser.blockType = Parser::EBlockType::END;
                        } else {
                            std::string typeName = std::string(str, len);
                            const NodeType* type = SUPER::context.getTagType(typeName);
                            if (!type && parser.nodes.size() > 0) {
                                for (auto it = parser.nodes.rbegin(); it != parser.nodes.rend(); ++it) {
                                    if ((*it)->type && (*it)->type->type == NodeType::Type::TAG) {
                                        auto& intermediates = static_cast<const TagNodeType*>((*it)->type)->intermediates;
                                        auto it = intermediates.find(typeName);
                                        if (it != intermediates.end()) {
                                            type = it->second.get();
                                            // Pop off the concatenation node, and apply this as the next arugment in the parent node.
                                            parser.popNode();
                                            parser.blockType = Parser::EBlockType::INTERMEDIATE;
                                        }
                                        break;
                                    }
                                }
                            }
                            if (!type) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG, typeName));
                                return false;
                            }
                            parser.state = Parser::State::ARGUMENT;
                            return parser.pushNode(std::make_unique<Node>(type), true) && parser.pushNode(std::make_unique<Node>(context.getArgumentsNodeType()), true);
                        }
                    } break;
                    case SUPER::State::OUTPUT:
                    case SUPER::State::RAW:
                    case SUPER::State::INITIAL:
                        assert(parser.nodes.back()->type == SUPER::context.getConcatenationNodeType());
                        parser.nodes.back()->children.push_back(std::make_unique<Node>(std::string(str, len)));
                    break;
                }
            } break;
            case Parser::State::ARGUMENT: {
                if (strncmp(str, "true", len) == 0)
                    return parser.pushNode(move(make_unique<Node>(Variant(true))));
                else if (strncmp(str, "false", len) == 0)
                    return parser.pushNode(move(make_unique<Node>(Variant(false))));
                else if (strncmp(str, "null", len) == 0)
                    return parser.pushNode(move(make_unique<Node>(Variant(nullptr))));
                auto& lastNode = parser.nodes.back();
                if (lastNode->type && lastNode->type->type == NodeType::Type::VARIABLE && !lastNode->children.back().get()) {
                    // Check for dot filters.
                    std::string opName = std::string(str, len);
                    const DotFilterNodeType* op = context.getDotFilterType(opName);
                    if (op) {
                        lastNode->children.pop_back();
                        auto operatorNode = make_unique<Node>(op);
                        operatorNode->children.push_back(move(lastNode));
                        parser.nodes.back() = move(operatorNode);
                    } else {
                        lastNode->children.back() = move(make_unique<Node>(Variant(move(opName))));
                    }
                } else {
                    if (lastNode->type && lastNode->children.size() > 0 && !lastNode->children.back().get()) {
                        // Check for unray operators.
                        std::string opName = std::string(str, len);
                        const OperatorNodeType* op = context.getUnaryOperatorType(opName);
                        if (op) {
                            assert(op->fixness == OperatorNodeType::Fixness::PREFIX);
                            return parser.pushNode(make_unique<Node>(op), true);
                        } else {
                            unique_ptr<Node> node = make_unique<Node>(context.getVariableNodeType());
                            node->children.push_back(make_unique<Node>(Variant(opName)));
                            parser.nodes.push_back(move(node));
                        }
                    } else {
                        // Check for operators.
                        std::string opName = std::string(str, len);
                        if (opName == "|") {
                            // In the case where we're chaining filters, and there are no arguments; the precense of another | is enough to terminate this an popUntil the filter.
                            if (
                                ((parser.filterState == Parser::EFilterState::COLON || parser.filterState == Parser::EFilterState::ARGUMENTS) && !parser.popNodeUntil(NodeType::Type::FILTER)) ||
                                (parser.filterState != Parser::EFilterState::UNSET && parser.filterState != Parser::EFilterState::ARGUMENTS && parser.filterState != Parser::EFilterState::COLON)
                            ) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, opName));
                                return false;
                            }
                            parser.filterState = Parser::EFilterState::NAME;
                            return true;
                        }
                        if (parser.filterState == Parser::EFilterState::NAME) {
                            parser.filterState = Parser::EFilterState::COLON;
                            const FilterNodeType* op = context.getFilterType(opName);
                            if (!op) {
                                if (parser.treatUnknownFiltersAsErrors) {
                                    parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER, opName));
                                    return false;
                                } else {
                                    op = static_cast<const FilterNodeType*>(context.getUnknownFilterNodeType());
                                }
                            }
                            auto operatorNode = make_unique<Node>(op);
                            auto& parentNode = parser.nodes[parser.nodes.size()-2];
                            assert(parentNode->type);
                            unique_ptr<Node> variableNode = move(parser.nodes.back());
                            operatorNode->children.push_back(move(variableNode));
                            operatorNode->children.push_back(nullptr);
                            parser.nodes.back() = move(operatorNode);
                            parser.nodes.push_back(std::make_unique<Node>(context.getArgumentsNodeType()));
                        } else {
                            // It's either an operator, or, if we're part of a tag, a qualifier. Check both. Operators first.
                            const OperatorNodeType* op = context.getBinaryOperatorType(opName);
                            if (!op) {
                                // If no operator found, check for a specified qualifier.
                                const TagNodeType::QualifierNodeType* qualifier = nullptr;
                                bool hasTag = false;
                                if (parser.nodes.size() > 0) {
                                    for (auto it = parser.nodes.rbegin(); it != parser.nodes.rend(); ++it) {
                                        if ((*it)->type == context.getConcatenationNodeType())
                                            break;
                                        if ((*it)->type && (*it)->type->type == NodeType::Type::TAG) {
                                            hasTag = true;
                                            auto& qualifiers = static_cast<const TagNodeType*>((*it)->type)->qualifiers;
                                            auto it = qualifiers.find(opName);
                                            if (it != qualifiers.end())
                                                qualifier = static_cast<TagNodeType::QualifierNodeType*>(it->second.get());
                                            break;
                                        }
                                    }
                                }
                                if (!qualifier) {
                                    parser.pushError(Parser::Error(*this, hasTag ? Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER : Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR, opName));
                                    return false;
                                }
                                parser.popNodeUntil(NodeType::Type::ARGUMENTS);
                                parser.nodes.back()->children.push_back(nullptr);
                                if (!parser.pushNode(std::make_unique<Node>(qualifier)))
                                    return false;
                                return true;
                            }
                            assert(op->fixness == OperatorNodeType::Fixness::INFIX);
                            auto operatorNode = make_unique<Node>(op);
                            auto& parentNode = parser.nodes[parser.nodes.size()-2];

                            assert(parentNode->type);
                            if (parentNode->type->type == NodeType::Type::OPERATOR &&
                                static_cast<const OperatorNodeType*>(parentNode->type)->priority >= op->priority) {
                                // If we're of a lower priority, push operator and
                                parser.popNode();
                                unique_ptr<Node> rotatedOperator = move(parser.nodes.back());
                                operatorNode->children.push_back(move(rotatedOperator));
                                operatorNode->children.push_back(nullptr);
                                parser.nodes.back() = move(operatorNode);
                            } else {
                                unique_ptr<Node> variableNode = move(parser.nodes.back());
                                operatorNode->children.push_back(move(variableNode));
                                operatorNode->children.push_back(nullptr);
                                parser.nodes.back() = move(operatorNode);
                            }
                        }
                    }
                }
            } break;
        }
        return true;
    }


    bool Parser::Lexer::openParenthesis() {
        return parser.pushNode(make_unique<Node>(context.getGroupNodeType()), true);
    }
    bool Parser::Lexer::closeParenthesis() {
        if (!parser.popNodeUntil(NodeType::Type::GROUP)) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP));
            return false;
        }
        return true;
    }


    bool Parser::Lexer::startOutputBlock(bool suppress) {
        parser.state = Parser::State::ARGUMENT;
        return parser.pushNode(make_unique<Node>(context.getOutputNodeType()), true) && parser.pushNode(make_unique<Node>(context.getArgumentsNodeType()), true);
    }

    bool Parser::pushNode(unique_ptr<Node> node, bool expectingNode) {
        if (nodes.size() > maximumParseDepth) {
            pushError(Parser::Error(lexer, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_PARSE_DEPTH_EXCEEDED));
            return false;
        }
        nodes.push_back(move(node));
        if (expectingNode)
            nodes.back()->children.push_back(nullptr);
        return true;
    }

    bool Parser::popNode() {
        if (nodes.size() <= 1)
            return false;
        unique_ptr<Node> argumentNode = move(nodes.back());
        nodes.pop_back();

        validate(*argumentNode.get());

        if (nodes.back()->children.size() == 0 || nodes.back()->children.back().get())
            return false;
        nodes.back()->children.back() = move(argumentNode);
        return true;
    }

    bool Parser::popNodeUntil(NodeType::Type type) {
        while (true) {
            auto& node = nodes.back();
            if (node->type && node->type->type == type)
                return true;
            if (!popNode())
                return false;
        }
        return true;
    }

    bool Parser::Lexer::endOutputBlock(bool suppress) {
        parser.filterState = Parser::EFilterState::UNSET;
        if (parser.state != Parser::State::ARGUMENT) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END));
            return false;
        }
        if (!parser.popNodeUntil(NodeType::Type::OUTPUT))
            return false;
        unique_ptr<Node> outputBlock = move(parser.nodes.back());
        parser.nodes.pop_back();
        assert(parser.nodes.back()->type && parser.nodes.back()->type == context.getConcatenationNodeType());
        parser.nodes.back()->children.push_back(move(outputBlock));
        parser.state = Parser::State::NODE;
        return true;
    }


    bool Parser::Lexer::endControlBlock(bool suppress) {
        parser.filterState = Parser::EFilterState::UNSET;
        if (parser.state != Parser::State::ARGUMENT) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END));
            return false;
        }
        if (parser.blockType != Parser::EBlockType::END) {
            if (!parser.popNodeUntil(NodeType::Type::ARGUMENTS))
                return false;
            // Prune unescessary null for no-arg blocks.
            if (parser.nodes.back()->children.size() == 1 && parser.nodes.back()->children[0].get() == nullptr)
                parser.nodes.back()->children.clear();
        }
        if (!parser.popNodeUntil(NodeType::Type::TAG))
            return false;
        auto& controlBlock = parser.nodes.back();
        if (parser.blockType != Parser::EBlockType::NONE || static_cast<const TagNodeType*>(controlBlock->type)->composition == TagNodeType::Composition::FREE) {
            unique_ptr<Node> controlNode = move(parser.nodes.back());
            parser.nodes.pop_back();
            parser.nodes.back()->children.push_back(move(controlNode));
            if (parser.blockType == Parser::EBlockType::INTERMEDIATE) {
                parser.nodes.back()->children.push_back(nullptr);
                parser.nodes.push_back(make_unique<Node>(context.getConcatenationNodeType()));
            }
            assert(parser.nodes.back()->type && parser.nodes.back()->type == context.getConcatenationNodeType());
        } else {
            controlBlock->children.push_back(nullptr);
            parser.nodes.push_back(make_unique<Node>(context.getConcatenationNodeType()));
        }
        parser.state = Parser::State::NODE;
        parser.blockType = Parser::EBlockType::NONE;
        return true;
    }

    Node Parser::parse(const char* buffer, size_t len) {
        errors.clear();
        nodes.clear();
        filterState = EFilterState::UNSET;
        blockType = EBlockType::NONE;
        state = State::NODE;

        nodes.push_back(std::make_unique<Node>(context.getConcatenationNodeType()));
        Lexer::Error error = lexer.parse(buffer, len);
        if (errors.size() > 0)
            throw Exception(errors[0]);
        if (error.type != Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_NONE)
            throw Exception(error);
        if (nodes.size() > 1)
            throw Exception(Liquid::Parser::Error(lexer, LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END));
        assert(nodes.size() == 1);
        Node node = move(*nodes.back().get());
        nodes.clear();
        return node;
    }
}
