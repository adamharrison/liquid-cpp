#ifndef CONTEXT_H
#define CONTEXT_H

#include "common.h"
#include "parser.h"
#include "renderer.h"

namespace Liquid {

    struct Renderer;

    struct NodeType {
        enum Type {
            VARIABLE,
            TAG,
            GROUP,
            GROUP_DEREFERENCE,
            OUTPUT,
            ARGUMENTS,
            OPERATOR,
            FILTER
        };
        Type type;
        string symbol;
        int maxChildren;

        NodeType(Type type, string symbol = "", int maxChildren = -1) : type(type), symbol(symbol), maxChildren(maxChildren) { }
        NodeType(const NodeType&) = default;
        NodeType(NodeType&&) = default;
        ~NodeType() { }

        // When a node is rendered, depending on its mode, it'll return a node.
        virtual Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const = 0;
        virtual Parser::Error validate(const Context& context, const Parser::Node& node) const { return Parser::Error(); }
        virtual void optimize(const Context& context, Parser::Node& node, Variable& store) const { }


        Parser::Node (*renderFunction)(Renderer& renderer, const Parser::Node& node, Variable& store) =
            +[](Renderer& renderer, const Parser::Node& node, Variable& store) {
                return node.type->render(renderer, node, store);
            };
        void (*optimizeFunction)(const Context& context, Parser::Node& node, Variable& store) =
            +[](const Context& context, Parser::Node& node, Variable& store) {
                node.type->optimize(context, node, store);
            };
    };

    struct TagNodeType : NodeType {
        enum class Composition {
            FREE,
            ENCLOSED
        };

        // For things like if/else, and whatnot. else is a free tag that sits inside the if statement.
        unordered_map<string, unique_ptr<NodeType>> intermediates;

        Composition composition;
        int minArguments;
        int maxArguments;

        TagNodeType(Composition composition, string symbol, int minArguments = -1, int maxArguments = -1) : NodeType(NodeType::Type::TAG, symbol, -1), composition(composition), minArguments(minArguments), maxArguments(maxArguments) { }
    };

    struct EnclosedNodeType : TagNodeType {
        EnclosedNodeType(string symbol, int minArguments = -1, int maxArguments = -1) : TagNodeType(Composition::ENCLOSED, symbol, minArguments, maxArguments) { }

        // I don't like doing this, but
        #define assignEnclosedRender(result, context, node, store) result = context.retrieveRenderedNode(node, store); if (!result.type && result.varaint.type == Parser::Variant::Type::CONTROL) return result;
    };

    struct OperatorNodeType : NodeType {
        enum class Arity {
            NONARY,
            UNARY,
            BINARY,
            NARY
        };
        Arity arity;
        int priority;

        enum class Fixness {
            PREFIX,
            INFIX,
            AFFIX
        };
        Fixness fixness;

        OperatorNodeType(string symbol, Arity arity, int priority = 0, Fixness fixness = Fixness::INFIX) : NodeType(Type::OPERATOR, symbol), arity(arity), priority(priority), fixness(fixness) {
            switch (arity) {
                case Arity::NONARY:
                    maxChildren = 0;
                break;
                case Arity::UNARY:
                    maxChildren = 1;
                break;
                case Arity::BINARY:
                    maxChildren = 2;
                break;
                case Arity::NARY:
                    maxChildren = -1;
                break;
            }
        }
    };

    struct FilterNodeType : NodeType {
        int minArguments;
        int maxArguments;

        FilterNodeType(string symbol, int minArguments = -1, int maxArguments = -1) : NodeType(NodeType::Type::FILTER, symbol, -1), minArguments(minArguments), maxArguments(maxArguments) { }


        Parser::Node getOperand(Renderer& renderer, const Parser::Node& node, Variable& store) const;
        Parser::Node getArgument(Renderer& renderer, const Parser::Node& node, Variable& store, int idx) const;
    };

    struct Context {


        struct ConcatenationNode : NodeType {
            ConcatenationNode() : NodeType(Type::OPERATOR) { }

            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const;
        };

        struct OutputNode : NodeType {
            OutputNode() : NodeType(Type::OUTPUT) { }

            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                assert(node.children.size() == 1);

                auto& argumentNode = node.children.front();
                assert(argumentNode->children.size() == 1);
                auto it = argumentNode->children.begin();
                if ((*it)->type)
                    return (*it)->type->render(renderer, *it->get(), store);
                return *it->get();
            }
        };

        struct PassthruNode : NodeType {
            PassthruNode(NodeType::Type type) :NodeType(type) { }
            ~PassthruNode() { }

            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                assert(node.children.size() == 1);
                auto it = node.children.begin();
                if ((*it)->type)
                    return (*it)->type->render(renderer, *it->get(), store);
                return *it->get();
            }
        };

        // These are purely for parsing purpose, and should not make their way to the rednerer.
        struct GroupNode : PassthruNode {
            GroupNode() : PassthruNode(Type::GROUP) { }
        };
        // These are purely for parsing purpose, and should not make their way to the rednerer.
        struct GroupDereferenceNode : PassthruNode {
            GroupDereferenceNode() : PassthruNode(Type::GROUP_DEREFERENCE) { }
        };
        // Used exclusively for tags. Should be never be rendered by itself.
        struct ArgumentNode : NodeType {
            ArgumentNode() : NodeType(Type::ARGUMENTS) { }
            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const { assert(false); }
        };

        struct VariableNode : NodeType {
            VariableNode() : NodeType(Type::VARIABLE) { }


            Variable* getVariable(Renderer& renderer, const Parser::Node& node, Variable& store, bool createIfNotExists) const {
                Variable* storePointer = &store;
                for (auto& link : node.children) {
                    auto node = renderer.retrieveRenderedNode(*link.get(), store);
                    switch (node.variant.type) {
                        case Parser::Variant::Type::INT:
                            if (!storePointer->getArrayVariable(storePointer, node.variant.i, createIfNotExists))
                                storePointer = nullptr;
                        break;
                        case Parser::Variant::Type::STRING:
                            if (!storePointer->getDictionaryVariable(storePointer, node.variant.s, createIfNotExists))
                                storePointer = nullptr;
                        break;
                        default:
                            storePointer = nullptr;
                        break;
                    }
                    if (!storePointer)
                        return nullptr;
                }
                return storePointer;
            }

            Parser::Node render(Renderer& renderer, const Parser::Node& node, Variable& store) const {
                Variable* storePointer = getVariable(renderer, node, store, false);
                if (!storePointer)
                    return Parser::Node(Parser::Variant());
                switch (storePointer->getType()) {
                    case Variable::Type::NIL: {
                        return Parser::Node(Parser::Variant());
                    } break;
                    case Variable::Type::FLOAT: {
                        double f;
                        storePointer->getFloat(f);
                        return Parser::Node(Parser::Variant(f));
                    } break;
                    case Variable::Type::STRING: {
                        std::string s;
                        storePointer->getString(s);
                        return Parser::Node(Parser::Variant(s));
                    } break;
                    case Variable::Type::INT: {
                        long long i;
                        storePointer->getInteger(i);
                        return Parser::Node(Parser::Variant(i));
                    } break;
                    case Variable::Type::BOOL: {
                        bool b;
                        storePointer->getBool(b);
                        return Parser::Node(Parser::Variant(b));
                    } break;
                    default:
                        return Parser::Node(Parser::Variant(storePointer));
                    break;
                }

            }
        };

        struct NamedVariableNode : VariableNode {

        };

        unordered_map<string, unique_ptr<NodeType>> tagTypes;
        unordered_map<string, unique_ptr<NodeType>> operatorTypes;
        unordered_map<string, unique_ptr<NodeType>> filterTypes;

        const NodeType* getConcatenationNodeType() const { static ConcatenationNode concatenationNodeType; return &concatenationNodeType; }
        const NodeType* getOutputNodeType() const { static OutputNode outputNodeType; return &outputNodeType; }
        const NodeType* getVariableNodeType() const { static VariableNode variableNodeType; return &variableNodeType; }
        const NodeType* getNamedVariableNodeType() const { static NamedVariableNode namedVariableNodeType; return &namedVariableNodeType; }
        const NodeType* getGroupNodeType() const { static GroupNode groupNodeType; return &groupNodeType; }
        const NodeType* getGroupDereferenceNodeType() const { static GroupDereferenceNode groupDereferenceNodeType; return &groupDereferenceNodeType; }
        const NodeType* getArgumentsNodeType() const { static ArgumentNode argumentNodeType; return &argumentNodeType; }


        void registerType(unique_ptr<NodeType> type) {
            switch (type->type) {
                case NodeType::Type::TAG:
                    tagTypes[type->symbol] = move(type);
                break;
                case NodeType::Type::OPERATOR:
                    operatorTypes[type->symbol] = move(type);
                break;
                case NodeType::Type::FILTER:
                    filterTypes[type->symbol] = move(type);
                break;
                default:
                    assert(false);
                break;
            }
        }
        template <class T> void registerType() { registerType(make_unique<T>()); }

        const TagNodeType* getTagType(string symbol) const {
            auto it = tagTypes.find(symbol);
            if (it == tagTypes.end())
                return nullptr;
            return static_cast<TagNodeType*>(it->second.get());
        }
        const OperatorNodeType* getOperatorType(string symbol) const {
            auto it = operatorTypes.find(symbol);
            if (it == operatorTypes.end())
                return nullptr;
            return static_cast<OperatorNodeType*>(it->second.get());
        }

        const FilterNodeType* getFilterType(string symbol) const {
            auto it = filterTypes.find(symbol);
            if (it == filterTypes.end())
                return nullptr;
            return static_cast<FilterNodeType*>(it->second.get());
        }

        void optimize(Parser::Node& ast, Variable& store);


    };
}

#endif
