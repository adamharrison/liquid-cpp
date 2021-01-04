#ifndef LIQUIDCONTEXT_H
#define LIQUIDCONTEXT_H

#include "common.h"
#include "parser.h"
#include "renderer.h"

namespace Liquid {

    struct Renderer;


    struct TagNodeType : NodeType {
        enum class Composition {
            ENCLOSED,
            FREE
        };

        // This is insanely stupid.
        struct QualifierNodeType : NodeType {
            enum class Arity {
                NONARY,
                UNARY
            };
            Arity arity;

            QualifierNodeType(const string& symbol, Arity arity) : NodeType(NodeType::Type::QUALIFIER, symbol, 1, LIQUID_OPTIMIZATION_SCHEME_NONE), arity(arity) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const { return Node(); }
        };

        // For things like if/else, and whatnot. else is a free tag that sits inside the if statement.
        unordered_map<string, unique_ptr<NodeType>> intermediates;
        // For things for the forloop; like reversed, limit, etc... Super stupid, but Shopify threw them in, and there you are.
        unordered_map<string, unique_ptr<NodeType>> qualifiers;

        Composition composition;
        int minArguments;
        int maxArguments;

        TagNodeType(Composition composition, string symbol, int minArguments = -1, int maxArguments = -1, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(NodeType::Type::TAG, symbol, -1, optimization), composition(composition), minArguments(minArguments), maxArguments(maxArguments) { }

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
        EnclosedNodeType(string symbol, int minArguments = -1, int maxArguments = -1, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : TagNodeType(Composition::ENCLOSED, symbol, minArguments, maxArguments, optimization) { }
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

        OperatorNodeType(string symbol, Arity arity, int priority = 0, Fixness fixness = Fixness::INFIX, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(Type::OPERATOR, symbol, -1, optimization), arity(arity), priority(priority), fixness(fixness) {
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

        FilterNodeType(string symbol, int minArguments = -1, int maxArguments = -1, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(NodeType::Type::FILTER, symbol, -1, optimization), minArguments(minArguments), maxArguments(maxArguments) { }

        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
    };


    struct DotFilterNodeType : NodeType {
        DotFilterNodeType(string symbol, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(NodeType::Type::DOT_FILTER, symbol, -1, optimization) { }

        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
    };

    struct Context {
        struct ConcatenationNode : NodeType {
            ConcatenationNode() : NodeType(Type::OPERATOR, "", -1, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const;
            bool optimize(Optimizer& optimizer, Node& node, Variable store) const;
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
        struct GroupNode : PassthruNode { GroupNode() : PassthruNode(Type::GROUP) { } };
        // These are purely for parsing purpose, and should not make their way to the rednerer.
        struct GroupDereferenceNode : PassthruNode { GroupDereferenceNode() : PassthruNode(Type::GROUP_DEREFERENCE) { } };
        // Used exclusively for tags. Should be never be rendered by itself.
        struct ArgumentNode : NodeType { ArgumentNode() : NodeType(Type::ARGUMENTS, "", -1, LIQUID_OPTIMIZATION_SCHEME_NONE) { } };
        struct ArrayLiteralNode : NodeType {
            ArrayLiteralNode() : NodeType(Type::ARRAY_LITERAL) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const {
                Variant var({ });
                var.a.reserve(node.children.size());
                for (size_t i = 0; i < node.children.size(); ++i)
                    var.a.push_back(move(renderer.retrieveRenderedNode(*node.children[i].get(), store).variant));
                return Node(move(var));
            }
        };

        struct UnknownFilterNode : FilterNodeType { UnknownFilterNode() : FilterNodeType("", -1, -1) { } };

        EFalsiness falsiness = FALSY_FALSE;
        bool allowArrayLiterals = true;

        struct VariableNode : NodeType {
            VariableNode() : NodeType(Type::VARIABLE) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const {
                pair<void*, Renderer::DropFunction> drop = renderer.getInternalDrop(node, store);
                if (drop.second) {
                    Node result = drop.second(renderer, node, store, drop.first);
                    if (result.type || result.variant.type != Variant::Type::VARIABLE)
                        return result;
                    return Node(renderer.parseVariant(result.variant.v));
                } else {
                    Variable storePointer = renderer.getVariable(node, store);
                    if (!storePointer.exists())
                        return Node();
                    return Node(renderer.parseVariant(storePointer));
                }
            }

            bool optimize(Optimizer& optimizer, Node& node, Variable store) const;
        };

        unordered_map<string, unique_ptr<NodeType>> tagTypes;
        unordered_map<string, unique_ptr<NodeType>> unaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> binaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> filterTypes;
        unordered_map<string, unique_ptr<NodeType>> dotFilterTypes;

        const NodeType* getConcatenationNodeType() const { static ConcatenationNode concatenationNodeType; return &concatenationNodeType; }
        const NodeType* getOutputNodeType() const { static OutputNode outputNodeType; return &outputNodeType; }
        const VariableNode* getVariableNodeType() const { static VariableNode variableNodeType; return &variableNodeType; }
        const NodeType* getGroupNodeType() const { static GroupNode groupNodeType; return &groupNodeType; }
        const NodeType* getGroupDereferenceNodeType() const { static GroupDereferenceNode groupDereferenceNodeType; return &groupDereferenceNodeType; }
        const NodeType* getArgumentsNodeType() const { static ArgumentNode argumentNodeType; return &argumentNodeType; }
        const NodeType* getUnknownFilterNodeType() const { static UnknownFilterNode filterNodeType; return &filterNodeType; }
        const NodeType* getArrayLiteralNodeType() const { static ArrayLiteralNode arrayLiteralNode; return &arrayLiteralNode; }

        NodeType* registerType(unique_ptr<NodeType> type) {
            switch (type->type) {
                case NodeType::Type::TAG:
                    tagTypes[type->symbol] = move(type);
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
            return type.get();
        }
        template <class T> NodeType* registerType() { return registerType(make_unique<T>()); }

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

    };
}

#endif
