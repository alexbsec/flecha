#ifndef FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP
#define FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP

#include <string_view>

#include "core/ast.hpp"

namespace flecha::semantic {

enum class SemanticTypeKind {
    I8,
    I16,
    I32,
    I64,
    ISize,
    U8,
    U16,
    U32,
    U64,
    USize,
    F32,
    F64,
    Bool,
    Char,
    Void,
    StringView,
    Null,
    None,
    Function,
    Error,
};

class SemanticType final {
   public:
    constexpr explicit SemanticType(SemanticTypeKind kind) : _kind(kind) {}

    [[nodiscard]] constexpr SemanticTypeKind Kind() const { return _kind; }
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] bool IsInteger() const;
    [[nodiscard]] bool IsFloating() const;
    [[nodiscard]] bool IsNumeric() const;
    [[nodiscard]] bool IsError() const {
        return _kind == SemanticTypeKind::Error;
    }

    bool operator==(const SemanticType&) const = default;

    [[nodiscard]] static SemanticType FromSyntax(core::PrimitiveType type);
    [[nodiscard]] static constexpr SemanticType Error() {
        return SemanticType(SemanticTypeKind::Error);
    }

   private:
    SemanticTypeKind _kind;
};

[[nodiscard]] bool IsAssignable(SemanticType destination, SemanticType source);
[[nodiscard]] bool CanExplicitlyCast(SemanticType source,
                                     SemanticType destination);

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP
