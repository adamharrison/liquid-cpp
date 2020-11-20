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
            QUALIFIER,
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
        virtual Node render(Renderer& renderer, const Node& node, Variable store) const = 0;
        virtual Parser::Error validate(const Context& context, const Node& node) const { return Parser::Error(); }
        virtual void optimize(const Context& context, Node& node, Variable store) const { }


        Node (*renderFunction)(Renderer& renderer, const Node& node, Variable store) =
            +[](Renderer& renderer, const Node& node, Variable store) {
                return node.type->render(renderer, node, store);
            };
        void (*optimizeFunction)(const Context& context, Node& node, Variable store) =
            +[](const Context& context, Node& node, Variable store) {
                node.type->optimize(context, node, store);
            };
    };

    struct TagNodeType : NodeType {
        enum class Composition {
            FREE,
            ENCLOSED
        };

        // This is insanely stupid.
        struct QualifierNodeType : NodeType {
            enum class Arity {
                NONARY,
                UNARY
            };
            Arity arity;

            QualifierNodeType(const string& symbol, Arity arity) : NodeType(NodeType::Type::QUALIFIER, symbol, 1), arity(arity) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        // For things like if/else, and whatnot. else is a free tag that sits inside the if statement.
        unordered_map<string, unique_ptr<NodeType>> intermediates;
        // For things for the forloop; like reversed, limit, etc... Super stupid, but Shopify threw them in, and there you are.
        unordered_map<string, unique_ptr<NodeType>> qualifiers;

        Composition composition;
        int minArguments;
        int maxArguments;

        TagNodeType(Composition composition, string symbol, int minArguments = -1, int maxArguments = -1) : NodeType(NodeType::Type::TAG, symbol, -1), composition(composition), minArguments(minArguments), maxArguments(maxArguments) { }

        // Used for registering intermedaites and qualiifers.
        template <class T>
        void registerType() {
            auto nodeType = make_unique<T>();
            switch (nodeType->type) {
                case NodeType::Type::TAG:
                    intermediates[nodeType->symbol] = std::move(nodeType);
                break;
                case NodeType::Type::QUALIFIER:
                    qualifiers[nodeType->symbol] = std::move(nodeType);
                break;
                default:
                    assert(false);
                break;
            }
        }
    };

    struct EnclosedNodeType : TagNodeType {
        EnclosedNodeType(string symbol, int minArguments = -1, int maxArguments = -1) : TagNodeType(Composition::ENCLOSED, symbol, minArguments, maxArguments) { }
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


        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
        Node getArgument(Renderer& renderer, const Node& node, Variable store, int idx) const;
    };

    struct Context {


        struct ConcatenationNode : NodeType {
            ConcatenationNode() : NodeType(Type::OPERATOR) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const;
        };

        struct OutputNode : NodeType {
            OutputNode() : NodeType(Type::OUTPUT) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const {
                assert(node.children.size() == 1);
                auto& argumentNode = node.children.front();
                assert(argumentNode->children.size() == 1);
                return renderer.retrieveRenderedNode(*argumentNode->children[0].get(), store);
            }
        };

        struct PassthruNode : NodeType {
            PassthruNode(NodeType::Type type) :NodeType(type) { }
            ~PassthruNode() { }

            Node render(Renderer& renderer, const Node& node, Variable store) const {
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
            Node render(Renderer& renderer, const Node& node, Variable store) const { assert(false); }
        };

        struct VariableNode : NodeType {
            VariableNode() : NodeType(Type::VARIABLE) { }

            LiquidVariableResolver resolver;

            Variable getVariable(Renderer& renderer, const Node& node, Variable store, bool createIfNotExists) const {
                Variable storePointer = store;
                for (auto& link : node.children) {
                    auto node = renderer.retrieveRenderedNode(*link.get(), store);
                    switch (node.variant.type) {
                        case Variant::Type::INT:
                            if (!resolver.getArrayVariable(storePointer, node.variant.i, createIfNotExists, (void**)&storePointer))
                                storePointer = Variable({ nullptr });
                        break;
                        case Variant::Type::STRING:
                            if (!resolver.getDictionaryVariable(storePointer, node.variant.s.data(), createIfNotExists, (void**)&storePointer))
                                storePointer = Variable({ nullptr });
                        break;
                        default:
                            storePointer = Variable({ nullptr });
                        break;
                    }
                    if (!storePointer.pointer)
                        return Variable({ nullptr });
                }
                return storePointer;
            }

            void setVariable(Variable variable, const Node& node) const {
                resolver.setString(variable, node.getString().data());
            }

            Node render(Renderer& renderer, const Node& node, Variable store) const {
                Variable storePointer = getVariable(renderer, node, store, false);
                return Node(renderer.context.parseVariant(storePointer));
            }
        };

        unordered_map<string, unique_ptr<NodeType>> tagTypes;
        unordered_map<string, unique_ptr<NodeType>> operatorTypes;
        unordered_map<string, unique_ptr<NodeType>> filterTypes;
        unique_ptr<NodeType> variableNodeType;

        const NodeType* getConcatenationNodeType() const { static ConcatenationNode concatenationNodeType; return &concatenationNodeType; }
        const NodeType* getOutputNodeType() const { static OutputNode outputNodeType; return &outputNodeType; }
        const VariableNode* getVariableNodeType() const { return static_cast<VariableNode*>(variableNodeType.get()); }
        const NodeType* getGroupNodeType() const { static GroupNode groupNodeType; return &groupNodeType; }
        const NodeType* getGroupDereferenceNodeType() const { static GroupDereferenceNode groupDereferenceNodeType; return &groupDereferenceNodeType; }
        const NodeType* getArgumentsNodeType() const { static ArgumentNode argumentNodeType; return &argumentNodeType; }


        void registerType(unique_ptr<NodeType> type) {
            switch (type->type) {
                case NodeType::Type::TAG:
                    tagTypes[type->symbol] = move(type);
                break;
                case NodeType::Type::VARIABLE:
                    variableNodeType = move(type);
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

        void optimize(Node& ast, Variable store);

        void inject(Variable& variable, const Variant& variant) const;
        Variant parseVariant(Variable variable) const;

        const LiquidVariableResolver& getVariableResolver() const { return getVariableNodeType()->resolver; }
        bool resolveVariableString(string& target, void* variable) const {
            const LiquidVariableResolver& resolver = getVariableResolver();
            long long length = resolver.getStringLength(variable);
            target.resize(length);
            if (!resolver.getString(variable, const_cast<char*>(target.data())))
                return false;
            return true;
        }

    };
}

#endif
