#include "context.h"
#include "parser.h"

namespace Liquid {

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

    void Parser::appendOperandNode(unique_ptr<Node> node) {
        auto& operatorArgumentNode = nodes.back();
        assert(!operatorArgumentNode->children.back().get());
        if (operatorArgumentNode->type && operatorArgumentNode->type->type == NodeType::Type::ARGUMENTS && operatorArgumentNode->children.size() == 0)
            operatorArgumentNode->children.push_back(move(node));
        else
            operatorArgumentNode->children.back() = move(node);
    }

    bool Parser::Lexer::colon() {
        if (parser.nodes.back()->type != context.getVariableNodeType() || parser.nodes.back()->children.size() != 1) {
            parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
            return false;
        }
        parser.nodes.back()->type = context.getNamedVariableNodeType();
        return true;
    }

    bool Parser::Lexer::comma() {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::ARGUMENT:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
        }
        parser.popNode();
        auto& argumentNode = parser.getArgumentNode();
        argumentNode.children.push_back(nullptr);
        return true;
    }

    bool Parser::Lexer::startVariableDereference() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:

            break;
        }
        return true;
    }

    bool Parser::Lexer::endVariableDereference() {
        if (--parser.dereferenceDepth < 0) {
            parser.pushError(Error(*this, Error::Type::UNBALANCED_GROUP, "]"));
            return false;
        }
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::ARGUMENT:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
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
                parser.appendOperandNode(move(make_unique<Node>(Variant(std::string(str, len)))));
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
                parser.appendOperandNode(move(make_unique<Node>(Variant(i))));
            } break;
        }
        return true;
    }

    bool Parser::Lexer::dot() {
        switch (parser.state) {
            case Parser::State::NODE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                if (parser.nodes.back()->type != context.getVariableNodeType()) {
                    parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                    return false;
                }
                return true;
            break;
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
                parser.appendOperandNode(move(make_unique<Node>(Variant(f))));
            } break;
        }
        return true;
    }

    Parser::Node& Parser::getArgumentNode() {
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            if ((*it)->type && (*it)->type->type == NodeType::Type::ARGUMENTS)
                return **it;
        }
        assert(false);
    }

    bool Parser::Lexer::literal(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE: {
                switch (this->state) {
                    case SUPER::State::CONTROL: {
                        //if (parser.argumentIdx == 0) {
                            if (len > 3 && strncmp(str, "end", 3) == 0) {
                                std::string typeName = std::string(&str[3], len - 3);
                                const NodeType* type = SUPER::context.getTagType(typeName);
                                if (!type) {
                                    parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, std::string(str, len)));
                                    return false;
                                }
                                parser.nodes.pop_back();
                            } else {
                                std::string typeName = std::string(str, len);
                                const NodeType* type = SUPER::context.getTagType(typeName);
                                if (!type) {
                                    parser.pushError(Error(*this, Error::Type::UNKNOWN_TAG, typeName));
                                    return false;
                                }
                                parser.nodes.push_back(std::make_unique<Node>(type));
                            }
                        //} else {
                        //}
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
                auto& argumentNode = parser.getArgumentNode();
                auto& lastNode = parser.nodes.back();
                if ((lastNode.get() == &argumentNode && (argumentNode.children.size() == 0 || !argumentNode.children.back().get()))) {
                    unique_ptr<Node> node = make_unique<Node>(context.getVariableNodeType());
                    // The nonary dot operator pops the last argument varaible back on to the stack.
                    node->children.push_back(make_unique<Node>(Variant(std::string(str, len))));
                    parser.nodes.push_back(move(node));
                } else {
                    // Check for operators.
                    std::string opName = std::string(str, len);
                    const OperatorNodeType* op = context.getOperatorType(opName);
                    if (!op) {
                        parser.pushError(Error(*this, Error::Type::UNKNOWN_OPERATOR, opName));
                        return false;
                    }
                    assert(op->fixness == OperatorNodeType::Fixness::INFIX);
                    auto operatorNode = make_unique<Node>(op);
                    auto variableNode = move(parser.nodes.back());
                    parser.nodes.pop_back();

                    if (variableNode->type->type == NodeType::Type::OPERATOR &&
                        static_cast<const OperatorNodeType*>(variableNode->type)->priority < op->priority) {
                        // If we're of a lower priority, push operator and
                        operatorNode->children.push_back(move(variableNode->children.back()));
                        operatorNode->children.push_back(nullptr);
                        variableNode->children.pop_back();
                        parser.nodes.push_back(move(variableNode));
                        parser.nodes.push_back(move(operatorNode));

                    } else {
                        operatorNode->children.push_back(move(variableNode));
                        operatorNode->children.push_back(nullptr);
                        parser.nodes.push_back(move(operatorNode));
                    }
                }
            } break;
        }
        return true;
    }


    bool Parser::Lexer::openParenthesis() {
        return true;
    }
    bool Parser::Lexer::closeParenthesis() {
        return true;
    }


    bool Parser::Lexer::startOutputBlock(bool suppress) {
        parser.state = Parser::State::ARGUMENT;
        parser.nodes.push_back(std::make_unique<Node>(context.getOutputNodeType()));
        parser.nodes.push_back(std::make_unique<Node>(context.getArgumentsNodeType()));
        return true;
    }

    void Parser::popNode() {
        unique_ptr<Node> argumentNode = move(nodes.back());
        nodes.pop_back();
        nodes.back()->children.push_back(move(argumentNode));
    }

    bool Parser::Lexer::endOutputBlock(bool suppress) {
        if (parser.state != Parser::State::ARGUMENT) {
            parser.pushError(Error(*this, Error::Type::UNEXPECTED_END));
            return false;
        }
        while (true) {
            auto& node = parser.nodes.back();
            if (!node->type || node->type->type == NodeType::Type::OUTPUT) {
                parser.popNode();
                break;
            }
            parser.popNode();
        }
        parser.state = Parser::State::NODE;
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
