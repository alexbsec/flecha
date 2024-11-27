#include "core/Parser.hpp"

#include <stdexcept>

namespace flecha {
namespace core {
/**
 * @brief Variable token types
 */
static const uset<TokenType> TYPES = {
    TokenType::Int,   TokenType::String, TokenType::Char,
    TokenType::Float, TokenType::Bool,
};

/* PRIVATE METHODS  */

/**
 * @brief Advances and gets next token
 *
 * @return - The current token at _current_token private
 * variable
 */
Token Parser::_Advance() {
    if (_index < _tokens.size() - 1) {
        // More tokens to retrieve
        _index++;
        _current_token = _tokens[_index];
    }

    return _current_token;
}

/**
 * @brief Matches the token to a token type and advances
 *
 * @param type - The expected token type
 *
 * @return - True if match the type / false otherwise
 */
bool Parser::_Match(TokenType type) {
    if (_Check(type)) {
        // They match, so we advance
        _Advance();
        return true;
    }

    return false;
}

/**
 * @brief Checks whether _current_token matches passed token type
 *
 * @param type - The expected token type
 *
 * @return - True if they match and _tokens is not empty / false otherwise
 */
bool Parser::_Check(TokenType type) {
    return !_tokens.empty() && _current_token.type == type;
}

/**
 * @brief Consumes current token by calling _Match and advances to next
 *
 * @param type - The expected token type
 *
 * @param err - The error message
 */
void Parser::_Consume(TokenType type, const string& err) {
    if (!_Match(type)) {
        throw std::runtime_error(
            "Parser Error: " + err + " Found: " + _current_token.value +
            " at line " + std::to_string(_current_token.line) + ", column " +
            std::to_string(_current_token.column));
    }
}

/**
 * @brief Parse expressions like int! var
 *
 * @return
 */
std::unique_ptr<ASTNode> Parser::_ParseExpression() {
    if (_Check(TokenType::NumberLiteral) || _Check(TokenType::StringLiteral) ||
        _Check(TokenType::CharLiteral)) {
        // Gets next token
        Token token = _Advance();
        return std::make_unique<LiteralNode>(token.value);
    } else if (_Check(TokenType::Identifier)) {
        // If its an identifier (variable)
        Token token = _Advance();
        return std::make_unique<VariableNode>(token.value);
    }

    throw std::runtime_error("Parser Error: Expected expression at line " +
                             std::to_string(_current_token.line));
}

}  // namespace core
}  // namespace flecha
