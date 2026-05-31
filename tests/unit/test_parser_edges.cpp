#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "parser/parser.h"
#include "tokenizer/tokenizer.h"
#include "utils/logger.h"

namespace {

// Drive the parser over source that may be ill-formed; branch coverage is the goal.
// Each entry point gets a fresh Parser — the implementation is not re-entrant.
void runParser(const std::string& source, bool printAst = false) {
    auto tokens = Tokenizer(source).tokenize();
    if (!tokens.success) {
        return;
    }
    const auto& toks = tokens.value.value();
    {
        Parser parser(toks);
        (void)parser.parseTree();
    }
    {
        Parser parser(toks);
        (void)parser.parse(printAst);
    }
}

void runParserOnTokens(const std::vector<Token>& tokens, bool printAst = false) {
    {
        Parser parser(tokens);
        (void)parser.parseTree();
    }
    {
        Parser parser(tokens);
        (void)parser.parse(printAst);
    }
}

Token tok(TokensType type, const char* lexeme, int line = 1, int column = 1) {
    return Token(type, lexeme, line, column);
}

}  // namespace

class ParserEdgeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("ParserEdgeTest");
            initialized = true;
        }
    }
};

// ---------------------------------------------------------------------------
// Winzig (grammar lines 1–2): program Name : Consts Types Dclns SubProgs Body Name '.'
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Winzig_ExtraTokensAfterProgramDot) {
    runParser(
        "program p:\n"
        "begin\n"
        "end p.\n"
        "trailing\n");
}

TEST_F(ParserEdgeTest, Winzig_MissingProgramKeyword) {
    runParser(
        "p:\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Winzig_MissingColonAfterName) {
    runParser(
        "program p\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Winzig_MissingFinalDot) {
    runParser(
        "program p:\n"
        "begin\n"
        "end p\n");
}

TEST_F(ParserEdgeTest, Winzig_MismatchedClosingName) {
    runParser(
        "program p:\n"
        "begin\n"
        "end q.\n");
}

TEST_F(ParserEdgeTest, ParseReturnsErrorWhenExtraTokensRemain) {
    const std::string source =
        "program p:\n"
        "begin\n"
        "end p.\n"
        "leftover\n";
    auto tokens = Tokenizer(source).tokenize();
    ASSERT_TRUE(tokens.success);
    auto tree = Parser(tokens.value.value()).parseTree();
    EXPECT_FALSE(tree.success);
    ASSERT_TRUE(tree.error_message.has_value());
    EXPECT_NE(tree.error_message->find("Expected end of file"), std::string::npos);

    auto parsed = Parser(tokens.value.value()).parse(false);
    EXPECT_FALSE(parsed.success);
}

TEST_F(ParserEdgeTest, ParseWithAstFlagOnValidProgram) {
    const std::string source =
        "program p:\n"
        "begin\n"
        "end p.\n";
    auto tokens = Tokenizer(source).tokenize();
    ASSERT_TRUE(tokens.success);
    testing::internal::CaptureStdout();
    auto result = Parser(tokens.value.value()).parse(true);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(result.success);
    EXPECT_NE(out.find("program(7)"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Consts / Const / ConstValue (grammar lines 3–8)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Const_InvalidConstValueKeyword) {
    // ConstValue -> integer | char | Name only; 'output' is a keyword.
    runParser(
        "program p:\n"
        "const x = output;\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Const_InvalidConstValueOperator) {
    runParser(
        "program p:\n"
        "const x = +;\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Const_MissingSemicolonAfterList) {
    runParser(
        "program p:\n"
        "const a = 1, b = 2\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Const_MissingEquals) {
    runParser(
        "program p:\n"
        "const x 1;\n"
        "begin\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Types / Type / LitList (grammar lines 9–12)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Type_LitListMissingOpenParen) {
    runParser(
        "program p:\n"
        "type t = x);\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Type_LitListMissingCloseParen) {
    runParser(
        "program p:\n"
        "type t = (x;\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Type_MissingSemicolonAfterDeclaration) {
    runParser(
        "program p:\n"
        "type a = (x);\n"
        "type b = (y)\n"
        "begin\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// SubProgs / Fcn / Params (grammar lines 13–16)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Fcn_MissingParameterListClose) {
    runParser(
        "program p:\n"
        "function f(x : integer\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Fcn_MissingReturnTypeName) {
    runParser(
        "program p:\n"
        "function f() : ;\n"
        "begin\n"
        "end f;\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Fcn_MissingBodyBegin) {
    runParser(
        "program p:\n"
        "function f() : integer;\n"
        "end f;\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Params_MissingColonInDcln) {
    runParser(
        "program p:\n"
        "function f(a integer) : integer;\n"
        "begin\n"
        "end f;\n"
        "begin\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Dclns / Dcln (grammar lines 17–19)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Dcln_MissingSemicolonAfterDeclaration) {
    runParser(
        "program p:\n"
        "var x : integer\n"
        "begin\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Dcln_MissingColonBetweenNamesAndType) {
    runParser(
        "program p:\n"
        "var x integer;\n"
        "begin\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Body / Statement ε (grammar lines 20, 37)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Body_NullStatementBetweenSemicolons) {
    // Statement -> ε produces "<null>" between two ';' in the statement list.
    runParser(
        "program p:\n"
        "begin\n"
        "  ;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Body_EmptyBlock) {
    runParser(
        "program p:\n"
        "begin\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Statement alternatives (grammar lines 21–36)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Statement_InvalidLeadingToken) {
    // Not Assignment, output, if, while, repeat, for, loop, case, read, exit, return, or Body.
    runParser(
        "program p:\n"
        "begin\n"
        "  1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Assignment_MissingAssignOrSwap) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  x;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Output_MissingCloseParen) {
    runParser(
        "program p:\n"
        "begin\n"
        "  output(1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, If_MissingThen) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  if 1 x := 1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, While_MissingDo) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  while 1 x := 1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Repeat_MissingUntil) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  repeat x := 1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Loop_MissingPool) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  loop x := 1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, For_MissingClosingParen) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  for (x := 1; x < 2; x := 3\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, For_NullInitAndTrueCondition) {
    // ForStat -> ε and ForExp -> ε (implemented as "true").
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  for ( ; ; x := 1 ) x := 0;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Case_MissingEnd) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  case x of\n"
        "    1: x := 1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Case_InvalidCaseExpression) {
    // CaseExpression -> ConstValue | ConstValue '..' ConstValue
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  case x of\n"
        "    output: x := 1;\n"
        "  end;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Case_OneClauseThenEndWithoutOtherwise) {
    // OtherwiseClause -> ε; caseclauses loop breaks on ';' then 'end'.
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  case x of\n"
        "    1: x := 1;\n"
        "  end;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Read_NonIdentifierArgument) {
    runParser(
        "program p:\n"
        "begin\n"
        "  read(1);\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Return_MissingExpression) {
    runParser(
        "program p:\n"
        "begin\n"
        "  return;\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Assignment / Expression / Primary (grammar lines 52–89)
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, Primary_InvalidAfterAssign) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  x := );\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Primary_CallMissingCloseParen) {
    runParser(
        "program p:\n"
        "var f : integer;\n"
        "begin\n"
        "  x := f(1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Primary_SuccMissingArgs) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  x := succ;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Primary_ParenMissingClose) {
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  x := (1;\n"
        "end p.\n");
}

TEST_F(ParserEdgeTest, Expression_RangeInCaseLabel) {
  // CaseExpression with '..' (grammar line 48).
    runParser(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "  case x of\n"
        "    1 .. 3: x := 1;\n"
        "  end;\n"
        "end p.\n");
}

// ---------------------------------------------------------------------------
// Token-level fixtures for paths that valid source cannot reach.
// ---------------------------------------------------------------------------

TEST_F(ParserEdgeTest, ConsumeFailureOnMissingColon) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}

TEST_F(ParserEdgeTest, IdentifierExpectedAtProgramName) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::IntegerLiteral, "42"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}

TEST_F(ParserEdgeTest, ConstValueDefaultBranch) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Key_const, "const"),
        tok(TokensType::Identifier, "c"),
        tok(TokensType::Equal, "="),
        tok(TokensType::Semicolon, ";"),  // invalid ConstValue
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}

TEST_F(ParserEdgeTest, StatementDefaultBranch) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::IntegerLiteral, "99"),
        tok(TokensType::Semicolon, ";"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}

TEST_F(ParserEdgeTest, AssignmentDefaultBranch) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Key_var, "var"),
        tok(TokensType::Identifier, "x"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Identifier, "integer"),
        tok(TokensType::Semicolon, ";"),
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::Identifier, "x"),
        tok(TokensType::Semicolon, ";"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}

TEST_F(ParserEdgeTest, PrimaryDefaultBranch) {
    runParserOnTokens({
        tok(TokensType::Key_program, "program"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Key_var, "var"),
        tok(TokensType::Identifier, "x"),
        tok(TokensType::Colon, ":"),
        tok(TokensType::Identifier, "integer"),
        tok(TokensType::Semicolon, ";"),
        tok(TokensType::Key_begin, "begin"),
        tok(TokensType::Identifier, "x"),
        tok(TokensType::Assignment, ":="),
        tok(TokensType::CloseParen, ")"),
        tok(TokensType::Semicolon, ";"),
        tok(TokensType::Key_end, "end"),
        tok(TokensType::Identifier, "p"),
        tok(TokensType::SingleDot, "."),
        tok(TokensType::EndOfFile, ""),
    });
}
