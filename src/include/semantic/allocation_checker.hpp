#ifndef FLECHA_SEMANTIC_ALLOCATION_CHECKER_HPP
#define FLECHA_SEMANTIC_ALLOCATION_CHECKER_HPP

#include "diagnostics/diagnostic_bag.hpp"
#include "semantic/semantic_model.hpp"

namespace flecha::semantic {
void AnalyzeAllocations(const core::CompilationUnit& unit,
                        const SemanticModel& model,
                        diagnostics::DiagnosticBag& diagnostics);
}
#endif
