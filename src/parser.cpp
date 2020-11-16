#include "context.h"
#include "parser.h"

namespace Liquid {

    Parser::Error Parser::Node::validate(const Context& context) const {
        if (type)
            return type->validate(context, *this);
        return Error();
    }

    // Performs implicit conversion.
    string Parser::Node::getString() {
        assert(type == nullptr);
        if (variant.type == Variant::Type::STRING)
            return variant.s;
        else if (variant.type == Variant::Type::FLOAT)
            return std::to_string(variant.f);
        else if (variant.type == Variant::Type::INT)
            return std::to_string(variant.i);
        return std::string();
    }

    bool Parser::Lexer::colon() {
        if (parser.filterState == Parser::EFilterState::COLON) {
            parser.filterState = Parser::EFilterState::ARGUMENTS;
            return true;
        }
        if (parser.nodes.back()->type != context.getVariableNodeType() || parser.nodes.back()->children.size() != 1) {
            parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, ":"));
            return false;
        }
        parser.nodes.back()->type = context.getNamedVariableNodeType();
        return true;
    }

    bool Parser::Lexer::comma() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, ","));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.popNodeUntil(NodeType::Type::ARGUMENTS)) {
                    parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, ","));
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
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, "."));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.nodes.back()->type || parser.nodes.back()->type->type != NodeType::Type::VARIABLE) {
                    parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, "."));
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
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.nodes.back()->type || parser.nodes.back()->type->type != NodeType::Type::VARIABLE) {
                    parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, "["));
                    return false;
                }
                parser.nodes.back()->children.push_back(nullptr);
                parser.nodes.push_back(make_unique<Node>(context.getGroupDereferenceNodeType()));
                parser.nodes.back()->children.push_back(nullptr);
            break;
        }
        return true;
    }

    bool Parser::Lexer::endVariableDereference() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, "]"));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (!parser.popNodeUntil(NodeType::Type::GROUP_DEREFERENCE)) {
                    parser.pushError(Error(*this, Error::Type::UNBALANCED_GROUP));
                    return false;
                }
                parser.popNode();
            break;
        }
        return true;
    }

    bool Parser::Lexer::string(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                parser.nodes.push_back(move(make_unique<Node>(Variant(std::string(str, len)))));
            break;
        }
        return true;
    }

    bool Parser::Lexer::integer(long long i) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                parser.nodes.push_back(move(make_unique<Node>(Variant(i))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::floating(double f) {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                parser.nodes.push_back(move(make_unique<Node>(Variant(f))));
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
                            const NodeType* type = SUPER::context.getTagType(typeName);

                            if (!type) {
                                parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, std::string(str, len)));
                                return false;
                            }
                            parser.popNodeUntil(NodeType::Type::TAG);
                            if (parser.nodes.back()->type != type) {
                                parser.pushError(Error(*this, Error::Type::UNEXPECTED_END, std::string(str, len)));
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
                                parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, typeName));
                                return false;
                            }
                            parser.state = Parser::State::ARGUMENT;
                            parser.nodes.push_back(std::make_unique<Node>(type));
                            parser.nodes.back()->children.push_back(nullptr);
                            parser.nodes.push_back(std::make_unique<Node>(context.getArgumentsNodeType()));
                            parser.nodes.back()->children.push_back(nullptr);
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
                auto& lastNode = parser.nodes.back();
                if (lastNode->type && lastNode->type->type == NodeType::Type::VARIABLE && !lastNode->children.back().get()) {
                    lastNode->children.back() = move(make_unique<Node>(Variant(std::string(str, len))));
                } else {
                    if (lastNode->type && !lastNode->children.back().get()) {
                        unique_ptr<Node> node = make_unique<Node>(context.getVariableNodeType());
                        node->children.push_back(make_unique<Node>(Variant(std::string(str, len))));
                        parser.nodes.push_back(move(node));
                    } else {
                        // Check for operators.
                        std::string opName = std::string(str, len);
                        if (opName == "|") {
                            if (parser.filterState != Parser::EFilterState::UNSET && parser.filterState != Parser::EFilterState::ARGUMENTS) {
                                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL, opName));
                                return false;
                            }
                            parser.filterState = Parser::EFilterState::NAME;
                            return true;
                        }
                        if (parser.filterState == Parser::EFilterState::NAME) {
                            parser.filterState = Parser::EFilterState::COLON;
                            const FilterNodeType* op = context.getFilterType(opName);
                            if (!op) {
                                parser.pushError(Error(*this, Error::Type::UNKNOWN_FILTER, opName));
                                return false;
                            }
                            auto operatorNode = make_unique<Node>(op);
                            auto& parentNode = parser.nodes[parser.nodes.size()-2];
                            assert(parentNode->type);
                            unique_ptr<Node> variableNode = move(parser.nodes.back());
                            operatorNode->children.push_back(move(variableNode));
                            operatorNode->children.push_back(nullptr);
                            parser.nodes.back() = move(operatorNode);
                            parser.nodes.push_back(std::make_unique<Node>(context.getArgumentsNodeType()));
                            parser.nodes.back()->children.push_back(nullptr);
                        } else {
                            const OperatorNodeType* op = context.getOperatorType(opName);
                            if (!op) {
                                parser.pushError(Error(*this, Error::Type::UNKNOWN_OPERATOR, opName));
                                return false;
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
        unique_ptr<Node> node = make_unique<Node>(context.getGroupNodeType());
        node->children.push_back(nullptr);
        parser.nodes.push_back(move(node));
        return true;
    }
    bool Parser::Lexer::closeParenthesis() {
        if (!parser.popNodeUntil(NodeType::Type::GROUP)) {
            parser.pushError(Error(*this, Error::Type::UNBALANCED_GROUP));
            return false;
        }
        parser.popNode();
        return true;
    }


    bool Parser::Lexer::startOutputBlock(bool suppress) {
        parser.state = Parser::State::ARGUMENT;
        parser.nodes.push_back(std::make_unique<Node>(context.getOutputNodeType()));
        parser.nodes.back()->children.push_back(nullptr);
        parser.nodes.push_back(std::make_unique<Node>(context.getArgumentsNodeType()));
        parser.nodes.back()->children.push_back(nullptr);
        return true;
    }

    bool Parser::popNode() {
        if (nodes.size() <= 1)
            return false;
        unique_ptr<Node> argumentNode = move(nodes.back());
        nodes.pop_back();
        argumentNode->validate(context);

        if (nodes.back()->children.size() == 0 || nodes.back()->children.back().get())
            return false;
        nodes.back()->children.back() = move(argumentNode);
        return true;
    }

    bool Parser::popNodeUntil(int type) {
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
            parser.pushError(Error(*this, Error::Type::UNEXPECTED_END));
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
            parser.pushError(Error(*this, Error::Type::UNEXPECTED_END));
            return false;
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
            parser.nodes.back()->children.push_back(nullptr);
            parser.nodes.push_back(make_unique<Node>(context.getConcatenationNodeType()));
        }
        parser.state = Parser::State::NODE;
        parser.blockType = Parser::EBlockType::NONE;
        return true;
    }

    Parser::Node Parser::parse(const char* buffer, size_t len) {
        nodes.push_back(std::make_unique<Node>(context.getConcatenationNodeType()));
        lexer.parse(buffer, len);
        if (errors.size() > 0)
            throw ParserException(errors[0]);
        assert(nodes.size() > 0);
        Node node = move(*nodes[nodes.size()-1]);
        nodes.clear();
        return node;
    }
}
