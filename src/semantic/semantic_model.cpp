#include "semantic/semantic_model.hpp"

namespace flecha::semantic {

const Symbol* SemanticModel::SymbolFor(
    const core::NameExpression& expression) const {
    const std::unordered_map<const core::NameExpression*,
                             const Symbol*>::const_iterator found =
        _bindings.find(&expression);
    return found == _bindings.end() ? nullptr : found->second;
}

const Symbol* SemanticModel::DeclaredSymbol(
    const core::AstNode& declaration) const {
    const std::unordered_map<const core::AstNode*,
                             const Symbol*>::const_iterator found =
        _declarations.find(&declaration);
    return found == _declarations.end() ? nullptr : found->second;
}

SemanticType SemanticModel::TypeOf(const core::Expression& expression) const {
    const std::unordered_map<const core::Expression*,
                             SemanticType>::const_iterator found =
        _types.find(&expression);
    return found == _types.end() ? SemanticType::Error() : found->second;
}

}  // namespace flecha::semantic
