#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "tokenizer/tokenizer.h"
#include "utils/logger.h"

namespace {

std::vector<Token> mustTokenize(const std::string& source) {
    auto result = Tokenizer(source).tokenize();
    EXPECT_TRUE(result.success) << result.error_message.value_or("");
    return result.value.value();
}

}  // namespace

class TokenizerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("TokenizerTest");
            initialized = true;
        }
    }
};

// ---------------------------------------------------------------------------
// tokenTypeToString: round-trip every enum value so every switch arm runs.
// ---------------------------------------------------------------------------
TEST_F(TokenizerTest, TokenTypeToStringCoversAllArms) {
    const std::vector<std::pair<TokensType, std::string>> table = {
        {TokensType::EndOfFile, "EndOfFile"},
        {TokensType::Unknown, "Unknown"},
        {TokensType::Newline, "Newline"},
        {TokensType::CommentTypeOne, "CommentTypeOneSingleLine"},
        {TokensType::CommentTypeTwo, "CommentTypeTwoMultiLine"},
        {TokensType::Identifier, "Identifier"},
        {TokensType::IntegerLiteral, "IntegerLiteral"},
        {TokensType::String, "String"},
        {TokensType::CharLiteral, "CharLiteral"},
        {TokensType::Key_program, "Key_program"},
        {TokensType::Key_var, "Key_var"},
        {TokensType::Key_const, "Key_const"},
        {TokensType::Key_type, "Key_type"},
        {TokensType::Key_function, "Key_function"},
        {TokensType::Key_return, "Key_return"},
        {TokensType::Key_begin, "Key_begin"},
        {TokensType::Key_end, "Key_end"},
        {TokensType::Key_output, "Key_output"},
        {TokensType::Key_if, "Key_if"},
        {TokensType::Key_then, "Key_then"},
        {TokensType::Key_else, "Key_else"},
        {TokensType::Key_while, "Key_while"},
        {TokensType::Key_do, "Key_do"},
        {TokensType::Key_case, "Key_case"},
        {TokensType::Key_of, "Key_of"},
        {TokensType::Key_otherwise, "Key_otherwise"},
        {TokensType::Key_repeat, "Key_repeat"},
        {TokensType::Key_for, "Key_for"},
        {TokensType::Key_until, "Key_until"},
        {TokensType::Key_loop, "Key_loop"},
        {TokensType::Key_pool, "Key_pool"},
        {TokensType::Key_exit, "Key_exit"},
        {TokensType::Key_read, "Key_read"},
        {TokensType::Key_succ, "Key_succ"},
        {TokensType::Key_pred, "Key_pred"},
        {TokensType::Key_chr, "Key_chr"},
        {TokensType::Key_ord, "Key_ord"},
        {TokensType::Key_eof, "Key_eof"},
        {TokensType::Swap, "Swap"},
        {TokensType::Assignment, "Assignment"},
        {TokensType::LessThanEqual, "LessThanEqual"},
        {TokensType::NotEqual, "NotEqual"},
        {TokensType::LessThan, "LessThan"},
        {TokensType::GreaterThanEqual, "GreaterThanEqual"},
        {TokensType::GreaterThan, "GreaterThan"},
        {TokensType::Equal, "Equal"},
        {TokensType::Modulus, "Modulus"},
        {TokensType::And, "And"},
        {TokensType::Or, "Or"},
        {TokensType::Not, "Not"},
        {TokensType::Plus, "Plus"},
        {TokensType::Minus, "Minus"},
        {TokensType::Multiply, "Multiply"},
        {TokensType::Divide, "Divide"},
        {TokensType::Dots, "Dots"},
        {TokensType::Colon, "Colon"},
        {TokensType::Semicolon, "Semicolon"},
        {TokensType::SingleDot, "SingleDot"},
        {TokensType::Comma, "Comma"},
        {TokensType::OpenParen, "OpenParen"},
        {TokensType::CloseParen, "CloseParen"},
    };

    for (const auto& [type, expected] : table) {
        EXPECT_EQ(tokenTypeToString(type), expected);
    }
}

TEST_F(TokenizerTest, EmptySourceProducesOnlyEof) {
    auto tokens = mustTokenize("");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokensType::EndOfFile);
    EXPECT_EQ(tokens[0].lexeme, "");
}

TEST_F(TokenizerTest, WhitespaceOnlySourceProducesOnlyEof) {
    auto tokens = mustTokenize("   \t   \r \t");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokensType::EndOfFile);
}

TEST_F(TokenizerTest, NewlinesAreSkipped) {
    auto tokens = mustTokenize("\n\n\nfoo\n\n");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[0].line, 4);
}

// ---------------------------------------------------------------------------
// Keywords: covers the keyword map and Identifier fallback.
// ---------------------------------------------------------------------------
TEST_F(TokenizerTest, AllKeywordsRecognized) {
    const std::vector<std::pair<std::string, TokensType>> cases = {
        {"program", TokensType::Key_program},
        {"var", TokensType::Key_var},
        {"const", TokensType::Key_const},
        {"type", TokensType::Key_type},
        {"function", TokensType::Key_function},
        {"return", TokensType::Key_return},
        {"begin", TokensType::Key_begin},
        {"end", TokensType::Key_end},
        {"output", TokensType::Key_output},
        {"if", TokensType::Key_if},
        {"then", TokensType::Key_then},
        {"else", TokensType::Key_else},
        {"while", TokensType::Key_while},
        {"do", TokensType::Key_do},
        {"case", TokensType::Key_case},
        {"of", TokensType::Key_of},
        {"otherwise", TokensType::Key_otherwise},
        {"repeat", TokensType::Key_repeat},
        {"for", TokensType::Key_for},
        {"until", TokensType::Key_until},
        {"loop", TokensType::Key_loop},
        {"pool", TokensType::Key_pool},
        {"exit", TokensType::Key_exit},
        {"read", TokensType::Key_read},
        {"succ", TokensType::Key_succ},
        {"pred", TokensType::Key_pred},
        {"chr", TokensType::Key_chr},
        {"ord", TokensType::Key_ord},
        {"eof", TokensType::Key_eof},
        {"mod", TokensType::Modulus},
        {"and", TokensType::And},
        {"or", TokensType::Or},
        {"not", TokensType::Not},
    };

    for (const auto& [lexeme, type] : cases) {
        auto tokens = mustTokenize(lexeme);
        ASSERT_GE(tokens.size(), 1u) << lexeme;
        EXPECT_EQ(tokens[0].type, type) << lexeme;
        EXPECT_EQ(tokens[0].lexeme, lexeme) << lexeme;
    }
}

TEST_F(TokenizerTest, IdentifierFallbackForNonKeywords) {
    auto tokens = mustTokenize("foo bar_baz qux1");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, TokensType::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[1].type, TokensType::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "bar_baz");
    EXPECT_EQ(tokens[2].type, TokensType::Identifier);
    EXPECT_EQ(tokens[2].lexeme, "qux1");
    EXPECT_EQ(tokens[3].type, TokensType::EndOfFile);
}

TEST_F(TokenizerTest, IdentifierMayContainDigitsAndUnderscores) {
    auto tokens = mustTokenize("abc123_def");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "abc123_def");
}

// ---------------------------------------------------------------------------
// Literals
// ---------------------------------------------------------------------------
TEST_F(TokenizerTest, IntegerLiteralIsParsed) {
    auto tokens = mustTokenize("12345");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::IntegerLiteral);
    EXPECT_EQ(tokens[0].lexeme, "12345");
}

TEST_F(TokenizerTest, MultipleIntegerLiteralsSeparatedByWhitespace) {
    auto tokens = mustTokenize("1 22 333");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].lexeme, "1");
    EXPECT_EQ(tokens[1].lexeme, "22");
    EXPECT_EQ(tokens[2].lexeme, "333");
}

TEST_F(TokenizerTest, StringLiteralBasic) {
    auto tokens = mustTokenize("\"hello world\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::String);
    EXPECT_EQ(tokens[0].lexeme, "hello world");
}

TEST_F(TokenizerTest, StringLiteralEmpty) {
    auto tokens = mustTokenize("\"\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::String);
    EXPECT_EQ(tokens[0].lexeme, "");
}

TEST_F(TokenizerTest, UnterminatedStringReachesNewlineFails) {
    auto result = Tokenizer("\"oops\n\"").tokenize();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("Unterminated literal"),
              std::string::npos);
}

TEST_F(TokenizerTest, UnterminatedStringReachesEofFails) {
    auto result = Tokenizer("\"oops").tokenize();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("reached EOF"),
              std::string::npos);
}

TEST_F(TokenizerTest, CharLiteralBasic) {
    auto tokens = mustTokenize("'a'");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::CharLiteral);
    EXPECT_EQ(tokens[0].lexeme, "a");
}

TEST_F(TokenizerTest, CharLiteralMissingClosingQuoteFails) {
    auto result = Tokenizer("'ab").tokenize();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("Character literal"),
              std::string::npos);
}

TEST_F(TokenizerTest, CharLiteralUnterminatedAtNewlineFails) {
    auto result = Tokenizer("'\n").tokenize();
    ASSERT_FALSE(result.success);
}

TEST_F(TokenizerTest, CharLiteralUnterminatedAtEofFails) {
    auto result = Tokenizer("'").tokenize();
    ASSERT_FALSE(result.success);
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------
TEST_F(TokenizerTest, LineCommentIsSkipped) {
    auto tokens = mustTokenize("# this is a line comment\nfoo");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokensType::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
}

TEST_F(TokenizerTest, LineCommentAtEndOfFileIsSkipped) {
    auto tokens = mustTokenize("# trailing comment with no newline");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokensType::EndOfFile);
}

TEST_F(TokenizerTest, MultiLineCommentIsSkipped) {
    auto tokens = mustTokenize("{ first comment } foo { second\nmulti-line } bar");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokensType::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[1].type, TokensType::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "bar");
}

TEST_F(TokenizerTest, UnterminatedMultiLineCommentFails) {
    auto result = Tokenizer("{ never closes").tokenize();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("Unterminated multi-line comment"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// Operators and punctuation
// ---------------------------------------------------------------------------
TEST_F(TokenizerTest, ColonAssignmentAndSwapTokens) {
    auto tokens = mustTokenize(": := :=:");
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].type, TokensType::Colon);
    EXPECT_EQ(tokens[1].type, TokensType::Assignment);
    EXPECT_EQ(tokens[1].lexeme, ":=");
    EXPECT_EQ(tokens[2].type, TokensType::Swap);
    EXPECT_EQ(tokens[2].lexeme, ":=:");
}

TEST_F(TokenizerTest, RelationalOperators) {
    auto tokens = mustTokenize("< <= <> > >= =");
    ASSERT_EQ(tokens.size(), 7u);
    EXPECT_EQ(tokens[0].type, TokensType::LessThan);
    EXPECT_EQ(tokens[1].type, TokensType::LessThanEqual);
    EXPECT_EQ(tokens[2].type, TokensType::NotEqual);
    EXPECT_EQ(tokens[3].type, TokensType::GreaterThan);
    EXPECT_EQ(tokens[4].type, TokensType::GreaterThanEqual);
    EXPECT_EQ(tokens[5].type, TokensType::Equal);
}

TEST_F(TokenizerTest, ArithmeticOperators) {
    auto tokens = mustTokenize("+ - * /");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokensType::Plus);
    EXPECT_EQ(tokens[1].type, TokensType::Minus);
    EXPECT_EQ(tokens[2].type, TokensType::Multiply);
    EXPECT_EQ(tokens[3].type, TokensType::Divide);
}

TEST_F(TokenizerTest, DotAndRangeOperators) {
    auto tokens = mustTokenize(". ..");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokensType::SingleDot);
    EXPECT_EQ(tokens[1].type, TokensType::Dots);
}

TEST_F(TokenizerTest, DelimitersAndPunctuation) {
    auto tokens = mustTokenize("( ) , ;");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokensType::OpenParen);
    EXPECT_EQ(tokens[1].type, TokensType::CloseParen);
    EXPECT_EQ(tokens[2].type, TokensType::Comma);
    EXPECT_EQ(tokens[3].type, TokensType::Semicolon);
}

TEST_F(TokenizerTest, UnexpectedCharacterFails) {
    auto result = Tokenizer("@").tokenize();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("Unexpected character"),
              std::string::npos);
}

TEST_F(TokenizerTest, TokenLineAndColumnAreTracked) {
    auto tokens = mustTokenize("foo\n  bar");
    ASSERT_GE(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].column, 1);
    EXPECT_EQ(tokens[1].line, 2);
    EXPECT_EQ(tokens[1].column, 3);
}

TEST_F(TokenizerTest, ToStringContainsTypeLexemeLineAndColumn) {
    Token t(TokensType::Identifier, "abc", 4, 7);
    const std::string s = t.toString();
    EXPECT_NE(s.find("Identifier"), std::string::npos);
    EXPECT_NE(s.find("abc"), std::string::npos);
    EXPECT_NE(s.find("line:4"), std::string::npos);
    EXPECT_NE(s.find("column:7"), std::string::npos);
}

TEST_F(TokenizerTest, TokenizerErrorMessageEncodesLocation) {
    TokenizerError e("boom", 3, 12);
    const std::string msg = e.message();
    EXPECT_NE(msg.find("TokenizerError"), std::string::npos);
    EXPECT_NE(msg.find("boom"), std::string::npos);
    EXPECT_NE(msg.find("line 3"), std::string::npos);
    EXPECT_NE(msg.find("column 12"), std::string::npos);
}

TEST_F(TokenizerTest, MixedProgramTokenizes) {
    const std::string src =
        "program demo:\n"
        "var x: integer;\n"
        "begin\n"
        "  x := 1 + 2;\n"
        "  output(\"hi\");\n"
        "end demo.";
    auto tokens = mustTokenize(src);
    EXPECT_EQ(tokens.front().type, TokensType::Key_program);
    EXPECT_EQ(tokens.back().type, TokensType::EndOfFile);
}
