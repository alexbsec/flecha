#ifndef FLECHA_TOKENIZER_HPP
#define FLECHA_TOKENIZER_HPP

#include <string>
#include <vector>
#include "token.hpp"

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

        char _Peek(size_t offset = 0) const;
        char _Advance();
        bool _Match(char expected);
        bool _IsAtEnd() const;
        void _SkipTrivia();
        Token _NextToken();        

    public:
        Tokenizer(const string& src);
        vector<Token> Tokenize();
    };
}
}

#endif  // FLECHA_TOKENIZER_HPP
