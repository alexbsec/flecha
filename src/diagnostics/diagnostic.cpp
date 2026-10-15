#include "diagnostics/diagnostic.hpp"

namespace flecha::diagnostics {

std::string_view DiagnosticCodeName(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::UnexpectedCharacter: return "FLECHA-L0001";
        case DiagnosticCode::UnterminatedBlockComment: return "FLECHA-L0002";
        case DiagnosticCode::UnterminatedStringLiteral: return "FLECHA-L0003";
        case DiagnosticCode::InvalidStringEscape: return "FLECHA-L0004";
        case DiagnosticCode::InvalidCharacterLiteral: return "FLECHA-L0005";
        case DiagnosticCode::InvalidCharacterEscape: return "FLECHA-L0006";
        case DiagnosticCode::UnexpectedToken: return "FLECHA-P0001";
        case DiagnosticCode::ExpectedToken: return "FLECHA-P0002";
        case DiagnosticCode::InvalidAssignmentTarget: return "FLECHA-P0003";
    }
    return "FLECHA-UNKNOWN";
}

}  // namespace flecha::diagnostics
