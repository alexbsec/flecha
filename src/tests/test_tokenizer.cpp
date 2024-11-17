#include "core/Tokenizer.hpp"
#include <gtest/gtest.h>
#include <vector>

using namespace flecha::core;

std::vector<Token> tokenize(const std::string& source) {
    Tokenizer tokenizer(source);
    std::vector<Token> tokens = tokenizer.Tokenize(); 
    return tokens;
}


TEST(TokenizerTests, RecognizesKeywords) {
    auto tokens = tokenize("int char bool");
    ASSERT_EQ(tokens.size(), 4); // 3 keywords + EOF

    EXPECT_EQ(tokens[0].type, TokenType::Int);
    EXPECT_EQ(tokens[0].value, "int");

    EXPECT_EQ(tokens[1].type, TokenType::Char);
    EXPECT_EQ(tokens[1].value, "char");

    EXPECT_EQ(tokens[2].type, TokenType::Bool);
    EXPECT_EQ(tokens[2].value, "bool");

    EXPECT_EQ(tokens[3].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesMultiCharacterOperators) {
    auto tokens = tokenize("-> ** == <=");
    ASSERT_EQ(tokens.size(), 5); // 4 operators + EOF

    EXPECT_EQ(tokens[0].type, TokenType::AssignVal);
    EXPECT_EQ(tokens[0].value, "->");

    EXPECT_EQ(tokens[1].type, TokenType::Pow);
    EXPECT_EQ(tokens[1].value, "**");

    EXPECT_EQ(tokens[2].type, TokenType::Compare);
    EXPECT_EQ(tokens[2].value, "==");

    EXPECT_EQ(tokens[3].type, TokenType::LessEqual);
    EXPECT_EQ(tokens[3].value, "<=");

    EXPECT_EQ(tokens[4].type, TokenType::EOF_TOKEN);
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
    auto tokens = tokenize("! ? ( ) [ ] { }");
    ASSERT_EQ(tokens.size(), 9); // 8 characters + EOF

    EXPECT_EQ(tokens[0].type, TokenType::Bang);
    EXPECT_EQ(tokens[0].value, "!");

    EXPECT_EQ(tokens[1].type, TokenType::AddressRef);
    EXPECT_EQ(tokens[1].value, "?");

    EXPECT_EQ(tokens[2].type, TokenType::LParen);
    EXPECT_EQ(tokens[2].value, "(");

    EXPECT_EQ(tokens[3].type, TokenType::RParen);
    EXPECT_EQ(tokens[3].value, ")");

    EXPECT_EQ(tokens[4].type, TokenType::LBracket);
    EXPECT_EQ(tokens[4].value, "[");

    EXPECT_EQ(tokens[5].type, TokenType::RBracket);
    EXPECT_EQ(tokens[5].value, "]");

    EXPECT_EQ(tokens[6].type, TokenType::LCurly);
    EXPECT_EQ(tokens[6].value, "{");

    EXPECT_EQ(tokens[7].type, TokenType::RCurly);
    EXPECT_EQ(tokens[7].value, "}");

    EXPECT_EQ(tokens[8].type, TokenType::EOF_TOKEN);
}

TEST(TokenizerTests, RecognizesComplexStatements) {
    auto tokens = tokenize("int! my_var = allot(int)->42;");
    ASSERT_EQ(tokens.size(), 12); // Tokens + EOF

    EXPECT_EQ(tokens[0].type, TokenType::Int);
    EXPECT_EQ(tokens[0].value, "int");

    EXPECT_EQ(tokens[1].type, TokenType::Bang);
    EXPECT_EQ(tokens[1].value, "!");

    EXPECT_EQ(tokens[2].type, TokenType::Identifier);
    EXPECT_EQ(tokens[2].value, "my_var");

    EXPECT_EQ(tokens[3].type, TokenType::Equal);
    EXPECT_EQ(tokens[3].value, "=");

    EXPECT_EQ(tokens[4].type, TokenType::Allot);
    EXPECT_EQ(tokens[4].value, "allot");

    EXPECT_EQ(tokens[5].type, TokenType::LParen);
    EXPECT_EQ(tokens[5].value, "(");

    EXPECT_EQ(tokens[6].type, TokenType::Int);
    EXPECT_EQ(tokens[6].value, "int");

    EXPECT_EQ(tokens[7].type, TokenType::RParen);
    EXPECT_EQ(tokens[7].value, ")");

    EXPECT_EQ(tokens[8].type, TokenType::AssignVal);
    EXPECT_EQ(tokens[8].value, "->");

    EXPECT_EQ(tokens[9].type, TokenType::NumberLiteral);
    EXPECT_EQ(tokens[9].value, "42");

    EXPECT_EQ(tokens[10].type, TokenType::SemiColon);
    EXPECT_EQ(tokens[10].value, ";");

    EXPECT_EQ(tokens[11].type, TokenType::EOF_TOKEN);
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

