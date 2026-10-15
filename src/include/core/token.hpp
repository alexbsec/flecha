#ifndef FLECHA_TOKEN_HPP
#define FLECHA_TOKEN_HPP

#include <string>
#include <utility>

#include "source/source_range.hpp"
#include "token_type.hpp"

using string = std::string;

namespace flecha {
namespace core {

    struct Token {
        TokenType type;
        string value;
        source::SourceRange range;

        Token() : type(TokenType::NoToken) {}
        Token(TokenType type, string value, source::SourceRange range)
            : type(type), value(std::move(value)), range(range) {}
    };

}   // namespace core
}   // namespace flecha

#endif  // FLECHA_TOKEN_HPP
