#include "semantic/semantic_type.hpp"

namespace flecha::semantic {

std::string_view SemanticType::Name() const {
    switch (_kind) {
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
        case SemanticTypeKind::StringView:
            return "stringv";
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
           _kind == SemanticTypeKind::StringView;
}

SemanticType SemanticType::FromSyntax(core::PrimitiveType type) {
    switch (type) {
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
        case core::PrimitiveType::StringView:
            return SemanticType(SemanticTypeKind::StringView);
    }
    return Error();
}

bool IsAssignable(SemanticType destination, SemanticType source) {
    if (destination.IsError() || source.IsError()) return true;
    if (destination == source) return true;
    return source.Kind() == SemanticTypeKind::None &&
           destination.Kind() != SemanticTypeKind::Void &&
           destination.Kind() != SemanticTypeKind::Function;
}

bool CanExplicitlyCast(SemanticType source, SemanticType destination) {
    if (source.IsError() || destination.IsError()) return true;
    return source.IsNumeric() && destination.IsNumeric();
}

}  // namespace flecha::semantic
