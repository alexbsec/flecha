#include "diagnostics/diagnostic.hpp"

namespace flecha::diagnostics {

std::string_view DiagnosticCodeName(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::UnexpectedCharacter:
            return "FLECHA-L0001";
        case DiagnosticCode::UnterminatedBlockComment:
            return "FLECHA-L0002";
        case DiagnosticCode::UnterminatedStringLiteral:
            return "FLECHA-L0003";
        case DiagnosticCode::InvalidStringEscape:
            return "FLECHA-L0004";
        case DiagnosticCode::InvalidCharacterLiteral:
            return "FLECHA-L0005";
        case DiagnosticCode::InvalidCharacterEscape:
            return "FLECHA-L0006";
        case DiagnosticCode::UnexpectedToken:
            return "FLECHA-P0001";
        case DiagnosticCode::ExpectedToken:
            return "FLECHA-P0002";
        case DiagnosticCode::InvalidAssignmentTarget:
            return "FLECHA-P0003";
        case DiagnosticCode::DuplicateDeclaration:
            return "FLECHA-S0001";
        case DiagnosticCode::UnknownName:
            return "FLECHA-S0002";
        case DiagnosticCode::IncompatibleType:
            return "FLECHA-S0003";
        case DiagnosticCode::SemanticInvalidAssignmentTarget:
            return "FLECHA-S0004";
        case DiagnosticCode::SymbolNotWritable:
            return "FLECHA-S0005";
        case DiagnosticCode::NotCallable:
            return "FLECHA-S0006";
        case DiagnosticCode::WrongArgumentCount:
            return "FLECHA-S0007";
        case DiagnosticCode::IncompatibleArgument:
            return "FLECHA-S0008";
        case DiagnosticCode::InvalidOperand:
            return "FLECHA-S0009";
        case DiagnosticCode::InvalidCondition:
            return "FLECHA-S0010";
        case DiagnosticCode::InvalidReturnType:
            return "FLECHA-S0011";
        case DiagnosticCode::CannotInferType:
            return "FLECHA-S0012";
        case DiagnosticCode::InvalidVariableType:
            return "FLECHA-S0013";
        case DiagnosticCode::LoopControlOutsideLoop:
            return "FLECHA-S0014";
        case DiagnosticCode::DuplicatePosition:
            return "FLECHA-S0015";
        case DiagnosticCode::UnknownPosition:
            return "FLECHA-S0016";
        case DiagnosticCode::InvalidCast:
            return "FLECHA-S0017";
        case DiagnosticCode::InvalidMember:
            return "FLECHA-S0018";
        case DiagnosticCode::NotIndexable:
            return "FLECHA-S0019";
        case DiagnosticCode::BorrowEscapesOwner:
            return "FLECHA-S0020";
        case DiagnosticCode::NativeTargetUnavailable:
            return "FLECHA-C0001";
        case DiagnosticCode::ObjectEmissionFailed:
            return "FLECHA-C0002";
        case DiagnosticCode::RuntimeUnavailable:
            return "FLECHA-C0003";
        case DiagnosticCode::LinkerFailed:
            return "FLECHA-C0004";
    }
    return "FLECHA-UNKNOWN";
}

}  // namespace flecha::diagnostics
