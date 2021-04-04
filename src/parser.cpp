#include "context.h"
#include "parser.h"

namespace Liquid {

    bool Parser::Lexer::colon() {
        if (parser.state == Parser::State::IGNORE_UNTIL_BLOCK_END)
            return true;
        if (parser.filterState == Parser::EFilterState::COLON) {
            parser.filterState = Parser::EFilterState::ARGUMENTS;
            parser.nodes.back()->children.push_back(nullptr);
            return true;
        }
        if (parser.filterState == Parser::EFilterState::ARGUMENTS) {
            auto qualifierNode = make_unique<Node>(context.getFilterWildcardQualifierNodeType());
            // Check to see if the parent filter has the ability to have wildcard qualifiers.
            for (auto it = parser.nodes.rbegin(); it != parser.nodes.rend(); ++it) {
                if ((*it)->type && (*it)->type->type == NodeType::Type::FILTER) {
                    if (!static_cast<const FilterNodeType*>((*it)->type)->allowsWildcardQualifiers) {
                        parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_QUALIFIER, (*it)->type->symbol));
                        return false;
                    }
                    break;
                }
            }
            // Only applies if the variable has a single underlying string that qualifies it.
            if (parser.nodes.back()->children.size() != 1 || parser.nodes.back()->children.front()->type || parser.nodes.back()->children.front()->variant.type != Variant::Type::STRING) {
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ":"));
                return false;
            }
            qualifierNode->children.push_back(move(parser.nodes.back()));
            qualifierNode->children.push_back(nullptr);
            parser.nodes.back() = move(qualifierNode);
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
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ","));
                return false;
            break;
            case Parser::State::LIQUID_ARGUMENT:
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
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "."));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT:
                if (!parser.nodes.back()->type || (parser.nodes.back()->type->type != NodeType::Type::VARIABLE && parser.nodes.back()->type->type != NodeType::Type::DOT_FILTER)) {
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
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "["));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT:
                if (!parser.nodes.back()->type || parser.nodes.back()->type->type != NodeType::Type::VARIABLE) {
                    // In this case, we're looking an array literal. Check to see if those are enabled.
                    if (parser.context.disallowArrayLiterals) {
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
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "]"));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT: {
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
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, std::string(str, len)));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT:
                return parser.pushNode(move(make_unique<Node>(Variant(std::string(str, len)))));
            break;
        }
        return true;
    }

    bool Parser::Lexer::integer(long long i) {
        switch (parser.state) {
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, std::to_string(i)));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT: {
                return parser.pushNode(move(make_unique<Node>(Variant(i))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::floating(double f) {
        switch (parser.state) {
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::NODE:
            case Parser::State::LIQUID_NODE:
                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, std::to_string(f)));
                return false;
            break;
            case Parser::State::ARGUMENT:
            case Parser::State::LIQUID_ARGUMENT: {
                return parser.pushNode(move(make_unique<Node>(Variant(f))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::newline() {
        if (parser.state == Parser::State::LIQUID_ARGUMENT) {
            if (parser.hasNode(NodeType::Type::OUTPUT))
                endOutputContext();
            else
                endTagContext();
            parser.state = Parser::State::LIQUID_NODE;
        }
        ++line;
        column = 0;
        return true;
    }

    bool Parser::Lexer::literal(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::IGNORE_UNTIL_BLOCK_END:
                return true;
            case Parser::State::LIQUID_NODE:
            case Parser::State::NODE: {
                switch (this->state) {
                    case SUPER::State::CONTROL: {
                        if (len > 3 && strncmp(str, "end", 3) == 0) {
                            std::string typeName = std::string(&str[3], len - 3);
                            const TagNodeType* type = static_cast<const TagNodeType*>(SUPER::context.getTagType(typeName));

                            if (!type || type->composition == TagNodeType::Composition::FREE) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG, std::string(str, len)));
                                return false;
                            }
                            if (!parser.popNodeUntil(NodeType::Type::TAG) || parser.nodes.back()->type != type) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_TAG, std::string(str, len)));
                                return false;
                            }
                            parser.state = Parser::State::ARGUMENT;
                            parser.blockType = Parser::EBlockType::END;
                        } else {
                            std::string typeName = std::string(str, len);
                            if (typeName == "liquid") {
                                parser.state = Parser::State::LIQUID_NODE;
                                return true;
                            } else if (parser.state == Parser::State::LIQUID_NODE && typeName == "echo") {
                                parser.state = Parser::State::LIQUID_ARGUMENT;
                                return parser.pushNode(make_unique<Node>(context.getOutputNodeType()), true) && parser.pushNode(make_unique<Node>(context.getArgumentsNodeType()), true);
                            }

                            const TagNodeType* type = SUPER::context.getTagType(typeName);
                            if (!type && parser.nodes.size() > 0) {
                                for (auto it = parser.nodes.rbegin(); it != parser.nodes.rend(); ++it) {
                                    if ((*it)->type && (*it)->type->type == NodeType::Type::TAG) {
                                        auto& intermediates = static_cast<const TagNodeType*>((*it)->type)->intermediates;
                                        auto it = intermediates.find(typeName);
                                        if (it != intermediates.end()) {
                                            type = static_cast<TagNodeType*>(it->second.get());
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
                            if (type->composition == TagNodeType::Composition::LEXING_HALT)
                                beginHalt(typeName.c_str(), typeName.size());
                            parser.state = parser.state == Parser::State::LIQUID_NODE ? Parser::State::LIQUID_ARGUMENT : Parser::State::ARGUMENT;
                            return parser.pushNode(std::make_unique<Node>(type), true) && parser.pushNode(std::make_unique<Node>(context.getArgumentsNodeType()), true);
                        }
                    } break;
                    case SUPER::State::OUTPUT:
                    case SUPER::State::HALT:
                    case SUPER::State::CONTROL_HALT:
                    case SUPER::State::INITIAL:
                        assert(parser.nodes.back()->type == SUPER::context.getConcatenationNodeType());
                        parser.nodes.back()->children.push_back(std::make_unique<Node>(std::string(str, len)));
                    break;
                }
            } break;
            case Parser::State::LIQUID_ARGUMENT:
            case Parser::State::ARGUMENT: {
                std::string opName = std::string(str, len);
                const LiteralType* type = SUPER::context.getLiteralType(opName);
                if (type)
                    return parser.pushNode(make_unique<Node>(type->value));
                auto& lastNode = parser.nodes.back();
                if (lastNode->type && (lastNode->type->type == NodeType::Type::VARIABLE || lastNode->type->type == NodeType::Type::DOT_FILTER) && !lastNode->children.back().get()) {
                    // Check for dot filters.
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
                            bool unknown = !op;
                            if (unknown) {
                                parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_FILTER, opName));
                                op = static_cast<const FilterNodeType*>(context.getUnknownFilterNodeType());
                            }
                            auto operatorNode = make_unique<Node>(op);
                            if (unknown)
                                operatorNode->children.push_back(make_unique<Node>(Variant(opName)));
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
                                const ContextualNodeType* contextualType = nullptr;
                                for (auto it = parser.nodes.rbegin(); it != parser.nodes.rend(); ++it) {
                                    if ((*it)->type == context.getConcatenationNodeType())
                                        break;
                                    if ((*it)->type) {
                                        if ((*it)->type->type == NodeType::Type::TAG || (*it)->type->type == NodeType::Type::OUTPUT) {
                                            contextualType = static_cast<const ContextualNodeType*>((*it)->type);
                                            break;
                                        }
                                    }
                                }
                                auto it = contextualType->operators.find(opName);
                                if (it == contextualType->operators.end()) {
                                    // If no operator found, check for a specified qualifier.
                                    const TagNodeType::QualifierNodeType* qualifier = nullptr;
                                    if (contextualType && contextualType->type == NodeType::Type::TAG) {
                                        auto it = static_cast<const TagNodeType*>(contextualType)->qualifiers.find(opName);
                                        if (it != static_cast<const TagNodeType*>(contextualType)->qualifiers.end())
                                            qualifier = static_cast<TagNodeType::QualifierNodeType*>(it->second.get());
                                    }
                                    if (!qualifier) {
                                        parser.pushError(Parser::Error(*this, contextualType && contextualType->type == NodeType::Type::TAG ? Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR_OR_QUALIFIER : Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNKNOWN_OPERATOR, opName));
                                        parser.state = Parser::State::IGNORE_UNTIL_BLOCK_END;
                                        return true;
                                    }
                                    parser.popNodeUntil(NodeType::Type::ARGUMENTS);
                                    parser.nodes.back()->children.push_back(nullptr);
                                    if (!parser.pushNode(std::make_unique<Node>(qualifier)))
                                        return false;
                                    return true;
                                } else
                                    op = static_cast<const OperatorNodeType*>(it->second.get());
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
        if (parser.state == Parser::State::IGNORE_UNTIL_BLOCK_END)
            return true;
        if (context.disallowGroupingOutsideAssign && !parser.hasNode(context.getTagType("assign"))) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, "("));
            return true;
        }
        return parser.pushNode(make_unique<Node>(context.getGroupNodeType()), true);
    }
    bool Parser::Lexer::closeParenthesis() {
        if (parser.state == Parser::State::IGNORE_UNTIL_BLOCK_END)
            return true;
        if (context.disallowGroupingOutsideAssign && !parser.hasNode(context.getTagType("assign"))) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_SYMBOL, ")"));
            return true;
        }
        if (!parser.popNodeUntil(NodeType::Type::GROUP)) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNBALANCED_GROUP));
            return false;
        }
        return true;
    }


    bool Parser::Lexer::startOutputBlock(bool suppress) {
        SUPER::startOutputBlock(suppress);
        parser.state = Parser::State::ARGUMENT;
        return parser.pushNode(make_unique<Node>(context.getOutputNodeType()), true) && parser.pushNode(make_unique<Node>(context.getArgumentsNodeType()), true);
    }

    bool Parser::pushNode(unique_ptr<Node> node, bool expectingNode) {
        node->line = lexer.line;
        node->column = lexer.column;
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

        if (argumentNode->type) {
            if (!argumentNode->type->validate(*this, *argumentNode.get())) {
                argumentNode = make_unique<Node>(Variant(""));
                return true;
            }
        }

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

    bool Parser::hasNode(NodeType::Type type) const {
        for (auto& node : nodes) {
            if (node->type && node->type->type == type)
                return true;
        }
        return false;
    }

    bool Parser::hasNode(const NodeType* type) const {
        for (auto& node : nodes) {
            if (node->type == type)
                return true;
        }
        return false;
    }

    bool Parser::Lexer::endOutputContext() {
        parser.filterState = Parser::EFilterState::UNSET;
        if (parser.state == Parser::State::NODE) {
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END));
            return false;
        }
        if (!parser.popNodeUntil(NodeType::Type::OUTPUT))
            return false;
        unique_ptr<Node> outputBlock = move(parser.nodes.back());
        parser.nodes.pop_back();
        assert(parser.nodes.back()->type && parser.nodes.back()->type == context.getConcatenationNodeType());
        parser.nodes.back()->children.push_back(move(outputBlock));
        return true;
    }

    bool Parser::Lexer::endOutputBlock(bool suppress) {
        if (!endOutputContext())
            return false;
        parser.state = Parser::State::NODE;
        SUPER::endOutputBlock(suppress);
        return true;
    }


    bool Parser::Lexer::endTagContext() {
        parser.filterState = Parser::EFilterState::UNSET;
        if (parser.state == Parser::State::NODE) {
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
        auto& arguments = controlBlock->children[0];
        const TagNodeType* controlType = static_cast<const TagNodeType*>(controlBlock->type);
        if (controlType->minArguments != -1 && (int)arguments->children.size() < controlType->minArguments)
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS, controlType->symbol, std::to_string(controlType->minArguments), std::to_string(arguments->children.size())));
        else if (controlType->maxArguments != -1 && (int)arguments->children.size() > controlType->maxArguments)
            parser.pushError(Parser::Error(*this, Parser::Error::Type::LIQUID_PARSER_ERROR_TYPE_INVALID_ARGUMENTS, controlType->symbol, std::to_string(controlType->minArguments), std::to_string(arguments->children.size())));
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

    bool Parser::Lexer::endControlBlock(bool suppress) {
        if (!endTagContext())
            return false;
        parser.state = Parser::State::NODE;
        parser.blockType = Parser::EBlockType::NONE;
        SUPER::endControlBlock(suppress);
        return true;
    }

    Node Parser::parseArgument(const char* buffer, size_t len) {
        errors.clear();
        nodes.clear();

        filterState = EFilterState::UNSET;
        blockType = EBlockType::INTERMEDIATE;
        state = State::ARGUMENT;

        pushNode(make_unique<Node>(context.getOutputNodeType()), true);
        Lexer::Error error = lexer.parse(buffer, len, Lexer::State::OUTPUT);
        if (error.type != Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_NONE)
            throw Exception(error);
        if (errors.size() > 0)
            throw Exception(errors);
        if (!popNodeUntil(NodeType::Type::OUTPUT))
            return Node();
        assert(nodes.size() == 1);
        Node node = move(*nodes.back()->children[0].get());
        nodes.clear();
        return node;
    }


    Node Parser::parseAppropriate(const char* buffer, size_t len, const std::string& file) {
        bool hasBraces = false;
        for (size_t i = 1; i < len; ++i) {
            if (buffer[i-1] == '{' && (buffer[i] == '{' || buffer[i] == '%')) {
                hasBraces = true;
                break;
            }
        }
        return hasBraces ? parse(buffer, len, file) : parseArgument(buffer, len);
    }

    Node Parser::parse(const char* buffer, size_t len, const string& file) {
        errors.clear();
        nodes.clear();
        filterState = EFilterState::UNSET;
        blockType = EBlockType::NONE;
        state = State::NODE;

        pushNode(make_unique<Node>(context.getConcatenationNodeType()), false);
        Lexer::Error error = lexer.parse(buffer, len);
        if (error.type != Lexer::Error::Type::LIQUID_LEXER_ERROR_TYPE_NONE)
            throw Exception(error);
        if (nodes.size() > 1) {
            if (errors.size() > 0)
                throw Exception(errors);
            for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
                if ((*it)->type && !(*it)->type->symbol.empty())
                    throw Exception({ Liquid::Parser::Error(lexer, LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END, (*it)->type->symbol) });
            }
            throw Exception({ Liquid::Parser::Error(lexer, LIQUID_PARSER_ERROR_TYPE_UNEXPECTED_END) });
        }
        assert(nodes.size() == 1);
        if (file.empty()) {
            Node node = move(*nodes.back().get());
            nodes.clear();
            return node;
        }
        auto node = Node(context.getContextBoundaryNodeType());
        node.children.push_back(make_unique<Node>(Variant(file)));
        node.children.push_back(std::move(nodes.back()));
        nodes.clear();
        return node;
    }

    void Parser::unparse(const Node& node, string& target, Parser::State state) {
        if (node.type) {
            switch (node.type->type) {
                case Liquid::NodeType::Type::TAG: {
                    const TagNodeType* tagNodeType = static_cast<const TagNodeType*>(node.type);
                    if (tagNodeType->composition == Liquid::TagNodeType::Composition::FREE) {
                        target.append("{% ");
                        target.append(tagNodeType->symbol);
                        bool latch = false;
                        for (auto& child : node.children[0]->children) {
                            target.push_back(' ');
                            if (latch)
                                target.push_back(',');
                            unparse(*child.get(), target, Parser::State::ARGUMENT);
                            latch = true;
                        }
                        target.append(" %}");
                    } else {
                        const TagNodeType* tagNodeType = static_cast<const TagNodeType*>(node.type);
                        target.append("{% ");
                        target.append(tagNodeType->symbol);
                        bool latch = false;
                        for (auto& child : node.children[0]->children) {
                            target.push_back(' ');
                            if (latch)
                                target.push_back(',');
                            unparse(*child.get(), target, Parser::State::ARGUMENT);
                            latch = true;
                        }
                        target.append(" %}");
                        for (size_t i = 1; i < node.children.size(); ++i)
                            unparse(*node.children[i].get(), target, Parser::State::NODE);
                        target.append("{% end");
                        target.append(tagNodeType->symbol);
                        target.append(" %}");
                    }
                } break;
                case Liquid::NodeType::Type::GROUP:
                    target.append("(");
                    unparse(*node.children[0].get(), target, Parser::State::ARGUMENT);
                    target.append(")");
                break;
                case Liquid::NodeType::GROUP_DEREFERENCE:
                    target.append("[");
                    unparse(*node.children[0].get(), target, Parser::State::ARGUMENT);
                    target.append("]");
                break;
                case Liquid::NodeType::Type::OUTPUT:
                    target.append("{{ ");
                    unparse(*node.children[0]->children[0].get(), target, Parser::State::ARGUMENT);
                    target.append(" }}");
                break;
                case Liquid::NodeType::Type::OPERATOR: {
                    const OperatorNodeType* operatorNodeType = static_cast<const OperatorNodeType*>(node.type);
                    if (operatorNodeType == context.getConcatenationNodeType()) {
                        for (auto& child : node.children)
                            unparse(*child.get(), target, Parser::State::NODE);
                    } else {
                        switch (operatorNodeType->arity) {
                            case OperatorNodeType::Arity::UNARY:
                                target.append(operatorNodeType->symbol);
                                unparse(*node.children[0].get(), target, Parser::State::ARGUMENT);
                            break;
                            case OperatorNodeType::Arity::BINARY:
                                unparse(*node.children[0].get(), target, Parser::State::ARGUMENT);
                                target.push_back(' ');
                                target.append(operatorNodeType->symbol);
                                target.push_back(' ');
                                unparse(*node.children[1].get(), target, Parser::State::ARGUMENT);
                            break;
                            default:
                                assert(false);
                            break;
                        }
                    }
                } break;
                case Liquid::NodeType::Type::VARIABLE:
                    for (size_t i = 0; i < node.children.size(); ++i) {
                        if (i > 0) {
                            string result;
                            unparse(*node.children[i].get(), result, Parser::State::ARGUMENT);
                            if (node.children[i]->type || result[0] == '"' || result[0] == '\'') {
                                if (result.size() > 0 && !node.children[i]->type && result.find_first_of("\"'", 1) == result.size()-1) {
                                    target.push_back('.');
                                    target.append(result, 1, result.size()-2);
                                } else {
                                    if (result[0] != '[')
                                        target.push_back('[');
                                    target.append(result);
                                    if (result[0] != '[')
                                        target.push_back(']');
                                }
                            } else {
                                target.push_back('.');
                                target.append(result);
                            }
                        } else {
                            assert(node.children[0]->variant.type == Variant::Type::STRING);
                            target.append(node.children[0]->variant.s);
                        }
                    }
                break;
                case Liquid::NodeType::Type::FILTER: {
                    const FilterNodeType* filterNodeType = static_cast<const FilterNodeType*>(node.type);
                    int offset = 0;
                    if (filterNodeType == context.getUnknownFilterNodeType()) {
                        offset = 1;
                        unparse(*node.children[offset].get(), target, Parser::State::ARGUMENT);
                        target.append(" | ");
                        target.append(node.children[0].get()->variant.s);
                    } else {
                        unparse(*node.children[offset].get(), target, Parser::State::ARGUMENT);
                        target.append(" | ");
                        target.append(filterNodeType->symbol);
                    }
                    if (node.children[offset+1]->children.size() > 0) {
                        target.append(": ");
                        for (size_t i = 0; i < node.children[1]->children.size(); ++i) {
                            if (i > 0)
                                target.append(", ");
                            unparse(*node.children[1]->children[i].get(), target, Parser::State::ARGUMENT);
                        }
                    }
                } break;
                case Liquid::NodeType::Type::CONTEXTUAL:
                    unparse(*node.children[1].get(), target, state);
                break;
                default:
                    assert(false);
                break;
            }
        } else {
            if (state == Parser::State::NODE) {
                target.append(node.getString());
            } else {
                switch (node.variant.type) {
                    case Variant::Type::STRING: {
                        target.push_back('"');
                        size_t start = 0;
                        while (true) {
                            size_t i = node.variant.s.find('"', start);
                            if (i == string::npos)
                                break;
                            target.append(node.variant.s, start, i - start);
                            target.push_back('\\');
                            target.push_back('"');
                            start = i+1;
                        }
                        target.append(node.variant.s, start, node.variant.s.size() - start);
                        target.push_back('"');
                    } break;
                    default:
                        target.append(node.getString());
                    break;
                }
            }
        }
    }
}
