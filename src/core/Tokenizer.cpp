#include "core/Tokenizer.hpp"
#include <unordered_map>
#include <stdexcept>

template <typename ...Args> using umap = std::unordered_map<Args...>;

namespace flecha {
namespace core {

    // Keywords mapping
    static const umap<string, TokenType> KEYWORDS = {
        {"int", TokenType::Int},
        {"char", TokenType::Char},
        {"bool", TokenType::Bool},
        {"float", TokenType::Float},
        {"string", TokenType::String},
        {"void", TokenType::Void},
        {"strict", TokenType::Strict},
        {"method", TokenType::Method},
        {"class", TokenType::Class},
        {"construct", TokenType::Construct},
        {"destruct", TokenType::Destruct},
        {"return", TokenType::Return},
        {"allot", TokenType::Allot},
        {"dellot", TokenType::Dellot},
    };

    /* PRIVATE METHODS */

    // Gets the current character
    char Tokenizer::_GetCurrentChar() const {
        return _source[_index]; 
    }

    // Advance to next character
    void Tokenizer::_Advance() {
        if (_GetCurrentChar() == '\n') {
            // It's a new-line, so we increment line
            // and set column to initial position
            _line++;
            _column = 0;
        } else {
            // We are still in the same line, so we 
            // go to next column
            _column++;
        }

        // Implement the index to keep track of the position
        _index++;
    }

    // Checks whether we reached end of source
    bool Tokenizer::_IsAtEnd() const {
        return _index >= _source.size();
    }

    // Skips whitespace
    void Tokenizer::_SkipWhiteSpace() {
        while (!_IsAtEnd() && std::isspace(_GetCurrentChar())) {
            // If we're not at end and current char is whitespace, we advance
            _Advance();
        }
    }

    /* PUBLIC METHODS */

    Tokenizer::Tokenizer(const string& src)
        : _source(src), _index(0), _line(1), _column(1) {}

    // Gets next token
    Token Tokenizer::NextToken() {
        _SkipWhiteSpace();

        if (_IsAtEnd())
            return Token(TokenType::EOF_TOKEN, "", _line, _column);

        char curr_ch = _GetCurrentChar();
        Token token;

        _Advance();
        token.line = _line;
        token.column = _column;

        // Match chars
        switch (curr_ch) {
            case '"': {
                size_t start = _index; // Position after the opening "
                string value;

                while (!_IsAtEnd() && _GetCurrentChar() != '"') {
                    if (_GetCurrentChar() == '\\') {
                        _Advance(); // Skip the backslash
                        if (_IsAtEnd()) break; // Avoid out-of-bounds errors
                        // Handle escape sequences
                        char escaped = _GetCurrentChar();
                        switch (escaped) {
                            case 'n': value += '\n'; break;
                            case 't': value += '\t'; break;
                            case '\\': value += '\\'; break;
                            case '"': value += '"'; break;
                            default: value += escaped; break; // Unknown escape, keep as is
                        }
                    } else {
                        value += _GetCurrentChar();
                    }
                    _Advance();
                }

                if (_IsAtEnd() || _GetCurrentChar() != '"') {
                    throw std::runtime_error("Unterminated string literal at line " + std::to_string(_line));
                }

                _Advance(); // Skip closing "
                token.type = TokenType::StringLiteral;
                token.value = value;
                break;
            }
            case '\'': {
                if (_IsAtEnd()) {
                    throw std::runtime_error("Unterminated character literal at line " + std::to_string(_line));
                }

                char val;
                if (_GetCurrentChar() == '\\') {
                    _Advance(); // Skip backslash
                    if (_IsAtEnd()) break;
                    switch (_GetCurrentChar()) {
                        case 'n': val = '\n'; break;
                        case 't': val = '\t'; break;
                        case '\\': val = '\\'; break;
                        case '\'': val = '\''; break;
                        default:
                            throw std::runtime_error("Invalid escape sequence in character literal at line " + std::to_string(_line));
                    }
                } else {
                    val = _GetCurrentChar();
                }
                _Advance(); // Consume the character

                if (_IsAtEnd() || _GetCurrentChar() != '\'') {
                    throw std::runtime_error("Unterminated character literal at line " + std::to_string(_line));
                }
                _Advance(); // Skip closing '
                token.type = TokenType::CharLiteral;
                token.value = string(1, val); // Store as a single-character string
                break;
            }
            case ';':
                token.type = TokenType::SemiColon;
                token.value = ";";
                break;
            case '(':
                token.type = TokenType::LParen;
                token.value = "(";
                break;
            case ')':
                token.type = TokenType::RParen;
                token.value = ")";
                break;
            case '+':
                token.type = TokenType::Add;
                token.value = "+";
                break;
            case '-':
                // Can be subtraction or assign val of pointer
                // must check next character
                if (_index + 1 < _source.size() && _source[_index] == '>') {
                    // Found assignval operator
                    _Advance();
                    token.type = TokenType::AssignVal;
                    token.value = "->";
                } else {
                    token.type = TokenType::Sub;
                    token.value = "-";
                }
                break;
            case '*':
                // Can be multiplication or power, must check
                // next character
                if (_index + 1 < _source.size() && _source[_index] ==  '*') {
                    // Found "**"
                    _Advance();
                    token.type = TokenType::Pow;
                    token.value = "**";
                } else {
                    token.type = TokenType::Mul;
                    token.value = "*";
                }
                break;
            case '/':
                token.type = TokenType::Div;
                token.value = "/";
                break;
            case '^':
                token.type = TokenType::Xor;
                token.value = "^";
                break;
            case '%':
                token.type = TokenType::Mod;
                token.value = "%";
                break;
            case '=':
                // Can be equal or compare token, must check
                // next character
                if (_index + 1 <= _source.size() && _source[_index] == '=') {
                    // Found compare "=="
                    _Advance();
                    token.type = TokenType::Compare;
                    token.value = "==";
                } else {
                    // Equal found "="
                    token.type = TokenType::Equal;
                    token.value = "=";
                }
                break;
            case '<':
                // Can be less than or less than or equal, must
                // check next character
                if (_index + 1 <= _source.size() && _source[_index] == '=') {
                    // Found less than or equal token
                    _Advance();
                    token.type = TokenType::LessEqual;
                    token.value = "<=";
                } else {
                    // Found less than
                    token.type = TokenType::Less;
                    token.value = "<";
                }
                break;
            case '>': 
                // Can be greater than or greater than or equal, must
                // check next character
                if (_index + 1 <= _source.size() && _source[_index] == '=') {
                    // Found greater than or equal
                    _Advance();
                    token.type = TokenType::GreaterEqual;
                    token.value = ">=";
                } else {
                    // Found greater than
                    token.type = TokenType::Greater;
                    token.value = ">";
                }
                break;
            case '&':
                // Must be and operator, check next char
                if (_index + 1 <= _source.size() && _source[_index] == '&') {
                    // Found and operator
                    _Advance();
                    token.type = TokenType::And;
                    token.value = "&&";
                } else {
                    // Handle
                }
                break;
            case '|':
                // Can be not equal, Or operator, or not operator, must check next
                // character
                if (_index + 1 <= _source.size() && _source[_index] == '=') {
                    // Found not equal op
                    _Advance();
                    token.type = TokenType::NotEqual;
                    token.value = "|=";
                } else if (_index + 1 <= _source.size() && _source[_index] == '|') {
                    // Found Or operator
                    _Advance(); 
                    token.type = TokenType::Or;
                    token.value = "||";
                } else {
                    // Found not op
                    token.type = TokenType::Not;
                    token.value = "|";
                }
                break;
            case '[':
                token.type = TokenType::LBracket;
                token.value = "[";
                break;
            case ']':
                token.type = TokenType::RBracket;
                token.value = "]";
                break;
            case '{':
                token.type = TokenType::LCurly;
                token.value = "{";
                break;
            case '}':
                token.type = TokenType::RCurly;
                token.value = "}";
                break;
            case '!':
                token.type = TokenType::Bang;
                token.value = "!";
                break;
            case '?':
                token.type = TokenType::AddressRef;
                token.value = "?";
                break;
        }

        if (token.type != TokenType::NoToken) {
            return token;
        }

        // Match numbers:
        if (std::isdigit(curr_ch)) {
            size_t start = _index - 1;
            bool has_decimal_point = false;

            while (!_IsAtEnd() && (std::isdigit(_GetCurrentChar()) || _GetCurrentChar() == '.')) {
                if (_GetCurrentChar() == '.') {
                    if (has_decimal_point) break; // Only one decimal point allowed
                    has_decimal_point = true;
                }
                _Advance();
            }
            token.line = _line;
            token.column = _column;
            token.type = has_decimal_point ? TokenType::FloatLiteral : TokenType::NumberLiteral;
            token.value = _source.substr(start, _index - start);
            return token;
        }

        // Match identifier and keywords
         if (std::isalpha(curr_ch) || curr_ch == '_') {
            size_t start = _index - 1; // Include the current character
            while (!_IsAtEnd() && (std::isalnum(_GetCurrentChar()) || _GetCurrentChar() == '_')) {
                _Advance();
            }
            string word = _source.substr(start, _index - start);
            auto it = KEYWORDS.find(word);
            token.line = _line;
            token.column = _column - (word.length() - 1);
            token.value = word;
            if (it != KEYWORDS.end()) {
                token.type = it->second;
                return token;
            }
            token.type = TokenType::Identifier;
            return token;
        }
               
        // Unkown token
        _Advance();
        token.value = string(1, curr_ch);
        token.line = _line;
        token.column = _column;
        return token;
    }

    vector<Token> Tokenize() {
        return vector<Token>();
    }
}
}
