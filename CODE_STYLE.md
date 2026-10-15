# Flecha C++ Code Style

## Naming

- Source and header filenames use `snake_case`: `source_range.hpp`.
- Types, classes, structs, and enums use `PascalCase`: `DiagnosticBag`.
- Every function and method uses `PascalCase`, regardless of visibility:
  `Tokenize()`, `Report()`, and `NextToken()`.
- Private data members use a leading underscore followed by `camelCase`:
  `_currentToken`. Do not use a trailing underscore such as `currentToken_`.
- Variables and function parameters use `camelCase`.
- Compile-time constants use `UPPER_SNAKE_CASE`.
- Avoid `auto` unless the inferred type is immediately obvious from the
  initializer. Prefer an explicit type when it improves readability.
- Names required by C++ or an external API retain their mandated spelling,
  such as the program entry point `main` and GoogleTest macros.

## Source ranges

- Byte offsets are zero-based.
- Displayed line and column numbers are one-based.
- Ranges are half-open: `[start, end)`.
