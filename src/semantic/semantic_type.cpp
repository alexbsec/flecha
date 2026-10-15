#include "semantic/semantic_type.hpp"

#include <cassert>
#include <utility>

namespace flecha::semantic {

SemanticType::SemanticType(SemanticTypeKind kind, SemanticType target,
                           bool isConst, std::uint64_t length)
    : _kind(kind),
      _isConst(isConst),
      _length(length),
      _target(std::make_shared<const SemanticType>(std::move(target))) {
    assert(IsCompound() && kind != SemanticTypeKind::Tuple);
}

SemanticType SemanticType::WithoutConst() const {
    SemanticType result = *this;
    result._isConst = false;
    return result;
}

SemanticType SemanticType::WithConst() const {
    SemanticType result = *this;
    result._isConst = true;
    return result;
}

bool SemanticType::OwnsStorage() const {
    if (_kind == SemanticTypeKind::String) return true;
    if (_kind == SemanticTypeKind::Tuple)
        for (const auto& element : Elements())
            if (element.OwnsStorage()) return true;
    return false;
}

bool SemanticType::operator==(const SemanticType& other) const {
    return _kind == other._kind && _isConst == other._isConst &&
           _family == other._family && _length == other._length &&
           ((_elements == nullptr && other._elements == nullptr) ||
            (_elements != nullptr && other._elements != nullptr &&
             *_elements == *other._elements)) &&
           ((_target == nullptr && other._target == nullptr) ||
            (_target != nullptr && other._target != nullptr &&
             *_target == *other._target));
}

SemanticType SemanticType::FromSyntax(const core::TypeNode& type) {
    if (type.TypeForm() == core::TypeKind::Named) return Error();
    if (type.TypeForm() == core::TypeKind::Tuple) {
        std::vector<SemanticType> elements;
        for (const auto& element : type.Elements())
            elements.push_back(FromSyntax(*element));
        return SemanticType(std::move(elements), type.IsConst());
    }
    if (type.TypeForm() == core::TypeKind::Slice)
        return SemanticType(SemanticTypeKind::Slice, FromSyntax(*type.Target()),
                            type.IsConst());
    if (type.TypeForm() == core::TypeKind::Array)
        return SemanticType(SemanticTypeKind::Array, FromSyntax(*type.Target()),
                            type.IsConst(), type.Length());
    if (type.TypeForm() != core::TypeKind::Primitive) {
        return SemanticType(type.TypeForm() == core::TypeKind::Pointer
                                ? SemanticTypeKind::Pointer
                                : SemanticTypeKind::Reference,
                            FromSyntax(*type.Target()), type.IsConst());
    }
    SemanticType result = FromSyntax(type.Type());
    result._isConst = type.IsConst();
    return result;
}

std::string SemanticType::Name() const {
    if (_isConst) return "const " + WithoutConst().Name();
    switch (_kind) {
        case SemanticTypeKind::ErrorValue:
            return "error";
        case SemanticTypeKind::NamedError:
            return _family->name;
        case SemanticTypeKind::I8:
            return "i8";
        case SemanticTypeKind::I16:
            return "i16";
        case SemanticTypeKind::I32:
            return "i32";
        case SemanticTypeKind::I64:
            return "i64";
        case SemanticTypeKind::ISize:
            return "isize";
        case SemanticTypeKind::U8:
            return "u8";
        case SemanticTypeKind::U16:
            return "u16";
        case SemanticTypeKind::U32:
            return "u32";
        case SemanticTypeKind::U64:
            return "u64";
        case SemanticTypeKind::USize:
            return "usize";
        case SemanticTypeKind::F32:
            return "f32";
        case SemanticTypeKind::F64:
            return "f64";
        case SemanticTypeKind::Bool:
            return "bool";
        case SemanticTypeKind::Char:
            return "char";
        case SemanticTypeKind::Void:
            return "void";
        case SemanticTypeKind::String:
            return "string";
        case SemanticTypeKind::StringView:
            return "stringv";
        case SemanticTypeKind::Tuple: {
            std::string name = "(";
            for (const auto& element : Elements()) {
                if (name.size() > 1) name += ", ";
                name += element.Name();
            }
            return name + ")";
        }
        case SemanticTypeKind::Slice:
            return "[]" + _target->Name();
        case SemanticTypeKind::Array:
            return "[" + std::to_string(_length) + "]" + _target->Name();
        case SemanticTypeKind::Pointer:
            return "*" + _target->Name();
        case SemanticTypeKind::Reference: {
            const std::string targetName = _target->Name();
            // Keep nested references distinct from the logical && token.
            return (targetName.starts_with('&') ? "& " : "&") + targetName;
        }
        case SemanticTypeKind::Null:
            return "null";
        case SemanticTypeKind::None:
            return "none";
        case SemanticTypeKind::Function:
            return "function";
        case SemanticTypeKind::Error:
            return "<error>";
    }
    return "<error>";
}

bool SemanticType::IsInteger() const {
    return _kind >= SemanticTypeKind::I8 && _kind <= SemanticTypeKind::USize;
}

bool SemanticType::IsFloating() const {
    return _kind == SemanticTypeKind::F32 || _kind == SemanticTypeKind::F64;
}

bool SemanticType::IsNumeric() const { return IsInteger() || IsFloating(); }

bool SemanticType::IsPrintable() const {
    return IsNumeric() || _kind == SemanticTypeKind::Bool ||
           _kind == SemanticTypeKind::Char ||
           _kind == SemanticTypeKind::String ||
           _kind == SemanticTypeKind::StringView;
}

SemanticType SemanticType::FromSyntax(core::PrimitiveType type) {
    switch (type) {
        case core::PrimitiveType::ErrorValue:
            return SemanticType(SemanticTypeKind::ErrorValue);
        case core::PrimitiveType::I8:
            return SemanticType(SemanticTypeKind::I8);
        case core::PrimitiveType::I16:
            return SemanticType(SemanticTypeKind::I16);
        case core::PrimitiveType::I32:
            return SemanticType(SemanticTypeKind::I32);
        case core::PrimitiveType::I64:
            return SemanticType(SemanticTypeKind::I64);
        case core::PrimitiveType::ISize:
            return SemanticType(SemanticTypeKind::ISize);
        case core::PrimitiveType::U8:
            return SemanticType(SemanticTypeKind::U8);
        case core::PrimitiveType::U16:
            return SemanticType(SemanticTypeKind::U16);
        case core::PrimitiveType::U32:
            return SemanticType(SemanticTypeKind::U32);
        case core::PrimitiveType::U64:
            return SemanticType(SemanticTypeKind::U64);
        case core::PrimitiveType::USize:
            return SemanticType(SemanticTypeKind::USize);
        case core::PrimitiveType::F32:
            return SemanticType(SemanticTypeKind::F32);
        case core::PrimitiveType::F64:
            return SemanticType(SemanticTypeKind::F64);
        case core::PrimitiveType::Bool:
            return SemanticType(SemanticTypeKind::Bool);
        case core::PrimitiveType::Char:
            return SemanticType(SemanticTypeKind::Char);
        case core::PrimitiveType::Void:
            return SemanticType(SemanticTypeKind::Void);
        case core::PrimitiveType::String:
            return SemanticType(SemanticTypeKind::String);
        case core::PrimitiveType::StringView:
            return SemanticType(SemanticTypeKind::StringView);
    }
    return Error();
}

bool IsAssignable(SemanticType destination, SemanticType source) {
    if (destination.IsError() || source.IsError()) return true;
    if (destination.WithoutConst() == source.WithoutConst()) return true;
    if (destination.Kind() == SemanticTypeKind::ErrorValue &&
        source.Kind() == SemanticTypeKind::NamedError)
        return true;
    if (destination.Kind() == SemanticTypeKind::Tuple &&
        source.Kind() == SemanticTypeKind::Tuple) {
        if (destination.Elements().size() != source.Elements().size())
            return false;
        for (std::size_t i = 0; i < destination.Elements().size(); ++i)
            if (!IsAssignable(destination.Elements()[i], source.Elements()[i]))
                return false;
        return true;
    }
    if (destination.Kind() == SemanticTypeKind::Pointer) {
        if (source.Kind() == SemanticTypeKind::Null) return true;
        if (source.Kind() == SemanticTypeKind::Pointer) {
            // Only add const at the immediate target; no nested covariance.
            return destination.Target()->WithoutConst() ==
                       source.Target()->WithoutConst() &&
                   (destination.Target()->IsConst() ||
                    !source.Target()->IsConst());
        }
    }
    if (destination.Kind() == SemanticTypeKind::String &&
        source.Kind() == SemanticTypeKind::StringView) {
        return true;
    }
    return source.Kind() == SemanticTypeKind::None &&
           destination.Kind() != SemanticTypeKind::Void &&
           destination.Kind() != SemanticTypeKind::Function &&
           !destination.IsBorrowed();
}

bool CanReborrow(SemanticType destination, SemanticType source) {
    return destination.IsBorrowed() && source.Kind() == destination.Kind() &&
           destination.Target()->WithoutConst() ==
               source.Target()->WithoutConst() &&
           (destination.Target()->IsConst() || !source.Target()->IsConst());
}

bool CanExplicitlyCast(SemanticType source, SemanticType destination) {
    if (source.IsError() || destination.IsError()) return true;
    if (source.IsCompound() || destination.IsCompound()) return false;
    if (source.WithoutConst() == destination.WithoutConst()) return true;
    if ((source.Kind() == SemanticTypeKind::StringView &&
         destination.Kind() == SemanticTypeKind::String) ||
        (source.Kind() == SemanticTypeKind::String &&
         destination.Kind() == SemanticTypeKind::StringView) ||
        (destination.Kind() == SemanticTypeKind::String &&
         (source.IsNumeric() || source.Kind() == SemanticTypeKind::Bool ||
          source.Kind() == SemanticTypeKind::Char))) {
        return true;
    }
    return source.IsNumeric() && destination.IsNumeric();
}

}  // namespace flecha::semantic
