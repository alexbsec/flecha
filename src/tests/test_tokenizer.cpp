#include "core/tokenizer.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace flecha::core;

std::vector<Token> tokenize(const std::string& source) {
    Tokenizer tokenizer(source);
    std::vector<Token> tokens = tokenizer.Tokenize(); 
    return tokens;
}


TEST(TokenizerTests, RecognizesKeywords) {
    auto tokens = tokenize("i32 u8 f64 char bool void unsafe return none");
    ASSERT_EQ(tokens.size(), 10);

    EXPECT_EQ(tokens[0].type, TokenType::I32);
    EXPECT_EQ(tokens[0].value, "i32");

    EXPECT_EQ(tokens[1].type, TokenType::U8);
    EXPECT_EQ(tokens[2].type, TokenType::F64);
    EXPECT_EQ(tokens[3].type, TokenType::Char);
    EXPECT_EQ(tokens[4].type, TokenType::Bool);
    EXPECT_EQ(tokens[5].type, TokenType::Void);
    EXPECT_EQ(tokens[6].type, TokenType::Unsafe);
    EXPECT_EQ(tokens[7].type, TokenType::Return);
    EXPECT_EQ(tokens[8].type, TokenType::None);
    EXPECT_EQ(tokens[9].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesFunctionModifiersAndReservedWords) {
    auto tokens = tokenize(
        "export extern inline comptime yield if else error const volatile as "
        "null true false");

    const std::vector<TokenType> expected = {
        TokenType::Export, TokenType::Extern, TokenType::Inline,
        TokenType::Comptime, TokenType::Yield, TokenType::If,
        TokenType::Else, TokenType::Error, TokenType::Const,
        TokenType::Volatile, TokenType::As, TokenType::Null,
        TokenType::True, TokenType::False, TokenType::EOF_TOKEN,
    };
    ASSERT_EQ(tokens.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(tokens[index].type, expected[index]);
    }
}

TEST(TokenizerTests, RecognizesMultiCharacterOperators) {
    auto tokens = tokenize("= := == != === !== <= >= && ||");
    ASSERT_EQ(tokens.size(), 11);

    EXPECT_EQ(tokens[0].type, TokenType::Assign);
    EXPECT_EQ(tokens[1].type, TokenType::DeclareAssign);
    EXPECT_EQ(tokens[2].type, TokenType::Equal);
    EXPECT_EQ(tokens[3].type, TokenType::NotEqual);
    EXPECT_EQ(tokens[4].type, TokenType::StrictEqual);
    EXPECT_EQ(tokens[5].type, TokenType::StrictNotEqual);
    EXPECT_EQ(tokens[6].type, TokenType::LessEqual);
    EXPECT_EQ(tokens[7].type, TokenType::GreaterEqual);
    EXPECT_EQ(tokens[8].type, TokenType::And);
    EXPECT_EQ(tokens[9].type, TokenType::Or);
    EXPECT_EQ(tokens[10].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesIdentifiers) {
    auto tokens = tokenize("variable1 _v_ar2");
    ASSERT_EQ(tokens.size(), 3); // 2 identifiers + EOF

    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "variable1");

    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "_v_ar2");

    EXPECT_EQ(tokens[2].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesNumbers) {
    auto tokens = tokenize("123 0 3.14");
    ASSERT_EQ(tokens.size(), 4); // 3 numbers + EOF

    EXPECT_EQ(tokens[0].type, TokenType::NumberLiteral);
    EXPECT_EQ(tokens[0].value, "123");

    EXPECT_EQ(tokens[1].type, TokenType::NumberLiteral);
    EXPECT_EQ(tokens[1].value, "0");

    EXPECT_EQ(tokens[2].type, TokenType::FloatLiteral); // If floating-point numbers are supported
    EXPECT_EQ(tokens[2].value, "3.14");

    EXPECT_EQ(tokens[3].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesSpecialCharacters) {
    auto tokens = tokenize("! ( ) [ ] { } , . : ; *");
    ASSERT_EQ(tokens.size(), 13);

    EXPECT_EQ(tokens[0].type, TokenType::Bang);
    EXPECT_EQ(tokens[0].value, "!");

    EXPECT_EQ(tokens[1].type, TokenType::LParen);
    EXPECT_EQ(tokens[2].type, TokenType::RParen);
    EXPECT_EQ(tokens[3].type, TokenType::LBracket);
    EXPECT_EQ(tokens[4].type, TokenType::RBracket);
    EXPECT_EQ(tokens[5].type, TokenType::LCurly);
    EXPECT_EQ(tokens[6].type, TokenType::RCurly);
    EXPECT_EQ(tokens[7].type, TokenType::Comma);
    EXPECT_EQ(tokens[8].type, TokenType::Dot);
    EXPECT_EQ(tokens[9].type, TokenType::Colon);
    EXPECT_EQ(tokens[10].type, TokenType::SemiColon);
    EXPECT_EQ(tokens[11].type, TokenType::Mul);
    EXPECT_EQ(tokens[12].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesComplexStatements) {
    auto tokens = tokenize("unsafe i32 main() { return none; }");
    ASSERT_EQ(tokens.size(), 11);

    EXPECT_EQ(tokens[0].type, TokenType::Unsafe);
    EXPECT_EQ(tokens[1].type, TokenType::I32);
    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[3].type, TokenType::LParen);
    EXPECT_EQ(tokens[4].type, TokenType::RParen);
    EXPECT_EQ(tokens[5].type, TokenType::LCurly);
    EXPECT_EQ(tokens[6].type, TokenType::Return);
    EXPECT_EQ(tokens[7].type, TokenType::None);
    EXPECT_EQ(tokens[8].type, TokenType::SemiColon);
    EXPECT_EQ(tokens[9].type, TokenType::RCurly);
    EXPECT_EQ(tokens[10].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, LegacyAllocationWordsAreIdentifiers) {
    auto tokens = tokenize("allot dellot");

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
}

TEST(TokenizerTests, SkipsLineAndBlockComments) {
    auto tokens = tokenize(
        "i32 // result type\n"
        "main /* no parameters yet */ ()");

    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].type, TokenType::I32);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].type, TokenType::LParen);
    EXPECT_EQ(tokens[3].type, TokenType::RParen);
    EXPECT_EQ(tokens[4].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RejectsUnterminatedBlockComment) {
    EXPECT_THROW(tokenize("i32 /* unfinished"), std::runtime_error);
}

TEST(TokenizerTests, RecognizesStringLiterals) {
    auto tokens = tokenize("\"Hello, World!\" \"This is a test.\" \"Escape \\\"quote\\\"\"");
    ASSERT_EQ(tokens.size(), 4); // 3 string literals + EOF

    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "Hello, World!");

    EXPECT_EQ(tokens[1].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[1].value, "This is a test.");

    EXPECT_EQ(tokens[2].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[2].value, "Escape \"quote\"");

    EXPECT_EQ(tokens[3].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, UnterminatedStringLiteral) {
    EXPECT_THROW({
        tokenize("\"This string is not terminated");
    }, std::runtime_error);
}

TEST(TokenizerTests, RecognizesEmptyStringLiteral) {
    auto tokens = tokenize("\"\"");
    ASSERT_EQ(tokens.size(), 2); // 1 empty string + EOF

    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "");

    EXPECT_EQ(tokens[1].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, StringWithNewlineEscape) {
    auto tokens = tokenize("\"Line1\\nLine2\"");
    ASSERT_EQ(tokens.size(), 2); // 1 string + EOF

    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "Line1\nLine2");

    EXPECT_EQ(tokens[1].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesCharacterLiterals) {
    auto tokens = tokenize("'a' '\\n' '\\''");
    ASSERT_EQ(tokens.size(), 4); // 3 char literals + EOF

    EXPECT_EQ(tokens[0].type, TokenType::CharLiteral);
    EXPECT_EQ(tokens[0].value, "a");

    EXPECT_EQ(tokens[1].type, TokenType::CharLiteral);
    EXPECT_EQ(tokens[1].value, "\n");

    EXPECT_EQ(tokens[2].type, TokenType::CharLiteral);
    EXPECT_EQ(tokens[2].value, "'");

    EXPECT_EQ(tokens[3].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, UnterminatedCharacterLiteral) {
    EXPECT_THROW({
        tokenize("'a");
    }, std::runtime_error);
}

TEST(TokenizerTests, InvalidMultiCharacterLiteral) {
    EXPECT_THROW({
        tokenize("'ab'");
    }, std::runtime_error);
}

TEST(TokenizerTests, EmptyCharacterLiteral) {
    EXPECT_THROW({
        tokenize("''");
    }, std::runtime_error);
}

TEST(TokenizerTests, StringWithEscapedCharacters) {
    auto tokens = tokenize("\"This is a \\\"test\\\" with \\n and \\t.\"");
    ASSERT_EQ(tokens.size(), 2); // 1 string + EOF

    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "This is a \"test\" with \n and \t.");

    EXPECT_EQ(tokens[1].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, CharacterWithInvalidEscapeSequence) {
    EXPECT_THROW({
        tokenize("'\\x'");
    }, std::runtime_error);
}
