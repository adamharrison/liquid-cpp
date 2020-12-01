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
            FILTER,
            DOT_FILTER
        };
        Type type;
        string symbol;
        int maxChildren;
        void* userData = nullptr;
        LiquidRenderFunction userRenderFunction = nullptr;

        NodeType(Type type, string symbol = "", int maxChildren = -1) : type(type), symbol(symbol), maxChildren(maxChildren) { }
        NodeType(const NodeType&) = default;
        NodeType(NodeType&&) = default;
        ~NodeType() { }

        virtual Node render(Renderer& renderer, const Node& node, Variable store) const {
            assert(userRenderFunction);
            userRenderFunction(LiquidRenderer{&renderer}, LiquidNode{const_cast<Node*>(&node)}, store, userData);
            return renderer.returnValue;
        }
        virtual Parser::Error validate(const Context& context, const Node& node) const { return Parser::Error(); }
        virtual void optimize(const Context& context, Node& node, Variable store) const { }
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


    struct DotFilterNodeType : NodeType {
        DotFilterNodeType(string symbol) : NodeType(NodeType::Type::DOT_FILTER, symbol, -1) { }

        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
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
                return Variant(renderer.getString(renderer.retrieveRenderedNode(*argumentNode->children[0].get(), store)));
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

        struct UnknownFilterNode : FilterNodeType {
            UnknownFilterNode() : FilterNodeType("", -1, -1) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        struct VariableNode : NodeType {
            /* Things like .size, etc.. */
            struct UnaryVariableFilterNode : FilterNodeType {
                UnaryVariableFilterNode(const std::string& symbol) : FilterNodeType(symbol, 0, 0) { }
            };

            unordered_map<std::string, unique_ptr<NodeType>> filters;

            LiquidVariableResolver resolver;

            VariableNode() : NodeType(Type::VARIABLE) { }

            Variable getVariable(Renderer& renderer, const Node& node, Variable store) const {
                Variable storePointer = store;
                for (auto& link : node.children) {
                    auto node = renderer.retrieveRenderedNode(*link.get(), store);
                    switch (node.variant.type) {
                        case Variant::Type::INT:
                            if (!resolver.getArrayVariable(storePointer, node.variant.i, storePointer)) {
                                storePointer = Variable({ nullptr });
                            }
                        break;
                        case Variant::Type::STRING:
                            if (!resolver.getDictionaryVariable(storePointer, node.variant.s.data(), storePointer))
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

            bool setVariable(Renderer& renderer, const Node& node, Variable store, Variable value) const {
                Variable storePointer = store;
                for (size_t i = 0; i < node.children.size(); ++i) {
                    auto& link = node.children[i];
                    auto part = renderer.retrieveRenderedNode(*link.get(), store);
                    if (i == node.children.size() - 1) {
                        switch (part.variant.type) {
                            case Variant::Type::INT:
                                return resolver.setArrayVariable(renderer, storePointer, part.variant.i, value);
                            case Variant::Type::STRING:
                                return resolver.setDictionaryVariable(renderer, storePointer, part.variant.s.data(), value);
                            default:
                                return false;
                        }
                    } else {
                        switch (part.variant.type) {
                            case Variant::Type::INT:
                                if (!resolver.getArrayVariable(storePointer, part.variant.i, storePointer))
                                    return false;
                            break;
                            case Variant::Type::STRING:
                                if (!resolver.getDictionaryVariable(storePointer, part.variant.s.data(), storePointer))
                                    return false;
                            break;
                            default:
                                return false;
                        }
                    }
                    if (!storePointer.pointer)
                        return false;
                }
                return false;
            }

            Node render(Renderer& renderer, const Node& node, Variable store) const {
                Variable storePointer = getVariable(renderer, node, store);
                return Node(renderer.parseVariant(storePointer));
            }
        };

        unordered_map<string, unique_ptr<NodeType>> tagTypes;
        unordered_map<string, unique_ptr<NodeType>> unaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> binaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> filterTypes;
        unordered_map<string, unique_ptr<NodeType>> dotFilterTypes;
        unique_ptr<NodeType> variableNodeType;

        const NodeType* getConcatenationNodeType() const { static ConcatenationNode concatenationNodeType; return &concatenationNodeType; }
        const NodeType* getOutputNodeType() const { static OutputNode outputNodeType; return &outputNodeType; }
        const VariableNode* getVariableNodeType() const { return static_cast<VariableNode*>(variableNodeType.get()); }
        const NodeType* getGroupNodeType() const { static GroupNode groupNodeType; return &groupNodeType; }
        const NodeType* getGroupDereferenceNodeType() const { static GroupDereferenceNode groupDereferenceNodeType; return &groupDereferenceNodeType; }
        const NodeType* getArgumentsNodeType() const { static ArgumentNode argumentNodeType; return &argumentNodeType; }
        const NodeType* getUnknownFilterNodeType() const { static UnknownFilterNode filterNodeType; return &filterNodeType; }


        void registerType(unique_ptr<NodeType> type) {
            switch (type->type) {
                case NodeType::Type::TAG:
                    tagTypes[type->symbol] = move(type);
                break;
                case NodeType::Type::VARIABLE:
                    variableNodeType = move(type);
                break;
                case NodeType::Type::OPERATOR:
                    switch (static_cast<OperatorNodeType*>(type.get())->arity) {
                        case OperatorNodeType::Arity::BINARY:
                            assert(static_cast<OperatorNodeType*>(type.get())->fixness == OperatorNodeType::Fixness::INFIX);
                            binaryOperatorTypes[type->symbol] = move(type);
                        break;
                        case OperatorNodeType::Arity::UNARY:
                            assert(static_cast<OperatorNodeType*>(type.get())->fixness == OperatorNodeType::Fixness::PREFIX);
                            unaryOperatorTypes[type->symbol] = move(type);
                        break;
                        default:
                            assert(false);
                        break;
                    }
                break;
                case NodeType::Type::FILTER:
                    filterTypes[type->symbol] = move(type);
                break;
                case NodeType::Type::DOT_FILTER:
                    dotFilterTypes[type->symbol] = move(type);
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
        const OperatorNodeType* getBinaryOperatorType(string symbol) const {
            auto it = binaryOperatorTypes.find(symbol);
            if (it == binaryOperatorTypes.end())
                return nullptr;
            return static_cast<OperatorNodeType*>(it->second.get());
        }

        const OperatorNodeType* getUnaryOperatorType(string symbol) const {
            auto it = unaryOperatorTypes.find(symbol);
            if (it == unaryOperatorTypes.end())
                return nullptr;
            return static_cast<OperatorNodeType*>(it->second.get());
        }

        const FilterNodeType* getFilterType(string symbol) const {
            auto it = filterTypes.find(symbol);
            if (it == filterTypes.end())
                return nullptr;
            return static_cast<FilterNodeType*>(it->second.get());
        }
        const DotFilterNodeType* getDotFilterType(string symbol) const {
            auto it = dotFilterTypes.find(symbol);
            if (it == dotFilterTypes.end())
                return nullptr;
            return static_cast<DotFilterNodeType*>(it->second.get());
        }

        void optimize(Node& ast, Variable store);

        const LiquidVariableResolver& getVariableResolver() const { return getVariableNodeType()->resolver; }
        bool resolveVariableString(string& target, void* variable) const {
            const LiquidVariableResolver& resolver = getVariableResolver();
            long long length = resolver.getStringLength(variable);
            if (length < 0)
                return false;
            target.resize(length);
            if (!resolver.getString(variable, const_cast<char*>(target.data())))
                return false;
            return true;
        }

    };
}

#endif
