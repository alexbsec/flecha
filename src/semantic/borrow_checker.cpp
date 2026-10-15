#include "semantic/borrow_checker.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <utility>

namespace flecha::semantic {
namespace {
using Symbols = std::set<const Symbol*>;
using Code = diagnostics::DiagnosticCode;
using Kind = core::AstNodeKind;

bool IsReference(const Symbol* symbol) {
    return symbol != nullptr &&
           symbol->Type().Kind() == SemanticTypeKind::Reference;
}

const core::Expression& Unwrap(const core::Expression& expression) {
    if (expression.Kind() == Kind::ParenthesizedExpression)
        return Unwrap(
            static_cast<const core::ParenthesizedExpression&>(expression)
                .Inner());
    return expression;
}

// A reborrow retains its owner and all parent handles. The sets are finite even
// on back edges; joins retain every possible origin rather than choosing one.
struct Origin final {
    const Symbol* owner;
    Symbols parents;
    auto operator<=>(const Origin&) const = default;
};
using Origins = std::set<Origin>;
struct Handle final {
    Origins origins;
    bool moved = false;
    bool operator==(const Handle&) const = default;
};
using State = std::map<const Symbol*, Handle>;
struct Value final {
    Origins origins;
    const Symbol* handle = nullptr;
    std::size_t temporary = 0;
};
struct Loan final {
    Value value;
    bool exclusive;
    std::size_t id;
};

void Merge(State& destination, const State& source) {
    for (const auto& [symbol, handle] : source) {
        Handle& target = destination[symbol];
        target.origins.insert(handle.origins.begin(), handle.origins.end());
        target.moved = target.moved || handle.moved;
    }
}

class BorrowChecker final {
   public:
    BorrowChecker(const SemanticModel& model,
                  diagnostics::DiagnosticBag& diagnostics,
                  const ControlFlowGraph& graph)
        : _model(model),
          _diagnostics(diagnostics),
          _graph(graph),
          _live(graph.nodes.size()),
          _uses(graph.nodes.size()),
          _defs(graph.nodes.size()) {}

    void Analyze(const core::FunctionDeclaration& function) {
        State seed;
        for (const std::unique_ptr<core::Parameter>& parameter :
             function.Parameters()) {
            const Symbol* symbol = _model.DeclaredSymbol(*parameter);
            if (IsReference(symbol)) seed[symbol].origins.insert({symbol, {}});
        }
        ComputeLiveness();
        std::vector<State> incoming(_graph.nodes.size()),
            outgoing(_graph.nodes.size());
        std::deque<NodeId> pending;
        std::vector<bool> queued(_graph.nodes.size(), false);
        for (NodeId id = 0; id < _graph.nodes.size(); ++id) {
            if (_graph.nodes[id].reachable) {
                pending.push_back(id);
                queued[id] = true;
            }
        }
        while (!pending.empty()) {
            const NodeId id = pending.front();
            pending.pop_front();
            queued[id] = false;
            State next = id == _graph.entry ? seed : State{};
            for (NodeId predecessor : _graph.nodes[id].predecessors)
                if (_graph.nodes[predecessor].reachable)
                    Merge(next, outgoing[predecessor]);
            PruneScope(next, _graph.nodes[id]);
            incoming[id] = next;
            Transfer(id, next);
            if (next == outgoing[id]) continue;
            outgoing[id] = std::move(next);
            for (NodeId successor : _graph.nodes[id].successors) {
                if (!queued[successor]) {
                    pending.push_back(successor);
                    queued[successor] = true;
                }
            }
        }
        // Report only from the fixed point, never transient worklist states.
        _report = true;
        for (NodeId id = 0; id < _graph.nodes.size(); ++id)
            if (_graph.nodes[id].reachable) Transfer(id, incoming[id]);
        for (const auto& [key, message] : _reported) {
            const auto& [range, code] = key;
            _diagnostics.Report(code, range, message);
        }
    }

   private:
    const Symbol* Named(const core::Expression& expression) const {
        const core::Expression& inner = Unwrap(expression);
        return inner.Kind() == Kind::NameExpression
                   ? _model.SymbolFor(
                         static_cast<const core::NameExpression&>(inner))
                   : nullptr;
    }

    void Report(Code code, source::SourceRange range, std::string message) {
        if (_report)
            _reported.try_emplace(std::make_pair(range, code),
                                  std::move(message));
    }

    void CollectUses(const core::Expression& expression, Symbols& uses) const {
        switch (expression.Kind()) {
            case Kind::NameExpression: {
                const Symbol* symbol = Named(expression);
                if (IsReference(symbol)) uses.insert(symbol);
                break;
            }
            case Kind::ParenthesizedExpression:
                CollectUses(Unwrap(expression), uses);
                break;
            case Kind::UnaryExpression:
                CollectUses(
                    static_cast<const core::UnaryExpression&>(expression)
                        .Operand(),
                    uses);
                break;
            case Kind::CastExpression:
                CollectUses(static_cast<const core::CastExpression&>(expression)
                                .Value(),
                            uses);
                break;
            case Kind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                CollectUses(binary.Left(), uses);
                CollectUses(binary.Right(), uses);
                break;
            }
            case Kind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                if (Named(assignment.Target()) == nullptr)
                    CollectUses(assignment.Target(), uses);
                CollectUses(assignment.Value(), uses);
                break;
            }
            case Kind::CallExpression: {
                const auto& call =
                    static_cast<const core::CallExpression&>(expression);
                CollectUses(call.Callee(), uses);
                for (const auto& argument : call.Arguments())
                    CollectUses(*argument, uses);
                break;
            }
            case Kind::MemberCallExpression: {
                const auto& call =
                    static_cast<const core::MemberCallExpression&>(expression);
                CollectUses(call.Receiver(), uses);
                for (const auto& argument : call.Arguments())
                    CollectUses(*argument, uses);
                break;
            }
            case Kind::IndexExpression: {
                const auto& index =
                    static_cast<const core::IndexExpression&>(expression);
                CollectUses(index.Receiver(), uses);
                CollectUses(index.Index(), uses);
                break;
            }
            default:
                break;
        }
    }

    void ComputeLiveness() {
        for (NodeId id = 0; id < _graph.nodes.size(); ++id) {
            const FlowNode& node = _graph.nodes[id];
            if (node.declaration != nullptr) {
                const Symbol* symbol = _model.DeclaredSymbol(*node.declaration);
                if (IsReference(symbol)) _defs[id].insert(symbol);
                if (node.declaration->Initializer() != nullptr)
                    CollectUses(*node.declaration->Initializer(), _uses[id]);
            }
            if (node.evaluation != nullptr) {
                CollectUses(*node.evaluation, _uses[id]);
                const core::Expression& expression = Unwrap(*node.evaluation);
                if (expression.Kind() == Kind::AssignmentExpression) {
                    const Symbol* symbol =
                        Named(static_cast<const core::AssignmentExpression&>(
                                  expression)
                                  .Target());
                    if (IsReference(symbol)) _defs[id].insert(symbol);
                }
            }
        }
        bool changed = true;
        while (changed) {
            changed = false;
            for (NodeId id = 0; id < _graph.nodes.size(); ++id) {
                if (!_graph.nodes[id].reachable) continue;
                Symbols next = _uses[id];
                for (NodeId successor : _graph.nodes[id].successors)
                    for (const Symbol* symbol : _live[successor])
                        if (!_defs[id].contains(symbol)) next.insert(symbol);
                if (next != _live[id]) {
                    _live[id] = std::move(next);
                    changed = true;
                }
            }
        }
    }

    void PruneScope(State& state, const FlowNode& node) const {
        std::erase_if(state, [&](const auto& entry) {
            const auto found = _graph.symbolScopes.find(entry.first);
            return found != _graph.symbolScopes.end() &&
                   std::find(node.scopes.begin(), node.scopes.end(),
                             found->second) == node.scopes.end();
        });
    }

    void Transfer(NodeId id, State& state) {
        _state = &state;
        _active = _live[id];
        _temporaries.clear();
        _pinned.clear();
        _nextTemporary = 1;
        const FlowNode& node = _graph.nodes[id];
        if (node.declaration != nullptr) {
            const Symbol* symbol = _model.DeclaredSymbol(*node.declaration);
            // A backward jump can execute a declaration again while another
            // handle still borrows its old storage.
            CheckHandleChange(symbol, node.declaration->NameRange());
            if (!IsReference(symbol))
                CheckAccess({{{symbol, {}}}, nullptr, 0}, true,
                            node.declaration->NameRange());
            Value value;
            if (node.declaration->Initializer() != nullptr)
                value = Evaluate(*node.declaration->Initializer());
            if (IsReference(symbol))
                Store(symbol, value, node.declaration->Initializer()->Range());
        }
        if (node.evaluation != nullptr) (void)Evaluate(*node.evaluation);
    }

    void CheckHandleChange(const Symbol* symbol, source::SourceRange range) {
        if (_pinned.contains(symbol)) {
            Report(Code::BorrowConflict, range,
                   "cannot move or rebind a reference used by the pending "
                   "assignment");
        }
        for (const auto& [handle, value] : *_state) {
            if (!_active.contains(handle)) continue;
            for (const Origin& origin : value.origins)
                if (origin.parents.contains(symbol))
                    Report(Code::BorrowConflict, range,
                           "cannot move or rebind '" + symbol->Name() +
                               "' while a child reborrow is live");
        }
        for (const Loan& loan : _temporaries)
            for (const Origin& origin : loan.value.origins)
                if (origin.parents.contains(symbol))
                    Report(Code::BorrowConflict, range,
                           "reference is borrowed for the current expression");
    }

    void CheckAccess(const Value& access, bool write,
                     source::SourceRange range) {
        for (const Origin& origin : access.origins) {
            for (const auto& [symbol, value] : *_state) {
                if (!_active.contains(symbol) || symbol == access.handle ||
                    origin.parents.contains(symbol))
                    continue;
                const bool exclusive = !symbol->Type().Target()->IsConst();
                for (const Origin& loan : value.origins) {
                    if (loan.owner == origin.owner && (write || exclusive)) {
                        Report(Code::BorrowConflict, range,
                               "access to '" + origin.owner->Name() +
                                   "' conflicts with live reference '" +
                                   symbol->Name() + "'");
                    }
                }
            }
            for (const Loan& loan : _temporaries) {
                if (loan.id == access.temporary) continue;
                for (const Origin& other : loan.value.origins)
                    if (other.owner == origin.owner &&
                        (write || loan.exclusive))
                        Report(Code::BorrowConflict, range,
                               "access conflicts with a borrow in the current "
                               "expression");
            }
        }
    }

    bool Outlives(const Symbol* owner, const Symbol* holder) const {
        const auto ownerScope = _graph.symbolScopes.find(owner);
        if (ownerScope == _graph.symbolScopes.end()) return true;
        const auto holderScope = _graph.symbolScopes.find(holder);
        if (holderScope == _graph.symbolScopes.end()) return false;
        const core::BlockStatement* scope = holderScope->second;
        while (scope != nullptr) {
            if (scope == ownerScope->second) return true;
            scope = _graph.scopeParents.at(scope);
        }
        return false;
    }

    void RemoveTemporary(std::size_t id) {
        std::erase_if(_temporaries,
                      [id](const Loan& loan) { return loan.id == id; });
    }

    void Store(const Symbol* destination, Value value,
               source::SourceRange range) {
        CheckHandleChange(destination, range);
        for (const Origin& origin : value.origins) {
            if (!Outlives(origin.owner, destination))
                Report(Code::ReferenceEscape, range,
                       "reference would outlive its owner '" +
                           origin.owner->Name() + "'");
            for (const Symbol* parent : origin.parents)
                if (parent == destination || !Outlives(parent, destination))
                    Report(Code::ReferenceEscape, range,
                           "reference would outlive or overwrite its parent "
                           "reborrow");
        }
        if (value.handle != nullptr &&
            !value.handle->Type().Target()->IsConst()) {
            CheckHandleChange(value.handle, range);
            CheckAccess(value, true, range);
            if (value.handle->Type().IsConst())
                Report(Code::MovedReference, range,
                       "cannot move from a fixed mutable-reference binding; "
                       "reborrow it instead");
            (*_state)[value.handle] = {{}, true};
        }
        RemoveTemporary(value.temporary);
        (*_state)[destination] = {std::move(value.origins), false};
        _active.insert(destination);
    }

    Value Place(const core::Expression& expression) {
        const core::Expression& inner = Unwrap(expression);
        const Symbol* symbol = Named(inner);
        if (symbol != nullptr) return {{{symbol, {}}}, nullptr, 0};
        if (inner.Kind() == Kind::UnaryExpression) {
            const auto& unary =
                static_cast<const core::UnaryExpression&>(inner);
            Value value = Evaluate(unary.Operand());
            if (_model.TypeOf(unary.Operand()).Kind() ==
                SemanticTypeKind::Reference)
                return value;
        }
        return {};
    }

    Value Borrow(Value value, bool exclusive, source::SourceRange range) {
        CheckAccess(value, exclusive, range);
        Origins origins;
        for (Origin origin : value.origins) {
            if (value.handle != nullptr) origin.parents.insert(value.handle);
            origins.insert(std::move(origin));
        }
        RemoveTemporary(value.temporary);
        Value result{std::move(origins), nullptr, _nextTemporary++};
        _temporaries.push_back({result, exclusive, result.temporary});
        return result;
    }

    Value Evaluate(const core::Expression& expression) {
        switch (expression.Kind()) {
            case Kind::ParenthesizedExpression:
                return Evaluate(Unwrap(expression));
            case Kind::NameExpression: {
                const Symbol* symbol = Named(expression);
                if (IsReference(symbol)) {
                    const Handle& handle = (*_state)[symbol];
                    if (handle.moved)
                        Report(Code::MovedReference, expression.Range(),
                               "use of possibly moved reference '" +
                                   symbol->Name() + "'");
                    return {handle.origins, symbol, 0};
                }
                if (symbol != nullptr && symbol->Kind() != SymbolKind::Function)
                    CheckAccess({{{symbol, {}}}, nullptr, 0}, false,
                                expression.Range());
                return {};
            }
            case Kind::UnaryExpression: {
                const auto& unary =
                    static_cast<const core::UnaryExpression&>(expression);
                if (unary.OperatorType() == TokenType::Ampersand)
                    return Borrow(
                        Place(unary.Operand()),
                        !_model.TypeOf(expression).Target()->IsConst(),
                        expression.Range());
                if (unary.OperatorType() == TokenType::RawAddress) {
                    CheckAccess(Place(unary.Operand()), true,
                                expression.Range());
                    return {};
                }
                if (unary.OperatorType() == TokenType::Mul) {
                    CheckAccess(Place(expression), false, expression.Range());
                    return {};
                }
                (void)Evaluate(unary.Operand());
                return {};
            }
            case Kind::AssignmentExpression: {
                const auto& assignment =
                    static_cast<const core::AssignmentExpression&>(expression);
                const Symbol* symbol = Named(assignment.Target());
                if (IsReference(symbol)) {
                    Value value = Evaluate(assignment.Value());
                    Store(symbol, std::move(value), assignment.Value().Range());
                    return {(*_state)[symbol].origins, symbol, 0};
                }
                Value place = Place(assignment.Target());
                const Symbols previousPinned = _pinned;
                if (place.handle != nullptr) _pinned.insert(place.handle);
                for (const Origin& origin : place.origins)
                    _pinned.insert(origin.parents.begin(),
                                   origin.parents.end());
                (void)Evaluate(assignment.Value());
                CheckAccess(place, true, assignment.Target().Range());
                _pinned = previousPinned;
                return {};
            }
            case Kind::BinaryExpression: {
                const auto& binary =
                    static_cast<const core::BinaryExpression&>(expression);
                (void)Evaluate(binary.Left());
                const bool conditional =
                    _model.TypeOf(expression).Kind() ==
                        SemanticTypeKind::Bool &&
                    (binary.OperatorType() == TokenType::And ||
                     binary.OperatorType() == TokenType::Or);
                const State skipped = conditional ? *_state : State{};
                (void)Evaluate(binary.Right());
                if (conditional) Merge(*_state, skipped);
                return {};
            }
            case Kind::CallExpression: {
                const auto& call =
                    static_cast<const core::CallExpression&>(expression);
                const Symbol* symbol = Named(call.Callee());
                const auto* function =
                    static_cast<const FunctionSymbol*>(symbol);
                const std::vector<Loan> previousLoans = _temporaries;
                for (std::size_t index = 0; index < call.Arguments().size();
                     ++index) {
                    Value value = Evaluate(*call.Arguments()[index]);
                    if (function != nullptr &&
                        index < function->ParameterTypes().size() &&
                        function->ParameterTypes()[index].Kind() ==
                            SemanticTypeKind::Reference) {
                        (void)Borrow(std::move(value),
                                     !function->ParameterTypes()[index]
                                          .Target()
                                          ->IsConst(),
                                     call.Arguments()[index]->Range());
                    }
                }
                _temporaries = previousLoans;
                return {};
            }
            case Kind::CastExpression:
                (void)Evaluate(
                    static_cast<const core::CastExpression&>(expression)
                        .Value());
                return {};
            case Kind::MemberCallExpression: {
                const auto& call =
                    static_cast<const core::MemberCallExpression&>(expression);
                (void)Evaluate(call.Receiver());
                for (const auto& argument : call.Arguments())
                    (void)Evaluate(*argument);
                return {};
            }
            case Kind::IndexExpression: {
                const auto& index =
                    static_cast<const core::IndexExpression&>(expression);
                (void)Evaluate(index.Receiver());
                (void)Evaluate(index.Index());
                return {};
            }
            default:
                return {};
        }
    }

    const SemanticModel& _model;
    diagnostics::DiagnosticBag& _diagnostics;
    const ControlFlowGraph& _graph;
    std::vector<Symbols> _live, _uses, _defs;
    State* _state = nullptr;
    Symbols _active, _pinned;
    std::vector<Loan> _temporaries;
    std::size_t _nextTemporary = 1;
    bool _report = false;
    std::map<std::pair<source::SourceRange, Code>, std::string> _reported;
};
}  // namespace

void AnalyzeBorrows(const core::FunctionDeclaration& function,
                    const SemanticModel& model,
                    diagnostics::DiagnosticBag& diagnostics,
                    const ControlFlowGraph& graph) {
    BorrowChecker(model, diagnostics, graph).Analyze(function);
}
}  // namespace flecha::semantic
