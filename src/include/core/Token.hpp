#ifndef FLECHA_TOKEN_HPP
#define FLECHA_TOKEN_HPP

#include <string>
#include "TokenType.hpp"

using string = std::string;

namespace flecha {
namespace core {

    struct Token {
        TokenType type;
        string value;
        int line;
        int column;

        Token()
            : type(TokenType::NoToken), value(""), line(-1), column(-1) {}
        Token(TokenType type, const string& value, int line, int col)
            : type(type), value(value), line(line), column(col) {}
    };

}   // namespace core
}   // namespace flecha

#endif  // FLECHA_TOKEN_HPP
