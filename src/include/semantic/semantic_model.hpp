#ifndef FLECHA_SEMANTIC_SEMANTIC_MODEL_HPP
#define FLECHA_SEMANTIC_SEMANTIC_MODEL_HPP

#include <memory>
#include <unordered_map>
#include <vector>

#include "core/ast.hpp"
#include "scope.hpp"

namespace flecha::semantic {

class SemanticAnalyzer;

class SemanticModel final {
   public:
    SemanticModel() = default;
    SemanticModel(const SemanticModel&) = delete;
    SemanticModel& operator=(const SemanticModel&) = delete;
    SemanticModel(SemanticModel&&) = default;
    SemanticModel& operator=(SemanticModel&&) = default;

    [[nodiscard]] const Symbol* SymbolFor(
        const core::NameExpression& expression) const;
    [[nodiscard]] const Symbol* DeclaredSymbol(
        const core::AstNode& declaration) const;
    [[nodiscard]] const std::vector<const Symbol*>& DeferredCaptures(
        const core::DeferStatement& statement) const;
    [[nodiscard]] SemanticType TypeOf(const core::Expression& expression) const;
    [[nodiscard]] const Scope& GlobalScope() const { return *_scopes.front(); }
    [[nodiscard]] const std::vector<std::unique_ptr<Symbol>>& Symbols() const {
        return _symbols;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<Scope>>& Scopes() const {
        return _scopes;
    }

   private:
    friend class SemanticAnalyzer;
    std::vector<std::unique_ptr<Symbol>> _symbols;
    std::vector<std::unique_ptr<Scope>> _scopes;
    std::unordered_map<const core::NameExpression*, const Symbol*> _bindings;
    std::unordered_map<const core::AstNode*, const Symbol*> _declarations;
    std::unordered_map<const core::Expression*, SemanticType> _types;
    std::unordered_map<const core::DeferStatement*, std::vector<const Symbol*>>
        _deferredCaptures;
};

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SEMANTIC_MODEL_HPP
