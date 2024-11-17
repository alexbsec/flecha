#ifndef FLECHA_TOKENIZER_HPP
#define FLECHA_TOKENIZER_HPP

#include <string>
#include <vector>
#include <cctype>
#include "Token.hpp"

using string = std::string;
template <typename ...Args> using vector = std::vector<Args...>;

namespace flecha {
namespace core {
    class Tokenizer {
    private:
        string _source;
        size_t _index;
        int _line;
        int _column;

        char _GetCurrentChar() const;
        void _Advance();
        bool _IsAtEnd() const;
        void _SkipWhiteSpace();
        

    public:
        Tokenizer(const string& src);
        Token NextToken();
        vector<Token> Tokenize();
    };
}
}

#endif  // FLECHA_TOKENIZER_HPP
