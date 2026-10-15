// Flecha Programming Language
// Copyright (c) 2024 J. A. C. Buschinelli - Flecha
// This code is licensed under the MIT License. See LICENSE for details.

#ifndef FLECHA_TOKENTYPE_HPP
#define FLECHA_TOKENTYPE_HPP

enum class TokenType {
    // Primitive types
    I8,
    I16,
    I32,
    I64,
    ISize,
    U8,
    U16,
    U32,
    U64,
    USize,
    F32,
    F64,
    Char,
    Bool,
    Void,
    String,
    StringView,

    // Operators
    Assign,
    DeclareAssign,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Xor,
    Bang,
    Equal,
    NotEqual,
    StrictEqual,
    StrictNotEqual,
    And,
    Or,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    // Punctuation
    LParen,
    RParen,
    LBracket,
    RBracket,
    LCurly,
    RCurly,
    Comma,
    Dot,
    Colon,
    SemiColon,
    Hash,

    // Keywords
    Unsafe,
    Export,
    Extern,
    Inline,
    Comptime,
    Return,
    Yield,
    If,
    Else,
    Also,
    While,
    Proceed,
    Break,
    JumpTo,
    Error,
    Const,
    Volatile,
    As,

    // Built-in constants
    None,
    Null,
    True,
    False,

    // Identifiers
    Identifier,
    NumberLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // End of file
    EOF_TOKEN,

    // No Token
    NoToken,

    // Invalid source retained for error recovery
    Invalid
};

#endif  // FLECHA_TOKENTYPE_HPP
