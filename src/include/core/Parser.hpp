#ifndef FLECHA_PARSER_HPP
#define FLECHA_PARSER_HPP

#include <memory>
#include <unordered_set>
#include <vector>

#include "AST.hpp"
#include "Token.hpp"

template <typename... Args>
using vector = std::vector<Args...>;

template <typename... Args>
using uset = std::unordered_set<Args...>;

namespace flecha {
namespace core {
class Parser {
   private:
    vector<Token> _tokens;
    size_t _index = 0;
    Token _current_token;

    Token _Advance();
    bool _Match(TokenType type);
    bool _Check(TokenType type);
    void _Consume(TokenType type, const string& err = "");

    // Parsing methods
    std::unique_ptr<ASTNode> _ParseExpression();
    std::unique_ptr<ASTNode> _ParseExpressionStatement();

   public:
    explicit Parser(const vector<Token>& tokens);
    std::unique_ptr<ASTNode> Parse();
};
}  // namespace core
}  // namespace flecha

#endif  // FLECHA_PARSER_HPP
