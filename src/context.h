#ifndef LIQUIDCONTEXT_H
#define LIQUIDCONTEXT_H

#include "common.h"
#include "parser.h"
#include "renderer.h"

namespace Liquid {

    struct Renderer;

    struct LiteralType {
        string symbol;
        Variant value;

        LiteralType(string symbol, Variant value) : symbol(symbol), value(value) { }
    };

    struct ContextualNodeType : NodeType {
        ContextualNodeType(NodeType::Type type, const std::string& symbol = "", int maxChildren = -1, LiquidOptimizationScheme scheme = LIQUID_OPTIMIZATION_SCHEME_NONE) : NodeType(type, symbol, maxChildren, scheme) { }

        // For operators that are internal to this tag.
        unordered_map<string, unique_ptr<NodeType>> operators;
        // For filters specific to this tag.
        unordered_map<string, unique_ptr<NodeType>> filters;

        // Used for registering intermedaites and qualiifers.
        template <class T>
        void registerType() {
            auto nodeType = make_unique<T>();
            switch (nodeType->type) {
                case NodeType::Type::FILTER:
                    filters[nodeType->symbol] = std::move(nodeType);
                break;
                case NodeType::Type::OPERATOR:
                    operators[nodeType->symbol] = std::move(nodeType);
                break;
                default:
                    assert(false);
                break;
            }
        }
    };

    struct TagNodeType : ContextualNodeType {
        enum class Composition {
            FREE,
            ENCLOSED,
            // When a tag needs to be like {% raw %} or {% comment %}, will take the entirety of its contents as a literal.
            LEXING_HALT
        };

        // This is insanely stupid.
        struct QualifierNodeType : NodeType {
            enum class Arity {
                NONARY,
                UNARY
            };
            Arity arity;

            QualifierNodeType(const string& symbol, Arity arity) : NodeType(NodeType::Type::QUALIFIER, symbol, 1, LIQUID_OPTIMIZATION_SCHEME_NONE), arity(arity) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };

        // For things like if/else, and whatnot. else is a free tag that sits inside the if statement.
        unordered_map<string, unique_ptr<NodeType>> intermediates;
        // For things for the forloop; like reversed, limit, etc... Super stupid, but Shopify threw them in, and there you are.
        unordered_map<string, unique_ptr<NodeType>> qualifiers;

        Composition composition;
        int minArguments;
        int maxArguments;

        TagNodeType(Composition composition, string symbol, int minArguments = -1, int maxArguments = -1, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : ContextualNodeType(NodeType::Type::TAG, symbol, -1, optimization), composition(composition), minArguments(minArguments), maxArguments(maxArguments) { }

        // Used for registering intermedaites and qualiifers.
        template <class T>
        T* registerType() {
            auto nodeType = make_unique<T>();
            T* pointer = nodeType.get();
            switch (nodeType->type) {
                case NodeType::Type::TAG:
                    intermediates[nodeType->symbol] = std::move(nodeType);
                break;
                case NodeType::Type::QUALIFIER:
                    qualifiers[nodeType->symbol] = std::move(nodeType);
                break;
                case NodeType::Type::OPERATOR:
                    operators[nodeType->symbol] = std::move(nodeType);
                break;
                case NodeType::Type::FILTER:
                    filters[nodeType->symbol] = std::move(nodeType);
                break;
                default:
                    assert(false);
                break;
            }
            return pointer;
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
        void compile(Compiler& compiler, const Node& node) const override;

        Node getOperand(Renderer& renderer, const Node& node, Variable store, int idx) const;
    };

    struct FilterNodeType : NodeType {
        int minArguments;
        int maxArguments;
        bool allowsWildcardQualifiers;

        // Wildcard Qualifier.
        struct QualifierNodeType : NodeType {
            QualifierNodeType() : NodeType(NodeType::Type::QUALIFIER, "", 1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };
        struct WildcardQualifierNodeType : QualifierNodeType {
            WildcardQualifierNodeType() { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override { return Node(); }
        };

        FilterNodeType(string symbol, int minArguments = -1, int maxArguments = -1, bool allowsWildcardQualifiers = false, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(NodeType::Type::FILTER, symbol, -1, optimization), minArguments(minArguments), maxArguments(maxArguments), allowsWildcardQualifiers(allowsWildcardQualifiers) { }

        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
        Node getArgument(Renderer& renderer, const Node& node, Variable store, int idx) const;

        void compile(Compiler& compiler, const Node& node) const override;
    };


    struct DotFilterNodeType : NodeType {
        DotFilterNodeType(string symbol, LiquidOptimizationScheme optimization = LIQUID_OPTIMIZATION_SCHEME_FULL) : NodeType(NodeType::Type::DOT_FILTER, symbol, -1, optimization) { }

        Node getOperand(Renderer& renderer, const Node& node, Variable store) const;
    };

    // Represents something a file, or whatnot. Allows the filling in of
    struct ContextBoundaryNode : NodeType {
        ContextBoundaryNode() : NodeType(NodeType::Type::CONTEXTUAL, "", -1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
        Node render(Renderer& renderer, const Node& node, Variable store) const override {
            renderer.nodeContext = this;
            return renderer.retrieveRenderedNode(*node.children[1].get(), store);
        }
    };

    struct Context {
        struct ConcatenationNode : NodeType {
            ConcatenationNode() : NodeType(Type::OPERATOR, "", -1, LIQUID_OPTIMIZATION_SCHEME_PARTIAL) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override;
            bool optimize(Optimizer& optimizer, Node& node, Variable store) const override;
            void compile(Compiler& compiler, const Node& node) const override;
        };

        struct OutputNode : ContextualNodeType {
            OutputNode() : ContextualNodeType(Type::OUTPUT, "echo", -1, LIQUID_OPTIMIZATION_SCHEME_FULL) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                assert(node.children.size() == 1);
                auto& argumentNode = node.children.front();
                assert(argumentNode->children.size() == 1);
                return Variant(renderer.getString(renderer.retrieveRenderedNode(*argumentNode->children[0].get(), store)));
            }

            void compile(Compiler& compiler, const Node& node) const override;
        };

        struct PassthruNode : NodeType {
            PassthruNode(NodeType::Type type) :NodeType(type) { }
            ~PassthruNode() { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                assert(node.children.size() == 1);
                auto it = node.children.begin();
                if ((*it)->type)
                    return (*it)->type->render(renderer, *it->get(), store);
                return *it->get();
            }


            void compile(Compiler& compiler, const Node& node) const override;
        };

        // These are purely for parsing purpose, and should not make their way to the rednerer.
        struct GroupNode : PassthruNode { GroupNode() : PassthruNode(Type::GROUP) { } };
        // These are purely for parsing purpose, and should not make their way to the rednerer.
        struct GroupDereferenceNode : PassthruNode { GroupDereferenceNode() : PassthruNode(Type::GROUP_DEREFERENCE) { } };
        // Used exclusively for tags. Should be never be rendered by itself.
        struct ArgumentNode : NodeType {
            ArgumentNode() : NodeType(Type::ARGUMENTS, "", -1, LIQUID_OPTIMIZATION_SCHEME_NONE) { }
            void compile(Compiler& compiler, const Node& node) const override;
        };
        struct ArrayLiteralNode : NodeType {
            ArrayLiteralNode() : NodeType(Type::ARRAY_LITERAL) { }
            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                Variant var { std::vector<Variant>() };
                var.a.reserve(node.children.size());
                for (size_t i = 0; i < node.children.size(); ++i)
                    var.a.push_back(move(renderer.retrieveRenderedNode(*node.children[i].get(), store).variant));
                return Node(move(var));
            }
        };

        struct UnknownFilterNode : FilterNodeType {
            UnknownFilterNode() : FilterNodeType("", -1, -1, true, LIQUID_OPTIMIZATION_SCHEME_NONE) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                if (renderer.logUnknownFilters)
                    renderer.pushUnknownFilterWarning(node, store);
                return Node();
            }
        };

        ECoercion coerciveness = COERCE_NONE;
        EFalsiness falsiness = FALSY_FALSE;
        bool disallowArrayLiterals = false;
        bool disallowGroupingOutsideAssign = false;

        struct VariableNode : NodeType {
            VariableNode() : NodeType(Type::VARIABLE) { }

            Node render(Renderer& renderer, const Node& node, Variable store) const override {
                pair<void*, Renderer::DropFunction> drop = renderer.getInternalDrop(node, store);
                if (drop.second) {
                    Node result = drop.second(renderer, node, store, drop.first);
                    if (result.type || result.variant.type != Variant::Type::VARIABLE)
                        return result;
                    return Node(renderer.parseVariant(result.variant.v));
                } else {
                    auto variableInfo = renderer.getVariable(node, store);
                    if (!variableInfo.first)
                        return Node();
                    return Node(renderer.parseVariant(variableInfo.second));
                }
            }

            bool optimize(Optimizer& optimizer, Node& node, Variable store) const override;
            void compile(Compiler& compiler, const Node& node) const override;
        };

        unordered_map<string, unique_ptr<NodeType>> tagTypes;
        unordered_map<string, unique_ptr<NodeType>> unaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> binaryOperatorTypes;
        unordered_map<string, unique_ptr<NodeType>> filterTypes;
        unordered_map<string, unique_ptr<NodeType>> dotFilterTypes;
        unordered_map<string, unique_ptr<LiteralType>> literalTypes;

        ConcatenationNode concatenationNodeType;
        OutputNode outputNodeType;
        VariableNode variableNodeType;
        GroupNode groupNodeType;
        GroupDereferenceNode groupDereferenceNodeType;
        ArgumentNode argumentNodeType;
        UnknownFilterNode unknownFilterNodeType;
        ArrayLiteralNode arrayLiteralNodeType;
        ContextBoundaryNode contextBoundaryNodeType;
        FilterNodeType::WildcardQualifierNodeType filterWildcardQualifierNodeType;

        const NodeType* getConcatenationNodeType() const { return &concatenationNodeType; }
        const NodeType* getOutputNodeType() const { return &outputNodeType; }
        const VariableNode* getVariableNodeType() const { return &variableNodeType; }
        const NodeType* getGroupNodeType() const { return &groupNodeType; }
        const NodeType* getGroupDereferenceNodeType() const { return &groupDereferenceNodeType; }
        const NodeType* getArgumentsNodeType() const { return &argumentNodeType; }
        const NodeType* getUnknownFilterNodeType() const { return &unknownFilterNodeType; }
        const NodeType* getArrayLiteralNodeType() const { return &arrayLiteralNodeType; }
        const NodeType* getContextBoundaryNodeType() const { return &contextBoundaryNodeType; }
        const NodeType* getFilterWildcardQualifierNodeType() const { return &filterWildcardQualifierNodeType; }

        NodeType* registerType(unique_ptr<NodeType> type) {
            NodeType* value = type.get();
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
            return value;
        }
        LiteralType* registerType(unique_ptr<LiteralType> type) {
            LiteralType* value = type.get();
            literalTypes[type->symbol] = move(type);
            return value;
        }
        template <class T> T* registerType() { return static_cast<T*>(registerType(make_unique<T>())); }

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

        const LiteralType* getLiteralType(string symbol) const {
            auto it = literalTypes.find(symbol);
            if (it == literalTypes.end())
                return nullptr;
            return static_cast<LiteralType*>(it->second.get());
        }

        void optimize(Node& ast, Variable store);

        enum EDialects {
            NO_DIALECT                      = 0,
            STRICT_STANDARD_DIALECT         = 1,
            PERMISSIVE_STANDARD_DIALECT     = 2,
            WEB_DIALECT                     = 4

        };
        Context(int dialects = NO_DIALECT);
    };
}

#endif
