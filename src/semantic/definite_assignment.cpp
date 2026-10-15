#include "semantic/definite_assignment.hpp"

#include <algorithm>
#include <deque>
#include <unordered_set>

#include "semantic/control_flow_graph.hpp"
namespace flecha::semantic {
void AnalyzeDefiniteAssignment(const core::FunctionDeclaration& function,
                               const SemanticModel& model,
                               diagnostics::DiagnosticBag& diagnostics,
                               const ControlFlowGraph& graph) {
    using SymbolSet = std::unordered_set<const Symbol*>;
    SymbolSet parameters;
    for (const auto& parameter : function.Parameters()) {
        const Symbol* symbol = model.DeclaredSymbol(*parameter);
        if (symbol != nullptr) parameters.insert(symbol);
    }
    const NodeId entry = graph.entry;
    std::deque<NodeId> worklist;
    for (NodeId index = 0; index < graph.nodes.size(); ++index) {
        if (graph.nodes[index].reachable) worklist.push_back(index);
    }
    // Must analysis starts at top, then descends to the greatest fixed
    // point. Only entry has a boundary state; unreachable cycles contribute
    // no predecessors. A backedge cannot initialize the first iteration.
    SymbolSet universe = parameters;
    for (const auto& [symbol, scope] : graph.symbolScopes) {
        universe.insert(symbol);
    }
    std::vector<SymbolSet> outgoing(graph.nodes.size(), universe);
    std::vector<SymbolSet> incomingStates(graph.nodes.size());
    std::vector<bool> queued(graph.nodes.size(), true);
    while (!worklist.empty()) {
        const NodeId index = worklist.front();
        worklist.pop_front();
        queued[index] = false;
        const FlowNode& node = graph.nodes[index];
        SymbolSet incoming = index == entry ? parameters : universe;
        for (NodeId predecessor : node.predecessors) {
            if (!graph.nodes[predecessor].reachable) continue;
            std::erase_if(incoming, [&](const Symbol* symbol) {
                return !outgoing[predecessor].contains(symbol);
            });
        }
        // Leaving a lexical scope ends its locals' lifetimes. This also
        // prevents a later jump back into that scope from reusing
        // initialization from a previous activation.
        std::erase_if(incoming, [&](const Symbol* symbol) {
            const auto scope = graph.symbolScopes.find(symbol);
            return scope != graph.symbolScopes.end() &&
                   std::find(node.scopes.begin(), node.scopes.end(),
                             scope->second) == node.scopes.end();
        });
        incomingStates[index] = incoming;
        if (node.reset != nullptr) incoming.erase(node.reset);
        if (node.write != nullptr) incoming.insert(node.write);
        if (incoming == outgoing[index]) continue;
        outgoing[index] = std::move(incoming);
        for (NodeId successor : node.successors) {
            if (!queued[successor]) {
                queued[successor] = true;
                worklist.push_back(successor);
            }
        }
    }

    // Source order is stable even though the graph is built backwards.
    std::vector<const core::NameExpression*> uninitialized;
    for (NodeId index = 0; index < graph.nodes.size(); ++index) {
        const FlowNode& node = graph.nodes[index];
        if (!node.reachable || node.read == nullptr) continue;
        const Symbol* symbol = model.SymbolFor(*node.read);
        if (symbol != nullptr && symbol->Kind() != SymbolKind::Function &&
            !incomingStates[index].contains(symbol)) {
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
        diagnostics.Report(diagnostics::DiagnosticCode::UninitializedVariable,
                           read->Range(),
                           "variable '" + read->Name() +
                               "' may be read before it is initialized");
    }
}
}  // namespace flecha::semantic
