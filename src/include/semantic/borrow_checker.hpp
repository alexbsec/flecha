#ifndef FLECHA_SEMANTIC_BORROW_CHECKER_HPP
#define FLECHA_SEMANTIC_BORROW_CHECKER_HPP

#include "diagnostics/diagnostic_bag.hpp"
#include "semantic/control_flow_graph.hpp"

namespace flecha::semantic {
void AnalyzeBorrows(const core::FunctionDeclaration& function,
                    const SemanticModel& model,
                    diagnostics::DiagnosticBag& diagnostics,
                    const ControlFlowGraph& graph);
}
#endif
