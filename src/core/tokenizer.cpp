#include "core/tokenizer.hpp"

#include <cctype>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace flecha::core {
namespace {

using KeywordMap = std::unordered_map<std::string_view, TokenType>;

const KeywordMap KEYWORDS = {
    {"i8", TokenType::I8},           {"i16", TokenType::I16},
    {"i32", TokenType::I32},         {"i64", TokenType::I64},
    {"isize", TokenType::ISize},     {"u8", TokenType::U8},
    {"u16", TokenType::U16},         {"u32", TokenType::U32},
    {"u64", TokenType::U64},         {"usize", TokenType::USize},
    {"f32", TokenType::F32},         {"f64", TokenType::F64},
    {"bool", TokenType::Bool},       {"char", TokenType::Char},
    {"void", TokenType::Void},       {"unsafe", TokenType::Unsafe},
    {"export", TokenType::Export},   {"extern", TokenType::Extern},
    {"inline", TokenType::Inline},   {"comptime", TokenType::Comptime},
    {"return", TokenType::Return},   {"yield", TokenType::Yield},
    {"if", TokenType::If},           {"else", TokenType::Else},
    {"error", TokenType::Error},     {"const", TokenType::Const},
    {"volatile", TokenType::Volatile}, {"as", TokenType::As},
    {"none", TokenType::None},       {"null", TokenType::Null},
    {"true", TokenType::True},       {"false", TokenType::False},
};

bool IsIdentifierStart(char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalpha(value) != 0 || character == '_';
}

bool IsIdentifierContinue(char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '_';
}

}  // namespace

char Tokenizer::_Peek(size_t offset) const {
    const size_t position = _index + offset;
    return position < _source.size() ? _source[position] : '\0';
}

char Tokenizer::_Advance() {
    const char character = _Peek();
    if (_IsAtEnd()) {
        return '\0';
    }

    ++_index;
    if (character == '\n') {
        ++_line;
        _column = 1;
    } else {
        ++_column;
    }
    return character;
}

bool Tokenizer::_Match(char expected) {
    if (_Peek() != expected) {
        return false;
    }
    _Advance();
    return true;
}

bool Tokenizer::_IsAtEnd() const { return _index >= _source.size(); }

void Tokenizer::_SkipTrivia() {
    while (!_IsAtEnd()) {
        const auto current = static_cast<unsigned char>(_Peek());
        if (std::isspace(current) != 0) {
            _Advance();
            continue;
        }

        if (_Peek() == '/' && _Peek(1) == '/') {
            while (!_IsAtEnd() && _Peek() != '\n') {
                _Advance();
            }
            continue;
        }

        if (_Peek() == '/' && _Peek(1) == '*') {
            const int start_line = _line;
            _Advance();
            _Advance();
            while (!_IsAtEnd() && !(_Peek() == '*' && _Peek(1) == '/')) {
                _Advance();
            }
            if (_IsAtEnd()) {
                throw std::runtime_error(
                    "Unterminated block comment at line " +
                    std::to_string(start_line));
            }
            _Advance();
            _Advance();
            continue;
        }

        break;
    }
}

Token Tokenizer::_NextToken() {
    _SkipTrivia();
    if (_IsAtEnd()) {
        return Token(TokenType::EOF_TOKEN, "", _line, _column);
    }

    const int start_line = _line;
    const int start_column = _column;
    const size_t start_index = _index;
    const char character = _Advance();

    const auto make_token = [&](TokenType type) {
        return Token(type, _source.substr(start_index, _index - start_index),
                     start_line, start_column);
    };

    switch (character) {
        case ';': return make_token(TokenType::SemiColon);
        case ',': return make_token(TokenType::Comma);
        case '.': return make_token(TokenType::Dot);
        case '(': return make_token(TokenType::LParen);
        case ')': return make_token(TokenType::RParen);
        case '[': return make_token(TokenType::LBracket);
        case ']': return make_token(TokenType::RBracket);
        case '{': return make_token(TokenType::LCurly);
        case '}': return make_token(TokenType::RCurly);
        case '+': return make_token(TokenType::Add);
        case '-': return make_token(TokenType::Sub);
        case '*': return make_token(TokenType::Mul);
        case '/': return make_token(TokenType::Div);
        case '%': return make_token(TokenType::Mod);
        case '^': return make_token(TokenType::Xor);
        case ':':
            return make_token(_Match('=') ? TokenType::DeclareAssign
                                          : TokenType::Colon);
        case '=':
            if (_Match('=')) {
                return make_token(_Match('=') ? TokenType::StrictEqual
                                              : TokenType::Equal);
            }
            return make_token(TokenType::Assign);
        case '!':
            if (_Match('=')) {
                return make_token(_Match('=') ? TokenType::StrictNotEqual
                                              : TokenType::NotEqual);
            }
            return make_token(TokenType::Bang);
        case '<':
            return make_token(_Match('=') ? TokenType::LessEqual
                                          : TokenType::Less);
        case '>':
            return make_token(_Match('=') ? TokenType::GreaterEqual
                                          : TokenType::Greater);
        case '&':
            if (_Match('&')) return make_token(TokenType::And);
            break;
        case '|':
            if (_Match('|')) return make_token(TokenType::Or);
            break;
        case '"': {
            string value;
            while (!_IsAtEnd() && _Peek() != '"') {
                if (_Peek() == '\n') {
                    throw std::runtime_error(
                        "Unterminated string literal at line " +
                        std::to_string(start_line));
                }
                if (_Peek() != '\\') {
                    value += _Advance();
                    continue;
                }

                _Advance();
                if (_IsAtEnd()) break;
                switch (_Advance()) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    default:
                        throw std::runtime_error(
                            "Invalid escape sequence in string literal at line " +
                            std::to_string(start_line));
                }
            }
            if (!_Match('"')) {
                throw std::runtime_error(
                    "Unterminated string literal at line " +
                    std::to_string(start_line));
            }
            return Token(TokenType::StringLiteral, value, start_line,
                         start_column);
        }
        case '\'': {
            if (_IsAtEnd() || _Peek() == '\'' || _Peek() == '\n') {
                throw std::runtime_error(
                    "Invalid character literal at line " +
                    std::to_string(start_line));
            }

            char value = _Advance();
            if (value == '\\') {
                if (_IsAtEnd()) {
                    throw std::runtime_error(
                        "Unterminated character literal at line " +
                        std::to_string(start_line));
                }
                switch (_Advance()) {
                    case 'n': value = '\n'; break;
                    case 't': value = '\t'; break;
                    case 'r': value = '\r'; break;
                    case '0': value = '\0'; break;
                    case '\\': value = '\\'; break;
                    case '\'': value = '\''; break;
                    default:
                        throw std::runtime_error(
                            "Invalid escape sequence in character literal at line " +
                            std::to_string(start_line));
                }
            }
            if (!_Match('\'')) {
                throw std::runtime_error(
                    "Character literal must contain exactly one character at line " +
                    std::to_string(start_line));
            }
            return Token(TokenType::CharLiteral, string(1, value), start_line,
                         start_column);
        }
        default: break;
    }

    const auto unsigned_character = static_cast<unsigned char>(character);
    if (std::isdigit(unsigned_character) != 0) {
        while (std::isdigit(static_cast<unsigned char>(_Peek())) != 0) {
            _Advance();
        }

        TokenType type = TokenType::NumberLiteral;
        if (_Peek() == '.' &&
            std::isdigit(static_cast<unsigned char>(_Peek(1))) != 0) {
            type = TokenType::FloatLiteral;
            _Advance();
            while (std::isdigit(static_cast<unsigned char>(_Peek())) != 0) {
                _Advance();
            }
        }
        return make_token(type);
    }

    if (IsIdentifierStart(character)) {
        while (IsIdentifierContinue(_Peek())) {
            _Advance();
        }
        const string word = _source.substr(start_index, _index - start_index);
        const auto keyword = KEYWORDS.find(word);
        return Token(keyword == KEYWORDS.end() ? TokenType::Identifier
                                               : keyword->second,
                     word, start_line, start_column);
    }

    throw std::runtime_error("Unexpected character '" + string(1, character) +
                             "' at line " + std::to_string(start_line) +
                             ", column " + std::to_string(start_column));
}

Tokenizer::Tokenizer(const string& source)
    : _source(source), _index(0), _line(1), _column(1) {}

vector<Token> Tokenizer::Tokenize() {
    vector<Token> tokens;
    while (tokens.empty() || tokens.back().type != TokenType::EOF_TOKEN) {
        tokens.push_back(_NextToken());
    }
    return tokens;
}

}  // namespace flecha::core
