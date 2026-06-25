// tests/unit/test_analyzer.cpp
//
// Unit tests for the semantic analyzer. Each case is a small WinZigC program
// that is tokenized and parsed, then handed to SemanticAnalyzer::analyze().
// Positive cases must analyze cleanly; negative cases must fail and surface a
// diagnostic whose message identifies the offending rule.
#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "semantic_analyzer/analyzer.h"
#include "parser/parser.h"
#include "tokenizer/tokenizer.h"
#include "utils/logger.h"
#include "utils/tree.h"

namespace {

struct Diagnostic {
    std::string message;
    int line;
    int column;
};

struct AnalysisOutcome {
    bool success = false;
    std::vector<Diagnostic> diagnostics;
    std::vector<Diagnostic> warnings;

    bool containsMessage(const std::string& needle) const {
        for (const auto& d : diagnostics) {
            if (d.message.find(needle) != std::string::npos) return true;
        }
        return false;
    }

    bool containsWarning(const std::string& needle) const {
        for (const auto& w : warnings) {
            if (w.message.find(needle) != std::string::npos) return true;
        }
        return false;
    }
};

// Tokenize -> parse -> analyze. The analyzer prints pretty diagnostics to
// stderr on failure; we redirect it into a sink so test output stays clean.
AnalysisOutcome analyzeSource(const std::string& source) {
    AnalysisOutcome outcome;

    auto tokens = Tokenizer(source).tokenize();
    EXPECT_TRUE(tokens.success) << "tokenize failed: "
                                << tokens.error_message.value_or("");
    if (!tokens.success) return outcome;

    auto tree = Parser(tokens.value.value()).parseTree();
    EXPECT_TRUE(tree.success) << "parse failed: "
                              << tree.error_message.value_or("");
    if (!tree.success) return outcome;

    TreeNode* root = tree.value.value();

    std::ostringstream sink;
    std::streambuf* previous = std::cerr.rdbuf(sink.rdbuf());
    SemanticAnalyzer analyzer(root);
    auto result = analyzer.analyze();
    std::cerr.rdbuf(previous);

    outcome.success = result.success;
    for (const auto& e : analyzer.getErrors()) {
        outcome.diagnostics.push_back({e.msg, e.line, e.column});
    }
    for (const auto& w : analyzer.getWarnings()) {
        outcome.warnings.push_back({w.msg, w.line, w.column});
    }

    delete root;
    return outcome;
}

}  // namespace

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("SemanticAnalyzerTest");
            initialized = true;
        }
    }
};

// --- Positive cases: well-formed programs analyze cleanly ---------------------

TEST_F(SemanticAnalyzerTest, AcceptsProgramWithConstTypeVarFunction) {
    const std::string source = R"(program t:
const Max = 10;
type Color = ( red, green );
var i : integer;
    c : Color;
function F ( x : integer ) : integer;
begin
  F := x + 1
end F;
begin
  i := F(Max);
  if i = 1 then output(i)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsSwapOfSameType) {
    const std::string source = R"(program t:
var x : integer;
    y : integer;
begin
  x :=: y
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsLoopsCasesAndEnumLiterals) {
    const std::string source = R"(program t:
type Color = ( red, green, blue );
var i : integer;
    c : Color;
begin
  i := 0;
  while i < 10 do i := i + 1;
  for (i := 0; i < 3; i := i + 1) output(i);
  case i of
    0: output(1);
    1..2: output(2)
  end;
  c := red;
  output(ord('a'))
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

// --- Negative cases: each violates one semantic rule -------------------------

TEST_F(SemanticAnalyzerTest, RejectsProgramEndNameMismatch) {
    const std::string source = R"(program t:
begin
end x.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("does not match"));
}

TEST_F(SemanticAnalyzerTest, ReportsSourcePositionOnDiagnostic) {
    const std::string source = R"(program t:
begin
end x.)";
    auto outcome = analyzeSource(source);
    ASSERT_FALSE(outcome.diagnostics.empty());
    // "end x." is on line 3; the end name carries a real source position.
    EXPECT_EQ(outcome.diagnostics.front().line, 3);
    EXPECT_GT(outcome.diagnostics.front().column, 0);
}

TEST_F(SemanticAnalyzerTest, RejectsUndeclaredVariableType) {
    const std::string source = R"(program t:
var x : Undeclared;
begin
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Type 'Undeclared' for variable 'x' is not declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsDuplicateVariable) {
    const std::string source = R"(program t:
var x : integer;
    x : integer;
begin
  x := 1
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Variable 'x' is already declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsDuplicateConstant) {
    const std::string source = R"(program t:
const a = 1, a = 2;
var x : integer;
begin
  x := 1
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Constant 'a' is already declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsDuplicateType) {
    const std::string source = R"(program t:
type A = ( one );
     A = ( two );
var x : integer;
begin
  x := 1
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Type 'A' is already declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsNonBooleanIfCondition) {
    const std::string source = R"(program t:
var x : integer;
begin
  if x then output(x)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Condition in 'if' statement must be of boolean type"));
}

TEST_F(SemanticAnalyzerTest, RejectsUndeclaredIdentifier) {
    const std::string source = R"(program t:
var x : integer;
begin
  x := y + 1
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Identifier 'y' is not declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsFunctionEndNameMismatch) {
    const std::string source = R"(program t:
function F ( x : integer ) : integer;
begin
  F := x
end G;
begin
  output(0)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Function 'F' end name 'G' does not match"));
}

TEST_F(SemanticAnalyzerTest, RejectsCallToUndeclaredFunction) {
    const std::string source = R"(program t:
var x : integer;
begin
  x := G(1)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Function 'G' is not declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsCallToNonFunction) {
    const std::string source = R"(program t:
var x : integer;
begin
  x := x(1)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("'x' is not a function"));
}

TEST_F(SemanticAnalyzerTest, RejectsNonPrintableOutput) {
    const std::string source = R"(program t:
var x : integer;
begin
  output(x = 1)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Output statement expects an integer or character expression"));
}

TEST_F(SemanticAnalyzerTest, RejectsLogicalNotOnInteger) {
    const std::string source = R"(program t:
var b : boolean;
begin
  b := not 1
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Logical NOT operator requires a boolean operand"));
}

TEST_F(SemanticAnalyzerTest, RejectsSuccOnCharacter) {
    const std::string source = R"(program t:
var x : integer;
begin
  x := succ('a')
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Successor or predecessor function requires an integer operand"));
}

TEST_F(SemanticAnalyzerTest, RejectsOrdOnInteger) {
    const std::string source = R"(program t:
var x : integer;
begin
  x := ord(1)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Ordinal conversion function requires a character operand"));
}

TEST_F(SemanticAnalyzerTest, RejectsUndeclaredCaseConstant) {
    const std::string source = R"(program t:
var x : integer;
begin
  case x of
    1: output(1);
    Bogus: output(2)
  end
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Case constant 'Bogus' is not declared"));
}

TEST_F(SemanticAnalyzerTest, RejectsEmptyBody) {
    const std::string source = R"(program t:
begin
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("No statements in body"));
}

// --- More positive cases: exercise each statement / expression form ----------

TEST_F(SemanticAnalyzerTest, AcceptsRepeatStatement) {
    const std::string source = R"(program t:
var x : integer;
    y : integer;
begin
  repeat x := 1; y := 2 until x = y
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsLoopWithExit) {
    const std::string source = R"(program t:
var x : integer;
begin
  loop x := 1; exit; pool
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsReadStatement) {
    const std::string source = R"(program t:
var a : integer;
    b : integer;
begin
  read(a, b)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsFunctionWithReturnStatement) {
    const std::string source = R"(program t:
function F ( x : integer ) : integer;
begin
  return(x)
end F;
begin
  output(F(1))
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsFunctionReturningUserDefinedType) {
    const std::string source = R"(program t:
type Color = ( red, green );
function pick ( n : integer ) : Color;
begin
  return(red)
end pick;
begin
  output(0)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsFunctionWithoutReturnStatement) {
    // Functions may omit `return` entirely (e.g. procedures); this must not error.
    const std::string source = R"(program t:
function F ( x : integer ) : integer;
begin
  output(x)
end F;
begin
  output(0)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, RejectsReturnTypeMismatch) {
    const std::string source = R"(program t:
function F ( x : integer ) : integer;
begin
  return('a')
end F;
begin
  output(F(1))
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Return type mismatch in function 'F'"));
}

TEST_F(SemanticAnalyzerTest, RejectsConflictingReturnTypesInSameFunction) {
    const std::string source = R"(program t:
function F ( x : integer ) : integer;
begin
  if x = 1 then return(1) else return('a')
end F;
begin
  output(F(1))
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Return type mismatch in function 'F'"));
}

TEST_F(SemanticAnalyzerTest, AcceptsForLoopWithEmptyCondition) {
    const std::string source = R"(program t:
var i : integer;
    x : integer;
begin
  for (i := 1; ; i := i + 1) x := i
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsCaseWithRangeCharAndOtherwise) {
    const std::string source = R"(program t:
var x : integer;
    y : integer;
begin
  case x of
    1: y := 1;
    'a': y := 3
  otherwise
    y := 0
  end
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, AcceptsArithmeticLogicalAndBuiltinExpressions) {
    const std::string source = R"(program t:
type Color = ( red, green );
var i : integer;
    b : boolean;
    c : char;
    p : Color;
    q : Color;
begin
  i := 1 + 2 - 3 * 4 / 2 mod 2;
  b := (i < 2) and (i > 0);
  b := (i <= 2) or (i >= 0);
  b := not b;
  b := i = 1;
  b := i <> 2;
  b := c = 'a';
  b := (i < 1) = (i > 2);
  b := p = q;
  i := -i;
  i := succ(i);
  i := pred(i);
  c := chr(i);
  i := ord(c);
  b := eof
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, InfersTypeOfUndeclaredVariableFromAssignment) {
    // Assigning to an undeclared name infers its type from the right-hand side.
    const std::string source = R"(program t:
begin
  newvar := 5;
  output(newvar)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

// --- More negative cases -----------------------------------------------------

TEST_F(SemanticAnalyzerTest, RejectsSwapOfMismatchedTypes) {
    const std::string source = R"(program t:
var x : integer;
    y : char;
begin
  x :=: y
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Type mismatch in swap statement"));
}

TEST_F(SemanticAnalyzerTest, RejectsSwapWithUndeclaredIdentifier) {
    const std::string source = R"(program t:
var x : integer;
begin
  x :=: z
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("One of the identifiers in the swap statement is not declared"));
}

TEST_F(SemanticAnalyzerTest, AcceptsUnaryMinusOnInteger) {
    const std::string source = R"(program t:
var i : integer;
begin
  i := -i;
  output(i)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.diagnostics.empty());
}

TEST_F(SemanticAnalyzerTest, RejectsUnaryMinusOnNonInteger) {
    const std::string source = R"(program t:
var c : char;
    i : integer;
begin
  i := -c
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Unary minus operator requires an integer operand"));
}

TEST_F(SemanticAnalyzerTest, RejectsInferringUserDefinedTypeFromAssignment) {
    const std::string source = R"(program t:
type Color = ( red, green );
begin
  newp := red;
  output(0)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_FALSE(outcome.success);
    EXPECT_TRUE(outcome.containsMessage("Cannot infer user-defined type for identifier 'newp'"));
}

// --- void return type and the "non-void function never returns" warning -------

// A function declares a value return type but its body never returns one: this
// is a warning, not an error, and analysis still succeeds.
TEST_F(SemanticAnalyzerTest, WarnsWhenNonVoidFunctionNeverReturnsValue) {
    const std::string source = R"(program t:
var g : integer;
function f ( n : integer ) : integer;
begin
  g := n + 1
end f;
begin
  g := 0;
  output(g)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);  // non-fatal
    EXPECT_TRUE(outcome.containsWarning("never returns a value"));
    EXPECT_TRUE(outcome.containsWarning("consider declaring it 'void'"));
}

// A function explicitly declared 'void' may omit a return entirely: no warning,
// no error.
TEST_F(SemanticAnalyzerTest, AcceptsVoidFunctionWithoutReturn) {
    const std::string source = R"(program t:
var g : integer;
function f ( n : integer ) : void;
begin
  g := n + 1
end f;
begin
  g := 0;
  output(g)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.warnings.empty());
}

// A 'void' function that returns a value is warned about (the value is ignored).
// The function is declared but not called, so no call-site type error arises.
TEST_F(SemanticAnalyzerTest, WarnsWhenVoidFunctionReturnsValue) {
    const std::string source = R"(program t:
function f ( n : integer ) : void;
begin
  return (n + 1)
end f;
begin
  output(0)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.containsWarning("declared 'void' but returns a value"));
}

// A well-formed value-returning function produces no warnings.
TEST_F(SemanticAnalyzerTest, NoWarningWhenFunctionReturnsDeclaredType) {
    const std::string source = R"(program t:
var g : integer;
function f ( n : integer ) : integer;
begin
  return (n + 1)
end f;
begin
  g := f(5);
  output(g)
end t.)";
    auto outcome = analyzeSource(source);
    EXPECT_TRUE(outcome.success);
    EXPECT_TRUE(outcome.warnings.empty());
}
