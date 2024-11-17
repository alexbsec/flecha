// Flecha Programming Language
// Copyright (c) 2024 J. A. C. Buschinelli - Flecha
// This code is licensed under the MIT License. See LICENSE for details.

#ifndef FLECHA_TOKENTYPE_HPP
#define FLECHA_TOKENTYPE_HPP

enum class TokenType {
    Int,
    Char,
    Bool,
    Float,
    String,
    Void,

    // Operators
    Equal, Add, Sub, Mul, Div, Pow, Xor, Mod, Bang,
    Compare, NotEqual, And, Or, Not, Less, LessEqual, Greater, GreaterEqual,

    // Punctuation
    LParen, RParen, LBracket, RBracket, LCurly, RCurly, SQuote, DQuote,

    // Pointer operators
    DeclPtr, Deref, AddressRef, AssignVal,

    // Keywords
    Strict, Method, Class, Return, Construct, Destruct,
    
    // Identifiers
    Identifier,
    NumberLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // Termination token
    SemiColon,

    // End of file
    EOF_TOKEN,

    // Memory Tokens
    Allot,
    Dellot,

    // No Token
    NoToken
};

#endif  // FLECHA_TOKENTYPE_HPP
