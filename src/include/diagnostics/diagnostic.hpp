#ifndef FLECHA_DIAGNOSTICS_DIAGNOSTIC_HPP
#define FLECHA_DIAGNOSTICS_DIAGNOSTIC_HPP

#include <string>
#include <string_view>

#include "source/source_range.hpp"

namespace flecha::diagnostics {

enum class DiagnosticSeverity { Note, Warning, Error };

enum class DiagnosticCode {
    UnexpectedCharacter,
    UnterminatedBlockComment,
    UnterminatedStringLiteral,
    InvalidStringEscape,
    InvalidCharacterLiteral,
    InvalidCharacterEscape,
    UnexpectedToken,
    ExpectedToken,
    InvalidAssignmentTarget,
};

[[nodiscard]] std::string_view DiagnosticCodeName(DiagnosticCode code);

struct Diagnostic {
    DiagnosticCode code;
    DiagnosticSeverity severity;
    std::string message;
    source::SourceRange range;
};

}  // namespace flecha::diagnostics

#endif  // FLECHA_DIAGNOSTICS_DIAGNOSTIC_HPP
