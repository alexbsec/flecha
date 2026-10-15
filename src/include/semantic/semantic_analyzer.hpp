#ifndef FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP
#define FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "diagnostics/diagnostic_bag.hpp"
#include "semantic_model.hpp"

namespace flecha::semantic {

class SemanticAnalyzer final {
   public:
    explicit SemanticAnalyzer(diagnostics::DiagnosticBag& diagnostics)
        : _diagnostics(diagnostics) {}

    [[nodiscard]] SemanticModel Analyze(const core::CompilationUnit& unit);

   private:
    SemanticType PlaceType(const core::Expression& expression) const;
    void RequireUnsafe(source::SourceRange range);
    SemanticType ResolveType(const core::TypeNode& type);
    void RegisterBuiltIns();
    void RegisterFunctions(const core::CompilationUnit& unit);
    void AnalyzeFunction(const core::FunctionDeclaration& function);
    void AnalyzeBlock(const core::BlockStatement& block, bool createScope);
    void AnalyzeStatement(const core::Statement& statement);
    void AnalyzeBranch(const core::ConditionalBranch& branch);
    void CollectPositions(const core::BlockStatement& block,
                          std::size_t loopDepth);
    SemanticType AnalyzeExpression(const core::Expression& expression);
    SemanticType AnalyzeLiteral(const core::LiteralExpression& expression);
    SemanticType AnalyzeName(const core::NameExpression& expression);
    SemanticType AnalyzeUnary(const core::UnaryExpression& expression);
    SemanticType AnalyzeBinary(const core::BinaryExpression& expression);
    SemanticType AnalyzeAssignment(
        const core::AssignmentExpression& expression);
    SemanticType AnalyzeCall(const core::CallExpression& expression);
    SemanticType AnalyzeMemberCall(
        const core::MemberCallExpression& expression);
    SemanticType AnalyzeIndex(const core::IndexExpression& expression);
    SemanticType AnalyzeCast(const core::CastExpression& expression);
    [[nodiscard]] bool BorrowsLocalString(
        const core::Expression& expression) const;
    const Symbol* AddSymbol(std::unique_ptr<Symbol> symbol,
                            const core::AstNode& declaration);
    Scope& AddScope(const Scope* parent);
    void Report(diagnostics::DiagnosticCode code, source::SourceRange range,
                std::string message);

    diagnostics::DiagnosticBag& _diagnostics;
    SemanticModel _model;
    Scope* _currentScope = nullptr;
    const FunctionSymbol* _currentFunction = nullptr;
    std::unordered_map<std::string, source::SourceRange> _positions;
    std::unordered_set<const Symbol*> _localStringBorrows;
    std::size_t _loopDepth = 0;
    bool _inUnsafe = false;
};

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SEMANTIC_ANALYZER_HPP
