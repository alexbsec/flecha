#ifndef FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP
#define FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP

#include <cassert>
#include <memory>
#include <string>

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
    String,
    StringView,
    Pointer,
    Reference,
    Array,
    Null,
    None,
    Function,
    Error,
};

class SemanticType final {
   public:
    explicit SemanticType(SemanticTypeKind kind, bool isConst = false)
        : _kind(kind), _isConst(isConst) {
        assert(!IsCompound());
    }
    SemanticType(SemanticTypeKind kind, SemanticType target,
                 bool isConst = false, std::uint64_t length = 0);

    [[nodiscard]] SemanticTypeKind Kind() const { return _kind; }
    [[nodiscard]] std::string Name() const;
    [[nodiscard]] bool IsInteger() const;
    [[nodiscard]] bool IsFloating() const;
    [[nodiscard]] bool IsNumeric() const;
    [[nodiscard]] bool IsPrintable() const;
    [[nodiscard]] bool IsError() const {
        return _kind == SemanticTypeKind::Error;
    }

    [[nodiscard]] bool IsConst() const { return _isConst; }
    [[nodiscard]] bool IsCompound() const {
        return _kind == SemanticTypeKind::Pointer ||
               _kind == SemanticTypeKind::Reference ||
               _kind == SemanticTypeKind::Array;
    }
    [[nodiscard]] std::uint64_t Length() const { return _length; }
    [[nodiscard]] const SemanticType* Target() const { return _target.get(); }
    [[nodiscard]] SemanticType WithoutConst() const;
    [[nodiscard]] SemanticType WithConst() const;
    bool operator==(const SemanticType& other) const;

    [[nodiscard]] static SemanticType FromSyntax(const core::TypeNode& type);

    [[nodiscard]] static SemanticType FromSyntax(core::PrimitiveType type);
    [[nodiscard]] static SemanticType Error() {
        return SemanticType(SemanticTypeKind::Error);
    }

   private:
    SemanticTypeKind _kind;
    bool _isConst;
    std::uint64_t _length = 0;
    std::shared_ptr<const SemanticType> _target;
};

[[nodiscard]] bool IsAssignable(SemanticType destination, SemanticType source);
[[nodiscard]] bool CanReborrow(SemanticType destination, SemanticType source);
[[nodiscard]] bool CanExplicitlyCast(SemanticType source,
                                     SemanticType destination);

}  // namespace flecha::semantic

#endif  // FLECHA_SEMANTIC_SEMANTIC_TYPE_HPP
