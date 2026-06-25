// tests/unit/test_optimizer.cpp
//
// Unit tests for the Optimizer (O1 pass), covering everything in
// src/optimizer/optimizer.cpp:
//   - Constant folding: +, - (binary and unary), *, /, and iterated folding
//     to a fixpoint (e.g. 4 + 2 + 3 -> 9).
//   - Dead global elimination: unused globals drop out of .data, and the
//     remaining globals' addresses compact.
//   - Dead local elimination: unused function locals are removed and the
//     stack frame shrinks (`reserve`), with surviving locals re-addressed and
//     parameters preserved.
//   - O0 is a no-op: nothing is folded or removed.
//
// Each case runs a real WinZigC program through the whole pipeline
// (tokenize -> parse -> analyze -> optimize -> codegen) and asserts on the
// emitted assembly, mirroring the style of test_code_generator.cpp. The
// code generator is given the OPTIMIZER's symbol table (as app/main.cpp does)
// so that removed variables are actually reflected in .data and frame sizes.
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "code_generator/generator.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "semantic_analyzer/analyzer.h"
#include "tokenizer/tokenizer.h"
#include "utils/tree.h"

namespace {

// Compile `source` through tokenize -> parse -> analyze -> optimize(level) ->
// codegen and return the emitted assembly as trimmed, non-empty lines.
// Default level is O2 (the full pipeline: dead-code + propagation + folding).
// Dead-code-only behavior is exercised explicitly with "O1".
std::vector<std::string> optimizeAsm(const std::string& source,
                                     const std::string& level = "O2") {
    std::vector<std::string> lines;

    auto tokens = Tokenizer(source).tokenize();
    EXPECT_TRUE(tokens.success) << "tokenize failed: "
                                << tokens.error_message.value_or("");
    if (!tokens.success) return lines;

    auto tree = Parser(tokens.value.value()).parseTree();
    EXPECT_TRUE(tree.success) << "parse failed: "
                              << tree.error_message.value_or("");
    if (!tree.success) return lines;

    TreeNode* root = tree.value.value();

    // Keep analyzer/optimizer/codegen diagnostics out of the test log.
    std::ostringstream sink;
    std::streambuf* prevErr = std::cerr.rdbuf(sink.rdbuf());
    std::streambuf* prevOut = std::cout.rdbuf(sink.rdbuf());

    SemanticAnalyzer analyzer(root);
    auto analysis = analyzer.analyze();
    EXPECT_TRUE(analysis.success) << "semantic analysis failed";

    Optimizer optimizer(root, analyzer.getSymbolTable(), level);
    auto optimized = optimizer.preOptimize();
    EXPECT_TRUE(optimized.success) << "optimization failed";
    TreeNode* optRoot = optimized.success ? optimized.value.value() : root;

    // Unique per-test output path so parallel ctest runs don't collide.
    const auto* info = testing::UnitTest::GetInstance()->current_test_info();
    std::string outPath = std::string(testing::TempDir()) + "wz_opt_" +
                          info->test_suite_name() + "_" + info->name() + ".asm";

    // Use the OPTIMIZER's symbol table so dead-variable removal is reflected.
    CodeGenerator generator(optRoot, optimizer.getSymbolTable(), outPath);
    auto codegen = generator.generate();
    EXPECT_TRUE(codegen.success) << "code generation failed";

    // Mirror app/main.cpp: run the peephole pass over the emitted assembly and
    // write the post-optimized result, so tests see the final output.
    auto assemblyLines = generator.assembly();
    auto post = optimizer.postOptimize(assemblyLines);
    EXPECT_TRUE(post.success) << "post-optimization failed";
    generator.writeAssembly(assemblyLines);

    std::cerr.rdbuf(prevErr);
    std::cout.rdbuf(prevOut);

    std::ifstream in(outPath);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos) lines.push_back(line.substr(start));
    }

    delete optRoot;
    return lines;
}

bool hasLine(const std::vector<std::string>& lines, const std::string& exact) {
    return std::find(lines.begin(), lines.end(), exact) != lines.end();
}

bool hasOpcode(const std::vector<std::string>& lines, const std::string& op) {
    std::string prefix = op + " ";
    for (const auto& l : lines) {
        if (l == op || l.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

// Names declared in the .data section, in emitted order.
std::vector<std::string> dataVars(const std::vector<std::string>& lines) {
    std::vector<std::string> vars;
    bool inData = false;
    for (const auto& l : lines) {
        if (l == ".data") { inData = true; continue; }
        if (!inData) continue;
        if (!l.empty() && l[0] == '.') break;  // next section (.text/.rodata)
        size_t colon = l.find(':');
        if (colon != std::string::npos) vars.push_back(l.substr(0, colon));
    }
    return vars;
}

// Compile `x := <expr>; output(x)` at O2 and return the asm. `decls` declares the
// variables (default a single integer `x`). Lets expression-folding tests stay terse.
std::vector<std::string> optimizeExpr(const std::string& expr,
                                      const std::string& decls = "var x : integer;") {
    return optimizeAsm("program p:\n" + decls + "\nbegin\n  x := " + expr +
                       ";\n  output(x);\nend p.\n");
}

// Compile a program with the given declarations + body at O2.
std::vector<std::string> optimizeProg(const std::string& body, const std::string& decls = "") {
    return optimizeAsm("program p:\n" + decls + "begin\n" + body + "\nend p.\n");
}

// `read(y); x := <expr in y>; output(x)` — for algebraic/copy tests that need a
// non-constant (runtime) operand the optimizer must preserve.
std::vector<std::string> optimizeWithInput(const std::string& expr) {
    return optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := " + expr +
                       ";\n  output(x);\nend p.\n");
}

// `if <cond> then output(111) else output(222)` at O2.
std::vector<std::string> optimizeCond(const std::string& cond) {
    return optimizeAsm("program p:\nbegin\n  if " + cond +
                       " then output(111) else output(222)\nend p.\n");
}

// ---- Peephole (postOptimize) helpers ----
//
// The peephole pass works on the emitted, label-based assembly. Testing it through
// a high-level program is unreliable because the AST passes (algebraic
// simplification) often remove the same redundancy first. So these helpers feed a
// hand-built sectioned-assembly body straight into Optimizer::postOptimize, which
// isolates the peephole. postOptimize ignores the AST/symbol table, so a default
// Optimizer is fine.
std::vector<std::string> runPeephole(const std::vector<std::string>& textBody,
                                     const std::string& level = "O2") {
    std::vector<std::string> asmLines = {".data", "", ".text", ".globl main", ""};
    for (const auto& l : textBody) asmLines.push_back(l);

    Optimizer optimizer(nullptr, SymbolTable{}, level);
    auto post = optimizer.postOptimize(asmLines);
    EXPECT_TRUE(post.success) << "post-optimization failed";

    std::vector<std::string> out;
    for (auto& l : asmLines) {
        size_t a = l.find_first_not_of(" \t");
        if (a == std::string::npos) continue;  // drop blanks
        size_t b = l.find_last_not_of(" \t\r");
        out.push_back(l.substr(a, b - a + 1));
    }
    return out;
}

// Count instruction lines (in the trimmed peephole output) with the given opcode.
int countOpcode(const std::vector<std::string>& lines, const std::string& op) {
    std::string prefix = op + " ";
    int n = 0;
    for (const auto& l : lines)
        if (l == op || l.rfind(prefix, 0) == 0) ++n;
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constant folding
// ---------------------------------------------------------------------------

TEST(OptimizerFoldingTest, FoldsConstantAddition) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 7 + 4;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 11"));
    EXPECT_FALSE(hasLine(asmLines, "lit 7"));
    EXPECT_FALSE(hasLine(asmLines, "lit 4"));
    EXPECT_FALSE(hasOpcode(asmLines, "add"));
}

TEST(OptimizerFoldingTest, FoldsConstantSubtraction) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 10 - 3;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 7"));
    EXPECT_FALSE(hasOpcode(asmLines, "subtract"));
}

TEST(OptimizerFoldingTest, FoldsConstantMultiplication) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 4 * 3;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 12"));
    EXPECT_FALSE(hasLine(asmLines, "lit 4"));
    EXPECT_FALSE(hasLine(asmLines, "lit 3"));
}

TEST(OptimizerFoldingTest, FoldsConstantDivision) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 20 / 4;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 5"));
    EXPECT_FALSE(hasLine(asmLines, "lit 20"));
}

TEST(OptimizerFoldingTest, FoldsUnaryMinus) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := -10;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit -10"));
}

// The fixpoint loop should keep folding freshly-created constants:
// (4 + 2) + 3 -> 6 + 3 -> 9.
TEST(OptimizerFoldingTest, FoldsNestedAdditionToFixpoint) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 4 + 2 + 3;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 9"));
    EXPECT_FALSE(hasOpcode(asmLines, "add"));
}

// Folding must not touch an expression with a variable operand.
TEST(OptimizerFoldingTest, DoesNotFoldExpressionWithVariable) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 1;\n"
        "    x := x + 20;\n"
        "    output(x);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 20"));
    EXPECT_TRUE(hasOpcode(asmLines, "add"));  // load x; lit 20; add stays
}

// ---------------------------------------------------------------------------
// Dead global elimination + address compaction
// ---------------------------------------------------------------------------

// `x` is kept live via read() (a runtime value, so constant propagation can't
// fold it away); the never-referenced `unused` global is removed from .data.
TEST(OptimizerDeadGlobalTest, RemovesUnusedGlobalFromData) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer; unused : integer;\n"
        "begin\n"
        "    read(x);\n"
        "    output(x);\n"
        "end p.\n");

    auto vars = dataVars(asmLines);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "unused"), vars.end());
}

// An unused global in the middle is removed; the survivors (kept live via read())
// remain, and their addresses compact (though .data references them by name).
TEST(OptimizerDeadGlobalTest, RemovesMiddleUnusedGlobalKeepsOthers) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var a : integer; b : integer; c : integer;\n"
        "begin\n"
        "    read(a);\n"
        "    c := a + 2;\n"
        "    output(c);\n"
        "end p.\n");

    auto vars = dataVars(asmLines);
    ASSERT_EQ(vars.size(), 2u);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "a"), vars.end());
    EXPECT_NE(std::find(vars.begin(), vars.end(), "c"), vars.end());
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "b"), vars.end());
    // save/load reference globals by name, so the surviving references stay valid.
    EXPECT_TRUE(hasLine(asmLines, "save a"));
    EXPECT_TRUE(hasLine(asmLines, "save c"));
}

// ---------------------------------------------------------------------------
// Constant propagation (single-assignment)
// ---------------------------------------------------------------------------

// A variable assigned a constant exactly once is propagated into its uses; the
// resulting expression folds, and the now-dead variables drop out of .data.
TEST(OptimizerConstPropTest, PropagatesSingleConstantThenFolds) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var a, b : integer;\n"
        "begin\n"
        "    a := 5;\n"
        "    b := a + 3;\n"
        "    output(b);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 8"));     // a->5, b->5+3->8
    EXPECT_FALSE(hasOpcode(asmLines, "add"));
    EXPECT_TRUE(dataVars(asmLines).empty());     // both globals eliminated
}

// A variable assigned more than once is NOT propagated (not single-assignment).
TEST(OptimizerConstPropTest, DoesNotPropagateMultiplyAssignedVariable) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 5;\n"
        "    x := x + 1;\n"
        "    output(x);\n"
        "end p.\n");

    auto vars = dataVars(asmLines);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x stays
    EXPECT_TRUE(hasOpcode(asmLines, "add"));                          // x + 1 not folded
}

// A use that precedes the single definition must not be propagated (the read
// would see the default 0, not the later constant).
TEST(OptimizerConstPropTest, DoesNotPropagateWhenUsedBeforeDefinition) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    output(x);\n"
        "    x := 5;\n"
        "    output(x);\n"
        "end p.\n");

    auto vars = dataVars(asmLines);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x kept
    EXPECT_TRUE(hasLine(asmLines, "load x"));                         // first output reads x
}

// Constant propagation also works on a function's own locals: `k := 10` is
// propagated into `n + k`, the slot is reclaimed, and the body folds.
TEST(OptimizerConstPropTest, PropagatesFunctionLocal) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(n:integer):integer;\n"
        "var k : integer;\n"
        "begin\n"
        "    k := 10;\n"
        "    return (n + k);\n"
        "end f;\n"
        "begin\n"
        "    g := f(5);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "lit 10"));        // k propagated to the literal
    EXPECT_FALSE(hasOpcode(asmLines, "reserve"));    // k's frame slot reclaimed
    EXPECT_FALSE(hasOpcode(asmLines, "save_local")); // k no longer stored
}

// A global assigned a constant once but also used inside a function is left
// alone (cross-scope propagation would be unsound).
TEST(OptimizerConstPropTest, DoesNotPropagateGlobalUsedInFunction) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(n:integer):integer;\n"
        "begin\n"
        "    return (g + n);\n"
        "end f;\n"
        "begin\n"
        "    g := 5;\n"
        "    output(f(1));\n"
        "end p.\n");

    auto vars = dataVars(asmLines);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "g"), vars.end());  // g kept (read in f)
}

// ---------------------------------------------------------------------------
// Dead local elimination + frame compaction
// ---------------------------------------------------------------------------

// A function with one dead local and one live local: the dead slot is reclaimed
// (`reserve` shrinks from 2 to 1) and the live local's frame address compacts
// down (2 -> 1) while the parameter keeps address 0.
TEST(OptimizerDeadLocalTest, RemovesUnusedLocalAndCompactsFrame) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(n:integer):integer;\n"
        "var dead : integer; keep : integer;\n"
        "begin\n"
        "    keep := n + 1;\n"
        "    return (keep + keep);\n"
        "end f;\n"
        "begin\n"
        "    g := f(5);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "enter 1"));
    EXPECT_TRUE(hasLine(asmLines, "reserve 1"));     // was reserve 2 before removal
    EXPECT_TRUE(hasLine(asmLines, "load_local 0"));  // parameter n preserved
    EXPECT_TRUE(hasLine(asmLines, "save_local 1"));  // keep shifted 2 -> 1
    EXPECT_FALSE(hasLine(asmLines, "save_local 2"));
}

// When every local is dead, the frame needs no locals at all: no `reserve`.
TEST(OptimizerDeadLocalTest, RemovesAllLocalsDropsReserve) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(n:integer):integer;\n"
        "var dead : integer;\n"
        "begin\n"
        "    return (n + 1);\n"
        "end f;\n"
        "begin\n"
        "    g := f(5);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "enter 1"));
    EXPECT_FALSE(hasOpcode(asmLines, "reserve"));
    EXPECT_TRUE(hasLine(asmLines, "load_local 0"));  // parameter still addressed
}

// ---------------------------------------------------------------------------
// Optimization levels: O0 = nothing, O1 = dead-code only, O2 = + propagate/fold
// ---------------------------------------------------------------------------

TEST(OptimizerLevelTest, O0DoesNotFoldConstants) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 7 + 4;\n"
        "    output(x);\n"
        "end p.\n",
        "O0");

    EXPECT_TRUE(hasOpcode(asmLines, "add"));
    EXPECT_FALSE(hasLine(asmLines, "lit 11"));
}

TEST(OptimizerLevelTest, O0KeepsUnusedGlobal) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer; unused : integer;\n"
        "begin\n"
        "    x := 1;\n"
        "    output(x);\n"
        "end p.\n",
        "O0");

    auto vars = dataVars(asmLines);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "unused"), vars.end());
}

// O1 does dead-code elimination but NOT constant folding/propagation.
TEST(OptimizerLevelTest, O1RemovesDeadCodeButDoesNotFold) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer; unused : integer;\n"
        "begin\n"
        "    x := 7 + 4;\n"
        "    output(x);\n"
        "end p.\n",
        "O1");

    EXPECT_TRUE(hasOpcode(asmLines, "add"));          // not folded at O1
    EXPECT_FALSE(hasLine(asmLines, "lit 11"));
    auto vars = dataVars(asmLines);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "unused"), vars.end());  // dead global gone
}

// O2 folds (and propagates).
TEST(OptimizerLevelTest, O2FoldsConstants) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var x : integer;\n"
        "begin\n"
        "    x := 7 + 4;\n"
        "    output(x);\n"
        "end p.\n",
        "O2");

    EXPECT_TRUE(hasLine(asmLines, "lit 11"));
    EXPECT_FALSE(hasOpcode(asmLines, "add"));
}

// O1 removes a function that is never called (dead-code elimination).
TEST(OptimizerLevelTest, O1RemovesUnusedFunction) {
    auto asmLines = optimizeAsm(
        "program p:\n"
        "var g : integer;\n"
        "function used(n:integer):integer;\n"
        "begin\n"
        "    return (n + 1);\n"
        "end used;\n"
        "function dead(n:integer):integer;\n"
        "begin\n"
        "    return (n + 777);\n"
        "end dead;\n"
        "begin\n"
        "    g := used(5);\n"
        "    output(g);\n"
        "end p.\n",
        "O1");

    EXPECT_TRUE(hasLine(asmLines, "used:"));     // called function kept
    EXPECT_FALSE(hasLine(asmLines, "dead:"));    // uncalled function removed
    EXPECT_FALSE(hasLine(asmLines, "lit 777"));  // its body is gone
}

// ===========================================================================
// Tier 1 #1 — mod folding
// ===========================================================================

TEST(OptimizerModFoldTest, Folds17Mod5)   { EXPECT_TRUE(hasLine(optimizeExpr("17 mod 5"), "lit 2")); }
TEST(OptimizerModFoldTest, Folds10Mod3)   { EXPECT_TRUE(hasLine(optimizeExpr("10 mod 3"), "lit 1")); }
TEST(OptimizerModFoldTest, Folds20Mod4)   { EXPECT_TRUE(hasLine(optimizeExpr("20 mod 4"), "lit 0")); }
TEST(OptimizerModFoldTest, Folds7Mod10)   { EXPECT_TRUE(hasLine(optimizeExpr("7 mod 10"), "lit 7")); }
TEST(OptimizerModFoldTest, Folds100Mod7)  { EXPECT_TRUE(hasLine(optimizeExpr("100 mod 7"), "lit 2")); }
TEST(OptimizerModFoldTest, Folds0Mod9)    { EXPECT_TRUE(hasLine(optimizeExpr("0 mod 9"), "lit 0")); }
TEST(OptimizerModFoldTest, NoModOpcodeAfterFold) { EXPECT_FALSE(hasOpcode(optimizeExpr("17 mod 5"), "mod")); }
TEST(OptimizerModFoldTest, FoldsNestedModThenAdd) { EXPECT_TRUE(hasLine(optimizeExpr("(8 mod 3) + 1"), "lit 3")); }
TEST(OptimizerModFoldTest, FoldsModInsideProduct) { EXPECT_TRUE(hasLine(optimizeExpr("2 * (9 mod 4)"), "lit 2")); }
TEST(OptimizerModFoldTest, FoldedModFeedsProduct) { EXPECT_TRUE(hasLine(optimizeExpr("(15 mod 4) * 10"), "lit 30")); }
TEST(OptimizerModFoldTest, DoesNotFoldVariableMod) {
    auto a = optimizeWithInput("y mod 3");
    EXPECT_TRUE(hasOpcode(a, "mod"));  // runtime operand: not folded
}

// ===========================================================================
// Tier 1 #2 — relational + boolean folding (observed via the surviving branch;
// the comparison/logical opcode must be folded away)
// ===========================================================================

TEST(OptimizerRelFoldTest, LessThanTrue)  { auto a = optimizeCond("3 < 5");  EXPECT_TRUE(hasLine(a, "lit 111")); EXPECT_FALSE(hasOpcode(a, "lessthan")); }
TEST(OptimizerRelFoldTest, LessThanFalse) { auto a = optimizeCond("5 < 3");  EXPECT_TRUE(hasLine(a, "lit 222")); EXPECT_FALSE(hasOpcode(a, "lessthan")); }
TEST(OptimizerRelFoldTest, EqualTrue)     { auto a = optimizeCond("7 = 7");  EXPECT_TRUE(hasLine(a, "lit 111")); EXPECT_FALSE(hasOpcode(a, "equal")); }
TEST(OptimizerRelFoldTest, EqualFalse)    { auto a = optimizeCond("7 = 8");  EXPECT_TRUE(hasLine(a, "lit 222")); }
TEST(OptimizerRelFoldTest, GreaterTrue)   { auto a = optimizeCond("9 > 2");  EXPECT_TRUE(hasLine(a, "lit 111")); EXPECT_FALSE(hasOpcode(a, "greater")); }
TEST(OptimizerRelFoldTest, GreaterEqual)  { auto a = optimizeCond("2 >= 2"); EXPECT_TRUE(hasLine(a, "lit 111")); }
TEST(OptimizerRelFoldTest, LessEqualFalse){ auto a = optimizeCond("4 <= 3"); EXPECT_TRUE(hasLine(a, "lit 222")); }
TEST(OptimizerRelFoldTest, NotEqualTrue)  { auto a = optimizeCond("5 <> 6"); EXPECT_TRUE(hasLine(a, "lit 111")); }
TEST(OptimizerBoolFoldTest, AndFalse)     { auto a = optimizeCond("true and false"); EXPECT_TRUE(hasLine(a, "lit 222")); EXPECT_FALSE(hasOpcode(a, "and")); }
TEST(OptimizerBoolFoldTest, AndTrue)      { auto a = optimizeCond("true and true");  EXPECT_TRUE(hasLine(a, "lit 111")); }
TEST(OptimizerBoolFoldTest, OrTrue)       { auto a = optimizeCond("true or false");  EXPECT_TRUE(hasLine(a, "lit 111")); EXPECT_FALSE(hasOpcode(a, "or")); }
TEST(OptimizerBoolFoldTest, OrFalse)      { auto a = optimizeCond("false or false"); EXPECT_TRUE(hasLine(a, "lit 222")); }
TEST(OptimizerBoolFoldTest, NotTrue)      { auto a = optimizeCond("not true");  EXPECT_TRUE(hasLine(a, "lit 222")); EXPECT_FALSE(hasOpcode(a, "not")); }
TEST(OptimizerBoolFoldTest, NotFalse)     { auto a = optimizeCond("not false"); EXPECT_TRUE(hasLine(a, "lit 111")); }
TEST(OptimizerBoolFoldTest, CombinedRelAnd) { auto a = optimizeCond("(3 < 5) and (2 = 2)"); EXPECT_TRUE(hasLine(a, "lit 111")); }

// ===========================================================================
// Tier 1 #3 — named-constant propagation (user consts, true/false, enum)
// ===========================================================================

TEST(OptimizerNamedConstTest, PropagatesUserConst) {
    EXPECT_TRUE(hasLine(optimizeProg("x := Max;\noutput(x)", "const Max = 10;\nvar x : integer;\n"), "lit 10"));
}
TEST(OptimizerNamedConstTest, ConstFoldsInExpression) {
    EXPECT_TRUE(hasLine(optimizeProg("x := Max + 5;\noutput(x)", "const Max = 10;\nvar x : integer;\n"), "lit 15"));
}
TEST(OptimizerNamedConstTest, ConstTimesLiteral) {
    EXPECT_TRUE(hasLine(optimizeProg("x := Max * 2;\noutput(x)", "const Max = 10;\nvar x : integer;\n"), "lit 20"));
}
TEST(OptimizerNamedConstTest, TwoConstsAdd) {
    EXPECT_TRUE(hasLine(optimizeProg("x := A + B;\noutput(x)", "const A = 3, B = 4;\nvar x : integer;\n"), "lit 7"));
}
TEST(OptimizerNamedConstTest, RepeatedConstUse) {
    EXPECT_TRUE(hasLine(optimizeProg("x := K + K;\noutput(x)", "const K = 2;\nvar x : integer;\n"), "lit 4"));
}
TEST(OptimizerNamedConstTest, ConstDirectlyInOutput) {
    EXPECT_TRUE(hasLine(optimizeProg("output(Max)", "const Max = 42;\n"), "lit 42"));
}
TEST(OptimizerNamedConstTest, TruePropagatesTrueBranch) {
    EXPECT_TRUE(hasLine(optimizeCond("true"), "lit 111"));
}
TEST(OptimizerNamedConstTest, FalsePropagatesElseBranch) {
    EXPECT_TRUE(hasLine(optimizeCond("false"), "lit 222"));
}
TEST(OptimizerNamedConstTest, EnumLiteralBecomesOrdinal) {
    // blue(2) and green(1) become ordinals; 2 = 1 is false, so the else arm survives.
    auto a = optimizeProg("c := blue;\nif c = green then output(7) else output(8)",
                          "type Color = ( red, green, blue );\nvar c : Color;\n");
    EXPECT_TRUE(hasLine(a, "lit 8"));
}
TEST(OptimizerNamedConstTest, EnumOrdinalComparison) {
    auto a = optimizeProg("c := blue;\nif c = red then output(1) else output(2)",
                          "type Color = ( red, green, blue );\nvar c : Color;\n");
    EXPECT_TRUE(hasLine(a, "lit 2"));  // blue(2) -> c, compared to red(0)
}

// ===========================================================================
// Tier 2 #4 — algebraic simplification
// ===========================================================================

TEST(OptimizerAlgebraTest, AddZeroRight)  { EXPECT_FALSE(hasOpcode(optimizeWithInput("y + 0"), "add")); }
TEST(OptimizerAlgebraTest, AddZeroLeft)   { EXPECT_FALSE(hasOpcode(optimizeWithInput("0 + y"), "add")); }
TEST(OptimizerAlgebraTest, SubZero)       { EXPECT_FALSE(hasOpcode(optimizeWithInput("y - 0"), "subtract")); }
TEST(OptimizerAlgebraTest, MulOneRight)   { EXPECT_FALSE(hasOpcode(optimizeWithInput("y * 1"), "multiply")); }
TEST(OptimizerAlgebraTest, MulOneLeft)    { EXPECT_FALSE(hasOpcode(optimizeWithInput("1 * y"), "multiply")); }
TEST(OptimizerAlgebraTest, DivOne)        { EXPECT_FALSE(hasOpcode(optimizeWithInput("y / 1"), "divide")); }
TEST(OptimizerAlgebraTest, MulZeroRight)  { auto a = optimizeWithInput("y * 0"); EXPECT_FALSE(hasOpcode(a, "multiply")); EXPECT_TRUE(hasLine(a, "lit 0")); }
TEST(OptimizerAlgebraTest, MulZeroLeft)   { auto a = optimizeWithInput("0 * y"); EXPECT_FALSE(hasOpcode(a, "multiply")); EXPECT_TRUE(hasLine(a, "lit 0")); }
TEST(OptimizerAlgebraTest, NestedIdentitiesCollapse) { EXPECT_FALSE(hasOpcode(optimizeWithInput("(y + 0) * 1"), "add")); }
TEST(OptimizerAlgebraTest, ChainedMulOne) { EXPECT_FALSE(hasOpcode(optimizeWithInput("(y * 1) * 1"), "multiply")); }
TEST(OptimizerAlgebraTest, KeepsCallWhenTimesZero) {
    // f(3) * 0 must NOT collapse to 0: the call may have side effects.
    auto a = optimizeProg("x := f(3) * 0;\noutput(x)",
                          "var x : integer;\nfunction f(n:integer):integer;\nbegin\n  return (n)\nend f;\n");
    EXPECT_TRUE(hasOpcode(a, "call"));      // f still called
    EXPECT_TRUE(hasOpcode(a, "multiply"));  // not simplified away
}

// ===========================================================================
// Tier 2 #5 — dead-branch elimination
// ===========================================================================

TEST(OptimizerDeadBranchTest, IfTrueKeepsThen)  { auto a = optimizeProg("if true then output(1) else output(2)");  EXPECT_TRUE(hasLine(a, "lit 1")); EXPECT_FALSE(hasLine(a, "lit 2")); }
TEST(OptimizerDeadBranchTest, IfFalseKeepsElse) { auto a = optimizeProg("if false then output(1) else output(2)"); EXPECT_TRUE(hasLine(a, "lit 2")); EXPECT_FALSE(hasLine(a, "lit 1")); }
TEST(OptimizerDeadBranchTest, IfTrueNoElse)     { auto a = optimizeProg("if true then output(5)");  EXPECT_TRUE(hasLine(a, "lit 5")); }
TEST(OptimizerDeadBranchTest, IfFalseNoElseDropped) { auto a = optimizeProg("if false then output(1);\noutput(9)"); EXPECT_FALSE(hasLine(a, "lit 1")); EXPECT_TRUE(hasLine(a, "lit 9")); }
TEST(OptimizerDeadBranchTest, FoldedRelTrue)    { auto a = optimizeProg("if 3 < 9 then output(5) else output(6)"); EXPECT_TRUE(hasLine(a, "lit 5")); }
TEST(OptimizerDeadBranchTest, FoldedRelFalse)   { auto a = optimizeProg("if 9 < 3 then output(5) else output(6)"); EXPECT_TRUE(hasLine(a, "lit 6")); }
TEST(OptimizerDeadBranchTest, ConstConditionResolves) { auto a = optimizeProg("if 5 > 3 then output(1) else output(2)"); EXPECT_TRUE(hasLine(a, "lit 1")); }
TEST(OptimizerDeadBranchTest, NoBranchOpcodesLeft)    { auto a = optimizeProg("if true then output(1) else output(2)"); EXPECT_FALSE(hasOpcode(a, "iffalse")); EXPECT_FALSE(hasOpcode(a, "goto")); }
TEST(OptimizerDeadBranchTest, WhileFalseRemoved)      { auto a = optimizeProg("while false do output(1);\noutput(2)"); EXPECT_FALSE(hasLine(a, "lit 1")); EXPECT_TRUE(hasLine(a, "lit 2")); }
TEST(OptimizerDeadBranchTest, NestedConstantIf)       { auto a = optimizeProg("if true then begin if false then output(1) else output(2) end else output(3)"); EXPECT_TRUE(hasLine(a, "lit 2")); EXPECT_FALSE(hasLine(a, "lit 1")); EXPECT_FALSE(hasLine(a, "lit 3")); }
TEST(OptimizerDeadBranchTest, WhileTrueKept)          { auto a = optimizeProg("while true do output(7)"); EXPECT_TRUE(hasLine(a, "lit 7")); }

// ===========================================================================
// Tier 2 #6 — unreachable-code elimination (after return/exit)
// ===========================================================================

namespace {
std::vector<std::string> optimizeFn(const std::string& fnBody) {
    return optimizeAsm("program p:\nvar g : integer;\nfunction f(n:integer):integer;\nbegin\n" +
                       fnBody + "\nend f;\nbegin\n  g := f(5);\n  output(g)\nend p.\n");
}
}  // namespace

TEST(OptimizerUnreachableTest, DropsAssignAfterReturn)   { EXPECT_FALSE(hasLine(optimizeFn("  return (n + 1);\n  g := 999"), "lit 999")); }
TEST(OptimizerUnreachableTest, DropsOutputAfterReturn)   { EXPECT_FALSE(hasLine(optimizeFn("  return (n);\n  output(888)"), "lit 888")); }
TEST(OptimizerUnreachableTest, DropsMultipleAfterReturn) { auto a = optimizeFn("  return (n);\n  g := 111;\n  g := 222"); EXPECT_FALSE(hasLine(a, "lit 111")); EXPECT_FALSE(hasLine(a, "lit 222")); }
TEST(OptimizerUnreachableTest, KeepsCodeBeforeReturn)    { EXPECT_TRUE(hasLine(optimizeFn("  g := 555;\n  return (n)"), "lit 555")); }
TEST(OptimizerUnreachableTest, KeepsConditionalReturnTail) {
    // The first return is conditional, so the code after it is still reachable.
    auto a = optimizeFn("  if n > 0 then return (n);\n  g := 777;\n  return (0)");
    EXPECT_TRUE(hasLine(a, "lit 777"));
}
TEST(OptimizerUnreachableTest, DropsAfterReturnInNestedBlock) {
    auto a = optimizeFn("  if n > 0 then begin return (n); g := 333 end;\n  return (0)");
    EXPECT_FALSE(hasLine(a, "lit 333"));
}
TEST(OptimizerUnreachableTest, MainBodyReturnNotApplicable) {
    // No return in the main body; nothing is dropped.
    EXPECT_TRUE(hasLine(optimizeProg("output(1);\noutput(2)"), "lit 2"));
}
TEST(OptimizerUnreachableTest, KeepsReturnValueItself) {
    EXPECT_TRUE(hasOpcode(optimizeFn("  return (n + 1);\n  g := 999"), "return"));
}
TEST(OptimizerUnreachableTest, DropsTwoStatementsAfterReturn) {
    auto a = optimizeFn("  return (n);\n  output(444);\n  output(666)");
    EXPECT_FALSE(hasLine(a, "lit 444"));
    EXPECT_FALSE(hasLine(a, "lit 666"));
}
TEST(OptimizerUnreachableTest, ReachableComputationPreserved) {
    auto a = optimizeFn("  g := 222;\n  return (g)");
    EXPECT_TRUE(hasLine(a, "lit 222"));
}

// ===========================================================================
// Tier 2 #7 — copy propagation
// ===========================================================================

TEST(OptimizerCopyPropTest, ReplacesSingleUse) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x eliminated
    EXPECT_NE(std::find(vars.begin(), vars.end(), "y"), vars.end());  // y kept
}
TEST(OptimizerCopyPropTest, ReplacesInExpression) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x + 1);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "x"), vars.end());
    EXPECT_TRUE(hasOpcode(a, "add"));
}
TEST(OptimizerCopyPropTest, ReplacesMultipleUses) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x);\n  output(x);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "x"), vars.end());
}
TEST(OptimizerCopyPropTest, NotPropagatedWhenSourceReassigned) {
    // y is reassigned after the copy, so x must keep the old value.
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  y := 5;\n  output(x);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x kept
}
TEST(OptimizerCopyPropTest, NotPropagatedWhenDestReassigned) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x);\n  x := 7;\n  output(x);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x assigned twice: not a copy
}
TEST(OptimizerCopyPropTest, NotPropagatedWhenUsedBeforeCopy) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  output(x);\n  x := y;\n  output(x);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_NE(std::find(vars.begin(), vars.end(), "x"), vars.end());  // x used before def
}
TEST(OptimizerCopyPropTest, CopyOfReadValueInArithmetic) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x * 2);\nend p.\n");
    EXPECT_TRUE(hasOpcode(a, "multiply"));
    auto vars = dataVars(a);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "x"), vars.end());
}
TEST(OptimizerCopyPropTest, ChainedCopiesCollapse) {
    auto a = optimizeAsm("program p:\nvar x, y, z : integer;\nbegin\n  read(y);\n  x := y;\n  z := x;\n  output(z);\nend p.\n");
    auto vars = dataVars(a);
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "x"), vars.end());
    EXPECT_EQ(std::find(vars.begin(), vars.end(), "z"), vars.end());
}
TEST(OptimizerCopyPropTest, SelfCopyNotAnIssue) {
    // x := x is not a copy candidate (source == dest); program still compiles.
    auto a = optimizeAsm("program p:\nvar x : integer;\nbegin\n  read(x);\n  x := x;\n  output(x);\nend p.\n");
    EXPECT_FALSE(a.empty());
}
TEST(OptimizerCopyPropTest, KeepsSourceVariable) {
    auto a = optimizeAsm("program p:\nvar x, y : integer;\nbegin\n  read(y);\n  x := y;\n  output(x);\nend p.\n");
    EXPECT_TRUE(hasLine(a, "load y"));  // uses y directly
}

// ---------------------------------------------------------------------------
// Peephole (postOptimize) — runs on the emitted, label-based assembly.
// ---------------------------------------------------------------------------

TEST(OptimizerPeepholeTest, RemovesAddZero) {
    // lit 0; add  ->  (gone)   because x + 0 = x
    // (distinct load/save operands so the result is not itself a self-copy)
    auto a = runPeephole({"\tload x", "\tlit 0", "\tadd", "\tsave y", "\tstop"});
    EXPECT_EQ(countOpcode(a, "lit"), 0);
    EXPECT_EQ(countOpcode(a, "add"), 0);
    EXPECT_TRUE(hasLine(a, "load x"));
    EXPECT_TRUE(hasLine(a, "save y"));
}

TEST(OptimizerPeepholeTest, RemovesSubtractZero) {
    // lit 0; subtract  ->  (gone)   because x - 0 = x
    auto a = runPeephole({"\tload x", "\tlit 0", "\tsubtract", "\tprint"});
    EXPECT_EQ(countOpcode(a, "lit"), 0);
    EXPECT_EQ(countOpcode(a, "subtract"), 0);
}

TEST(OptimizerPeepholeTest, RemovesMultiplyOne) {
    // lit 1; multiply  ->  (gone)   because x * 1 = x
    auto a = runPeephole({"\tload x", "\tlit 1", "\tmultiply", "\tprint"});
    EXPECT_EQ(countOpcode(a, "lit"), 0);
    EXPECT_EQ(countOpcode(a, "multiply"), 0);
}

TEST(OptimizerPeepholeTest, RemovesDivideOne) {
    // lit 1; divide  ->  (gone)   because x / 1 = x
    auto a = runPeephole({"\tload x", "\tlit 1", "\tdivide", "\tprint"});
    EXPECT_EQ(countOpcode(a, "lit"), 0);
    EXPECT_EQ(countOpcode(a, "divide"), 0);
}

TEST(OptimizerPeepholeTest, RemovesDoubleNegate) {
    // negate; negate  ->  (gone)   because -(-x) = x
    auto a = runPeephole({"\tload x", "\tnegate", "\tnegate", "\tprint"});
    EXPECT_EQ(countOpcode(a, "negate"), 0);
    EXPECT_TRUE(hasLine(a, "load x"));
    EXPECT_TRUE(hasLine(a, "print"));
}

TEST(OptimizerPeepholeTest, RemovesGlobalSelfCopy) {
    // load x; save x  ->  (gone)   self-copy is a no-op
    auto a = runPeephole({"\tload x", "\tsave x", "\tstop"});
    EXPECT_EQ(countOpcode(a, "load"), 0);
    EXPECT_EQ(countOpcode(a, "save"), 0);
    EXPECT_TRUE(hasLine(a, "stop"));
}

TEST(OptimizerPeepholeTest, RemovesLocalSelfCopy) {
    // load_local 2; save_local 2  ->  (gone)
    auto a = runPeephole({"\tload_local 2", "\tsave_local 2", "\treturn"});
    EXPECT_EQ(countOpcode(a, "load_local"), 0);
    EXPECT_EQ(countOpcode(a, "save_local"), 0);
}

TEST(OptimizerPeepholeTest, RemovesGotoToNextInstruction) {
    // goto L; L: ...  ->  the goto falls through, so it is dropped (label kept)
    auto a = runPeephole({"\tlit 1", "\tgoto .L0", ".L0:", "\tprint"});
    EXPECT_EQ(countOpcode(a, "goto"), 0);
    EXPECT_TRUE(hasLine(a, ".L0:"));
    EXPECT_TRUE(hasLine(a, "print"));
}

TEST(OptimizerPeepholeTest, KeepsGotoToNonAdjacentLabel) {
    // goto .L1 jumps past `print`; it is NOT a fall-through and must be kept.
    auto a = runPeephole({"\tgoto .L1", "\tprint", ".L1:", "\tstop"});
    EXPECT_EQ(countOpcode(a, "goto"), 1);
    EXPECT_TRUE(hasLine(a, "print"));
}

TEST(OptimizerPeepholeTest, DoesNotCancelPairAcrossLabel) {
    // The second `negate` is a jump target, so control may enter between the two
    // and we cannot assume the first ran: the pair must be preserved.
    auto a = runPeephole({"\tnegate", ".L0:", "\tnegate", "\tprint"});
    EXPECT_EQ(countOpcode(a, "negate"), 2);
    EXPECT_TRUE(hasLine(a, ".L0:"));
}

TEST(OptimizerPeepholeTest, DoesNotRemoveSaveThenLoad) {
    // save x; load x is the IRREDUCIBLE direction: `save` stores (a side effect),
    // `load` pushes it back — neither can be dropped on this ISA.
    auto a = runPeephole({"\tsave x", "\tload x", "\tprint"});
    EXPECT_EQ(countOpcode(a, "save"), 1);
    EXPECT_EQ(countOpcode(a, "load"), 1);
}

TEST(OptimizerPeepholeTest, KeepsNonIdentityArithmetic) {
    // lit 2; add is a real addition, not an identity — keep it.
    auto a = runPeephole({"\tload x", "\tlit 2", "\tadd", "\tprint"});
    EXPECT_EQ(countOpcode(a, "lit"), 1);
    EXPECT_EQ(countOpcode(a, "add"), 1);
}

TEST(OptimizerPeepholeTest, DoesNotRemoveMultiplyZeroAsPair) {
    // lit 0; multiply is x*0 = 0, which would also have to drop the computed x —
    // not a simple two-instruction cancellation, so the peephole leaves it.
    auto a = runPeephole({"\tload x", "\tlit 0", "\tmultiply", "\tprint"});
    EXPECT_EQ(countOpcode(a, "multiply"), 1);
}

TEST(OptimizerPeepholeTest, FloatsLeadingLabelForwardOnRemoval) {
    // The label sits on the FIRST instruction of a cancelled pair; it must survive
    // and attach to the following instruction.
    auto a = runPeephole({"\tlit 5", ".L0:", "\tnegate", "\tnegate", "\tprint"});
    EXPECT_EQ(countOpcode(a, "negate"), 0);
    EXPECT_TRUE(hasLine(a, ".L0:"));
    EXPECT_TRUE(hasLine(a, "print"));
}

TEST(OptimizerPeepholeTest, IteratesToFixpoint) {
    // Removing the fall-through goto makes `lit 0; add` adjacent; a second sweep
    // removes that too. Both must be gone in one postOptimize call.
    auto a = runPeephole({"\tlit 5", "\tgoto .L0", ".L0:", "\tlit 0", "\tadd", "\tprint"});
    EXPECT_EQ(countOpcode(a, "goto"), 0);
    EXPECT_EQ(countOpcode(a, "add"), 0);
    EXPECT_EQ(countOpcode(a, "lit"), 1);  // only the surviving `lit 5`
    EXPECT_TRUE(hasLine(a, "lit 5"));
}

TEST(OptimizerPeepholeTest, O0LeavesAssemblyUntouched) {
    // Peephole is an O2 cleanup; O0 must emit codegen output verbatim.
    auto a = runPeephole({"\tload x", "\tlit 0", "\tadd", "\tsave x"}, "O0");
    EXPECT_EQ(countOpcode(a, "lit"), 1);
    EXPECT_EQ(countOpcode(a, "add"), 1);
}

TEST(OptimizerPeepholeTest, O1LeavesAssemblyUntouched) {
    // O1 is dead-code elimination only; the peephole does not run.
    auto a = runPeephole({"\tload x", "\tnegate", "\tnegate", "\tsave x"}, "O1");
    EXPECT_EQ(countOpcode(a, "negate"), 2);
}

TEST(OptimizerPeepholeTest, EndToEndDoubleNegateFolds) {
    // -(-y) is not folded at the AST level (y is a runtime value), so the
    // negate; negate pair survives codegen and is removed by the peephole.
    auto a = optimizeAsm(
        "program p:\nvar y : integer;\nbegin\n  read(y);\n  y := -(-y);\n  output(y);\nend p.\n");
    EXPECT_FALSE(hasOpcode(a, "negate"));
}
