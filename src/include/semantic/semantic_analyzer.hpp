#ifndef FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP
#define FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP

#include "diagnostics/diagnostic_bag.hpp"
#include "semantic_model.hpp"

namespace flecha::semantic {

class SemanticAnalyzer final {
   public:
    explicit SemanticAnalyzer(diagnostics::DiagnosticBag& diagnostics)
        : _diagnostics(diagnostics) {}

    [[nodiscard]] SemanticModel Analyze(const core::CompilationUnit& unit);

   private:
    void RegisterFunctions(const core::CompilationUnit& unit);
    void AnalyzeFunction(const core::FunctionDeclaration& function);
    void AnalyzeBlock(const core::BlockStatement& block, bool createScope);
    void AnalyzeStatement(const core::Statement& statement);
    void AnalyzeBranch(const core::ConditionalBranch& branch);
    SemanticType AnalyzeExpression(const core::Expression& expression);
    SemanticType AnalyzeLiteral(const core::LiteralExpression& expression);
    SemanticType AnalyzeName(const core::NameExpression& expression);
    SemanticType AnalyzeUnary(const core::UnaryExpression& expression);
    SemanticType AnalyzeBinary(const core::BinaryExpression& expression);
    SemanticType AnalyzeAssignment(
        const core::AssignmentExpression& expression);
    SemanticType AnalyzeCall(const core::CallExpression& expression);
    const Symbol* AddSymbol(std::unique_ptr<Symbol> symbol,
                            const core::AstNode& declaration);
    Scope& AddScope(const Scope* parent);
    void Report(diagnostics::DiagnosticCode code, source::SourceRange range,
                std::string message);

    diagnostics::DiagnosticBag& _diagnostics;
    SemanticModel _model;
    Scope* _currentScope = nullptr;
    const FunctionSymbol* _currentFunction = nullptr;
};

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP
