#include "context.h"
#include "parser.h"

namespace Liquid {

    // Performs implicit conversion.
    string Parser::Node::getString() {
        assert(type == nullptr);
        assert(arguments.size() == 1);
        assert(arguments[0].type == Variable::Type::STATIC);
        if (arguments[0].variant.type == Variant::Type::STRING)
            return arguments[0].variant.s;
        else if (arguments[0].variant.type == Variant::Type::FLOAT)
            return std::to_string(arguments[0].variant.f);
        else if (arguments[0].variant.type == Variant::Type::INT)
            return std::to_string(arguments[0].variant.i);
        return std::string();
    }

    bool Parser::Lexer::colon() {
        if (parser.nodes.back()->type != context.getVariableNodeType() || parser.activeVariable().chain.size() != 1 || parser.activeVariable().type != Variable::Type::REFERENCE) {
            parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
            return false;
        }
        parser.activeVariable().type = Variable::Type::NAMED;
        return true;
    }

    void Parser::closeVariable() {
        unique_ptr<Node> node = move(nodes.back());
        nodes.pop_back();
        nodes.back()->children.push_back(move(node));
    }

    bool Parser::Lexer::comma() {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::ARGUMENT:
            case Parser::State::VARIABLE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::VARIABLE_FINISHED:
                parser.closeVariable();
                parser.state = Parser::State::ARGUMENT;
            break;
        }
        return true;
    }

    bool Parser::Lexer::endVariableDereference() {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::ARGUMENT:
            case Parser::State::VARIABLE:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::VARIABLE_FINISHED:
                assert(false);
            break;
        }
        return true;
    }

    bool Parser::Lexer::string(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::VARIABLE_FINISHED:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT:
                parser.state = Parser::State::VARIABLE_FINISHED;
                assert(false);
                //parser.variables.push_back(Variable());
                //parser.variables.back().chain.push_back(Variant(std::string(str, len)));
            break;
            case Parser::State::VARIABLE:
                assert(false);
                // parser.variables.back().chain.push_back(Variant(std::string(str, len)));
                // parser.state = Parser::State::VARIABLE_FINISHED;
            break;
        }
        return true;
    }

    bool Parser::Lexer::integer(long long i) {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::VARIABLE_FINISHED:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                parser.state = Parser::State::VARIABLE_FINISHED;
                unique_ptr<Node> node = make_unique<Node>(Variable(Variant(i)));
                if (parser.argumentNodeIdx++ == 0) {
                    parser.nodes.push_back(move(node));
                } else {
                    parser.nodes.back()->children.push_back(move(node));
                }
            } break;
            case Parser::State::VARIABLE:
                parser.activeVariable().chain.push_back(Variant(i));
                parser.state = Parser::State::VARIABLE_FINISHED;
            break;
        }
        return true;
    }

    bool Parser::Lexer::floating(double f) {
        switch (parser.state) {
            case Parser::State::NODE:
            case Parser::State::VARIABLE_FINISHED:
                parser.pushError(Error(*this, Error::Type::INVALID_SYMBOL));
                return false;
            break;
            case Parser::State::ARGUMENT: {
                parser.state = Parser::State::VARIABLE_FINISHED;
                unique_ptr<Node> node = make_unique<Node>(Variable(Variant(f)));
                if (parser.argumentNodeIdx++ == 0) {
                    parser.nodes.push_back(move(node));
                } else {
                    parser.nodes.back()->children.push_back(move(node));
                }
            } break;
            case Parser::State::VARIABLE:
                parser.activeVariable().chain.push_back(Variant(f));
                parser.state = Parser::State::VARIABLE_FINISHED;
            break;
        }
        return true;
    }

    bool Parser::Lexer::literal(const char* str, size_t len) {
        switch (parser.state) {
            case Parser::State::NODE: {
                switch (this->state) {
                    case SUPER::State::CONTROL: {
                        if (parser.argumentIdx == 0) {
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
                        } else {
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
                parser.state = Parser::State::VARIABLE_FINISHED;
                unique_ptr<Node> node = make_unique<Node>(context.getVariableNodeType());
                node->arguments.push_back(Variable(Variable::Type::REFERENCE));
                node->arguments.back().chain.push_back(Variant(std::string(str, len)));
                if (parser.argumentNodeIdx++ == 0) {
                    parser.nodes.push_back(move(node));
                } else {
                    parser.nodes.back()->children.push_back(move(node));
                }
            } break;
            case Parser::State::VARIABLE:
                parser.nodes.back()->arguments.back().chain.push_back(Variant(std::string(str, len)));
                parser.state = Parser::State::VARIABLE_FINISHED;
                ++parser.argumentNodeIdx;
            break;
            case Parser::State::VARIABLE_FINISHED: {
                // Check for operators. Once we check for an operator.
                std::string opName = std::string(str, len);
                const OperatorNodeType* op = context.getOperatorType(opName);
                if (!op) {
                    parser.pushError(Error(*this, Error::Type::UNKNOWN_OPERATOR, opName));
                    return false;
                }
                parser.state = Parser::State::ARGUMENT;
                assert(op->fixness == OperatorNodeType::Fixness::INFIX);
                auto operatorNode = make_unique<Node>(op);
                auto variableNode = move(parser.nodes.back());
                parser.nodes.pop_back();
                operatorNode->children.push_back(move(variableNode));
                parser.nodes.push_back(move(operatorNode));
                ++parser.argumentNodeIdx;
            } break;
        }
        return true;
    }

    bool Parser::Lexer::startOutputBlock(bool suppress) {
        parser.state = Parser::State::ARGUMENT;
        parser.argumentNodeIdx = 0;
        parser.nodes.push_back(std::make_unique<Node>(context.getOutputNodeType()));
        return true;
    }
    bool Parser::Lexer::endOutputBlock(bool suppress) {
        if (parser.state == Parser::State::ARGUMENT) {
            parser.pushError(Error(*this, Error::Type::UNEXPECTED_END));
            return false;
        }

        parser.state = Parser::State::NODE;
        parser.closeVariable();

        unique_ptr<Node> node = move(parser.nodes.back());
        parser.nodes.pop_back();
        parser.nodes.back()->children.push_back(move(node));
        return true;
    }

    Parser::Node Parser::parse(const char* buffer, size_t len) {
        nodes.push_back(std::make_unique<Node>(context.getConcatenationNodeType()));
        lexer.parse(buffer, len);
        assert(nodes.size() > 0);
        Node node = move(*nodes[nodes.size()-1]);
        nodes.clear();
        return node;
    }
}
