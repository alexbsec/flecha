#ifndef FLECHA_SEMANTIC_DEFINITE_ASSIGNMENT_HPP
#define FLECHA_SEMANTIC_DEFINITE_ASSIGNMENT_HPP

#include "diagnostics/diagnostic_bag.hpp"
#include "semantic_model.hpp"

namespace flecha::semantic {

// Run after binding so shadowed variables have distinct symbol identities.
void AnalyzeDefiniteAssignment(const core::FunctionDeclaration& function,
                               const SemanticModel& model,
                               diagnostics::DiagnosticBag& diagnostics);

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_DEFINITE_ASSIGNMENT_HPP
