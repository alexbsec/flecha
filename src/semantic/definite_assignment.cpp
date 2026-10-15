#include "semantic/definite_assignment.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flecha::semantic {
namespace {

using SymbolSet = std::unordered_set<const Symbol*>;
using NodeId = std::size_t;

struct FlowNode final {
    std::vector<NodeId> successors;
    std::vector<NodeId> predecessors;
    std::vector<const core::BlockStatement*> scopes;
    const core::NameExpression* read = nullptr;
    const Symbol* write = nullptr;
    const Symbol* reset = nullptr;
    bool reachable = false;
    SymbolSet incoming;
    SymbolSet outgoing;
};

class AssignmentGraph final {
   public:
    explicit AssignmentGraph(const SemanticModel& model) : _model(model) {}

    void Analyze(const core::FunctionDeclaration& function,
                 diagnostics::DiagnosticBag& diagnostics) {
        SymbolSet parameters;
        for (const std::unique_ptr<core::Parameter>& parameter :
             function.Parameters()) {
            const Symbol* symbol = _model.DeclaredSymbol(*parameter);
            if (symbol != nullptr) parameters.insert(symbol);
        }
        const NodeId exit = AddNode({});
        const NodeId body = BuildBlock(function.Body(), exit);
        const NodeId entry = AddNode({body});
        for (const auto& [jump, position] : _jumps) {
            const auto target = _positions.find(position);
            if (target != _positions.end()) {
                _nodes[jump].successors.push_back(target->second);
            }
        }
        for (NodeId node = 0; node < _nodes.size(); ++node) {
            for (NodeId successor : _nodes[node].successors) {
                _nodes[successor].predecessors.push_back(node);
            }
        }

        std::deque<NodeId> worklist;
        std::vector<NodeId> pending{entry};
        while (!pending.empty()) {
            const NodeId node = pending.back();
            pending.pop_back();
            if (_nodes[node].reachable) continue;
            _nodes[node].reachable = true;
            worklist.push_back(node);
            for (NodeId successor : _nodes[node].successors) {
                pending.push_back(successor);
            }
        }

        // Must analysis starts at top, then descends to the greatest fixed
        // point. Only entry has a boundary state; unreachable cycles contribute
        // no predecessors. A backedge cannot initialize the first iteration.
        SymbolSet universe = parameters;
        for (const auto& [symbol, scope] : _symbolScopes) {
            universe.insert(symbol);
        }
        for (FlowNode& node : _nodes) node.outgoing = universe;
        std::vector<bool> queued(_nodes.size(), true);
        while (!worklist.empty()) {
            const NodeId index = worklist.front();
            worklist.pop_front();
            queued[index] = false;
            FlowNode& node = _nodes[index];
            SymbolSet incoming = index == entry ? parameters : universe;
            for (NodeId predecessor : node.predecessors) {
                if (!_nodes[predecessor].reachable) continue;
                std::erase_if(incoming, [&](const Symbol* symbol) {
                    return !_nodes[predecessor].outgoing.contains(symbol);
                });
            }
            // Leaving a lexical scope ends its locals' lifetimes. This also
            // prevents a later jump back into that scope from reusing
            // initialization from a previous activation.
            std::erase_if(incoming, [&](const Symbol* symbol) {
                const auto scope = _symbolScopes.find(symbol);
                return scope != _symbolScopes.end() &&
                       std::find(node.scopes.begin(), node.scopes.end(),
                                 scope->second) == node.scopes.end();
            });
            node.incoming = incoming;
            if (node.reset != nullptr) incoming.erase(node.reset);
            if (node.write != nullptr) incoming.insert(node.write);
            if (incoming == node.outgoing) continue;
            node.outgoing = std::move(incoming);
            for (NodeId successor : node.successors) {
                if (!queued[successor]) {
                    queued[successor] = true;
                    worklist.push_back(successor);
                }
            }
        }

        // Source order is stable even though the graph is built backwards.
        std::vector<const core::NameExpression*> uninitialized;
        for (const FlowNode& node : _nodes) {
            if (!node.reachable || node.read == nullptr) continue;
            const Symbol* symbol = _model.SymbolFor(*node.read);
            if (symbol != nullptr && symbol->Kind() != SymbolKind::Function &&
                !node.incoming.contains(symbol)) {
                uninitialized.push_back(node.read);
            }
        }
        std::sort(uninitialized.begin(), uninitialized.end(),
                  [](const core::NameExpression* left,
                     const core::NameExpression* right) {
                      return left->Range().start.offset <
                             right->Range().start.offset;
                  });
        for (const core::NameExpression* read : uninitialized) {
            diagnostics.Report(
                diagnostics::DiagnosticCode::UninitializedVariable,
                read->Range(),
                "variable '" + read->Name() +
                    "' may be read before it is initialized");
        }
    }

   private:
    NodeId AddNode(std::vector<NodeId> successors) {
        const NodeId node = _nodes.size();
        _nodes.push_back(FlowNode{});
        _nodes.back().successors = std::move(successors);
        _nodes.back().scopes = _scopes;
        return node;
    }

    NodeId BuildBlock(const core::BlockStatement& block, NodeId next,
                      const core::AlsoClause* also = nullptr) {
        _scopes.push_back(&block);
        // Keep the body's scope active through its trailing also clause,
        // matching lowering's combined block. The also body adds its own scope;
        // name binding still keeps the two bodies' declarations separate.
        const NodeId end = AddNode({next});
        next = end;
        if (also != nullptr) {
            const NodeId body = BuildBlock(also->Body(), next);
            next = BuildCondition(also->Condition(), body, next);
        }
        for (auto statement = block.Statements().rbegin();
             statement != block.Statements().rend(); ++statement) {
            next = BuildStatement(**statement, next);
        }
        _scopes.pop_back();
        return next;
    }

    NodeId BuildBranch(const core::ConditionalBranch& branch, NodeId next,
                       NodeId alternative) {
        const NodeId body = BuildBlock(branch.Body(), next, branch.Also());
        return BuildCondition(branch.Condition(), body, alternative);
    }

    NodeId BuildStatement(const core::Statement& statement, NodeId next) {
        switch (statement.Kind()) {
            case core::AstNodeKind::VariableDeclarationStatement: {
                const auto& declaration =
                    static_cast<const core::VariableDeclarationStatement&>(
                        statement);
                const Symbol* symbol = _model.DeclaredSymbol(declaration);
                if (symbol != nullptr)
                    _symbolScopes.emplace(symbol, _scopes.back());
                const NodeId store = AddNode({next});
                _nodes[store].reset = symbol;
                if (declaration.Initializer() == nullptr) return store;
                _nodes[store].write = symbol;
                return BuildExpression(*declaration.Initializer(), store);
            }
            case core::AstNodeKind::ExpressionStatement:
                return BuildExpression(
                    static_cast<const core::ExpressionStatement&>(statement)
                        .Value(),
                    next);
            case core::AstNodeKind::ReturnStatement: {
                const auto& result =
                    static_cast<const core::ReturnStatement&>(statement);
                const NodeId exit = AddNode({});
                return result.Value() == nullptr
                           ? exit
                           : BuildExpression(*result.Value(), exit);
            }
            case core::AstNodeKind::IfStatement: {
                const auto& conditional =
                    static_cast<const core::IfStatement&>(statement);
                NodeId alternative =
                    conditional.ElseBody() == nullptr
                        ? next
                        : BuildBlock(*conditional.ElseBody(), next);
                for (auto branch = conditional.ElseIfBranches().rbegin();
                     branch != conditional.ElseIfBranches().rend(); ++branch) {
                    alternative = BuildBranch(**branch, next, alternative);
                }
                return BuildBranch(conditional.PrimaryBranch(), next,
                                   alternative);
            }
            case core::AstNodeKind::WhileStatement: {
                const auto& loop =
                    static_cast<const core::WhileStatement&>(statement);
                const NodeId header = AddNode({});
                _loops.push_back({header, next});
                const NodeId body =
                    BuildBlock(loop.Body(), header, loop.Also());
                _loops.pop_back();
                const NodeId condition =
                    BuildCondition(loop.Condition(), body, next);
                _nodes[header].successors.push_back(condition);
                return header;
            }
            case core::AstNodeKind::ProceedStatement:
                return AddNode(_loops.empty()
                                   ? std::vector<NodeId>{}
                                   : std::vector<NodeId>{_loops.back().first});
            case core::AstNodeKind::BreakStatement:
                return AddNode(_loops.empty()
                                   ? std::vector<NodeId>{}
                                   : std::vector<NodeId>{_loops.back().second});
            case core::AstNodeKind::JumpToStatement: {
                const NodeId jump = AddNode({});
                _jumps.emplace_back(
                    jump, static_cast<const core::JumpToStatement&>(statement)
                              .Position());
                return jump;
            }
            case core::AstNodeKind::PositionStatement: {
                const NodeId position = AddNode({next});
                _positions.emplace(
                    static_cast<const core::PositionStatement&>(statement)
                        .Name(),
                    position);
                return position;
            }
            default:
                return next;
        }
    }

    NodeId BuildCondition(const core::Expression& expression, NodeId yes,
                          NodeId no) {
        if (expression.Kind() == core::AstNodeKind::ParenthesizedExpression) {
            return BuildCondition(
                static_cast<const core::ParenthesizedExpression&>(expression)
                    .Inner(),
                yes, no);
        }
        if (expression.Kind() == core::AstNodeKind::UnaryExpression) {
            const auto& unary =
                static_cast<const core::UnaryExpression&>(expression);
            if (unary.OperatorType() == TokenType::Bang) {
                return BuildCondition(unary.Operand(), no, yes);
            }
        }
        if (expression.Kind() == core::AstNodeKind::BinaryExpression &&
            _model.TypeOf(expression).Kind() == SemanticTypeKind::Bool) {
            const auto& binary =
                static_cast<const core::BinaryExpression&>(expression);
            if (binary.OperatorType() == TokenType::And) {
                const NodeId right = BuildCondition(binary.Right(), yes, no);
                return BuildCondition(binary.Left(), right, no);
            }
            if (binary.OperatorType() == TokenType::Or) {
                const NodeId right = BuildCondition(binary.Right(), yes, no);
                return BuildCondition(binary.Left(), yes, right);
            }
        }
        // Deliberately keep both outcomes, including literal conditions. This
        // is definite assignment, not constant folding or value-range analysis.
        return BuildExpression(expression, AddNode({yes, no}));
    }

    NodeId BuildExpression(const core::Expression& expression, NodeId next) {
        switch (expression.Kind()) {
            case core::AstNodeKind::NameExpression: {
                const NodeId read = AddNode({next});
                _nodes[read].read =
                    static_cast<const core::NameExpression*>(&expression);
                return read;
            }
            case core::AstNodeKind::ParenthesizedExpression:
                return BuildExpression(
                    static_cast<const core::ParenthesizedExpression&>(
                        expression)
                        .Inner(),
                    next);
            case core::AstNodeKind::UnaryExpression:
                return BuildExpression(
                    static_cast<const core::UnaryExpression&>(expression)
                        .Operand(),
                    next);
            case core::AstNodeKind::CastExpression:
                return BuildExpression(
                    static_cast<const core::CastExpression&>(expression)
                        .Value(),
                    next);
            case core::AstNodeKind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                if (_model.TypeOf(expression).Kind() ==
                        SemanticTypeKind::Bool &&
                    (binary.OperatorType() == TokenType::And ||
                     binary.OperatorType() == TokenType::Or)) {
                    return BuildCondition(expression, next, next);
                }
                const NodeId right = BuildExpression(binary.Right(), next);
                return BuildExpression(binary.Left(), right);
            }
            case core::AstNodeKind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                const NodeId store = AddNode({next});
                if (assignment.Target().Kind() ==
                    core::AstNodeKind::NameExpression) {
                    _nodes[store].write = _model.SymbolFor(
                        static_cast<const core::NameExpression&>(
                            assignment.Target()));
                }
                return BuildExpression(assignment.Value(), store);
            }
            case core::AstNodeKind::CallExpression: {
                const auto& call =
                    static_cast<const core::CallExpression&>(expression);
                for (auto argument = call.Arguments().rbegin();
                     argument != call.Arguments().rend(); ++argument) {
                    next = BuildExpression(**argument, next);
                }
                return BuildExpression(call.Callee(), next);
            }
            case core::AstNodeKind::MemberCallExpression: {
                const auto& call =
                    static_cast<const core::MemberCallExpression&>(expression);
                for (auto argument = call.Arguments().rbegin();
                     argument != call.Arguments().rend(); ++argument) {
                    next = BuildExpression(**argument, next);
                }
                return BuildExpression(call.Receiver(), next);
            }
            case core::AstNodeKind::IndexExpression: {
                const auto& index =
                    static_cast<const core::IndexExpression&>(expression);
                const NodeId subscript = BuildExpression(index.Index(), next);
                return BuildExpression(index.Receiver(), subscript);
            }
            default:
                return next;
        }
    }

    const SemanticModel& _model;
    std::vector<FlowNode> _nodes;
    std::vector<const core::BlockStatement*> _scopes;
    std::unordered_map<const Symbol*, const core::BlockStatement*>
        _symbolScopes;
    std::unordered_map<std::string, NodeId> _positions;
    std::vector<std::pair<NodeId, std::string>> _jumps;
    std::vector<std::pair<NodeId, NodeId>> _loops;
};

}  // namespace

void AnalyzeDefiniteAssignment(const core::FunctionDeclaration& function,
                               const SemanticModel& model,
                               diagnostics::DiagnosticBag& diagnostics) {
    AssignmentGraph(model).Analyze(function, diagnostics);
}

}  // namespace flecha::semantic
