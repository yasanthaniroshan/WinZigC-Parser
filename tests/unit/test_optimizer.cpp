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
    auto optimized = optimizer.optimize();
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
