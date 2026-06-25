// tests/unit/test_code_generator.cpp
//
// Unit tests for the code generator, focused on the frame-pointer calling
// convention and scope-aware variable addressing:
//   - Globals (program scope) are addressed absolutely with `save` / `load`.
//   - Function parameters and locals are frame-relative (`save_local` /
//     `load_local`) and each function numbers them from 0.
//   - Each function emits an `enter <argc>` prologue, plus `reserve <nvars>`
//     when it has non-parameter locals.
//
// Each case is a small WinZigC program run through the real pipeline
// (tokenize -> parse -> analyze -> codegen). We assert on the emitted assembly
// rather than exact line numbers, so the tests stay robust to layout changes.
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "code_generator/generator.h"
#include "parser/parser.h"
#include "semantic_analyzer/analyzer.h"
#include "tokenizer/tokenizer.h"
#include "utils/tree.h"

namespace {

// Compile `source` through the whole pipeline and return the emitted assembly
// as a list of trimmed, non-empty instruction lines.
std::vector<std::string> generateAsm(const std::string& source) {
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

    // Keep analyzer/codegen diagnostics out of the test log.
    std::ostringstream sink;
    std::streambuf* prevErr = std::cerr.rdbuf(sink.rdbuf());
    std::streambuf* prevOut = std::cout.rdbuf(sink.rdbuf());

    SemanticAnalyzer analyzer(root);
    auto analysis = analyzer.analyze();
    EXPECT_TRUE(analysis.success) << "semantic analysis failed";

    // Unique per-test output path so parallel ctest runs don't collide.
    const auto* info = testing::UnitTest::GetInstance()->current_test_info();
    std::string outPath = std::string(testing::TempDir()) + "wz_cg_" +
                          info->test_suite_name() + "_" + info->name() + ".asm";

    CodeGenerator generator(root, analyzer.getSymbolTable(), outPath);
    auto codegen = generator.generate();
    EXPECT_TRUE(codegen.success) << "code generation failed";

    std::cerr.rdbuf(prevErr);
    std::cout.rdbuf(prevOut);

    std::ifstream in(outPath);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }

    delete root;
    return lines;
}

bool hasLine(const std::vector<std::string>& lines, const std::string& exact) {
    return std::find(lines.begin(), lines.end(), exact) != lines.end();
}

int countLine(const std::vector<std::string>& lines, const std::string& exact) {
    return static_cast<int>(std::count(lines.begin(), lines.end(), exact));
}

bool hasOpcode(const std::vector<std::string>& lines, const std::string& op) {
    std::string prefix = op + " ";
    for (const auto& l : lines) {
        if (l == op || l.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

}  // namespace

// A program with no functions touches only globals: absolute save/load, and
// none of the frame machinery is emitted.
TEST(CodeGeneratorFrameTest, GlobalsUseAbsoluteAddressingAndNoFrameOps) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var x : integer;\n"
        "    y : integer;\n"
        "begin\n"
        "    x := 1;\n"
        "    y := x + 2;\n"
        "    output(y);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "save 0"));   // x
    EXPECT_TRUE(hasLine(asmLines, "save 1"));   // y
    EXPECT_TRUE(hasLine(asmLines, "load 0"));   // read x

    EXPECT_FALSE(hasOpcode(asmLines, "save_local"));
    EXPECT_FALSE(hasOpcode(asmLines, "load_local"));
    EXPECT_FALSE(hasOpcode(asmLines, "enter"));
    EXPECT_FALSE(hasOpcode(asmLines, "reserve"));
}

// A one-parameter function with no locals emits `enter 1` and accesses its
// parameter frame-relative; no `reserve` is needed.
TEST(CodeGeneratorFrameTest, ParameterOnlyFunctionEntersWithoutReserve) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(a:integer):integer;\n"
        "begin\n"
        "    return (a);\n"
        "end f;\n"
        "begin\n"
        "    g := f(3);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "enter 1"));
    EXPECT_FALSE(hasOpcode(asmLines, "reserve"));
    EXPECT_TRUE(hasLine(asmLines, "load_local 0"));  // param a
    EXPECT_TRUE(hasLine(asmLines, "save 0"));        // global g (absolute)
}

// A function with a local variable reserves space for it; the parameter is
// frame slot 0 and the local is slot 1.
TEST(CodeGeneratorFrameTest, LocalVariableGetsReservedFrameSlot) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(a:integer):integer;\n"
        "var v : integer;\n"
        "begin\n"
        "    v := a;\n"
        "    return (v);\n"
        "end f;\n"
        "begin\n"
        "    g := f(3);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "enter 1"));
    EXPECT_TRUE(hasLine(asmLines, "reserve 1"));
    EXPECT_TRUE(hasLine(asmLines, "load_local 0"));  // param a
    EXPECT_TRUE(hasLine(asmLines, "save_local 1"));  // local v
    EXPECT_TRUE(hasLine(asmLines, "load_local 1"));  // return v
}

// Assigning a (non-shadowed) global from inside a function uses absolute
// `save` -- the global lives below every frame, so the write persists.
TEST(CodeGeneratorFrameTest, FunctionAssignsGlobalWithAbsoluteSave) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var g : integer;\n"
        "function setg(a:integer):integer;\n"
        "begin\n"
        "    g := a;\n"
        "    return (0);\n"
        "end setg;\n"
        "begin\n"
        "    x := setg(7);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_TRUE(hasLine(asmLines, "load_local 0"));  // read param a (frame-relative)
    EXPECT_TRUE(hasLine(asmLines, "save 0"));        // write global g (absolute)
    // The parameter is only read, never written, so there is no save_local.
    EXPECT_FALSE(hasOpcode(asmLines, "save_local"));
}

// A parameter that shares a global's name shadows it: assignment to that name
// is frame-relative (save_local), leaving the global untouched.
TEST(CodeGeneratorFrameTest, ParameterShadowsGlobalOfSameName) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var g : integer;\n"
        "function f(g:integer):integer;\n"
        "begin\n"
        "    g := 99;\n"
        "    return (0);\n"
        "end f;\n"
        "begin\n"
        "    g := 1;\n"
        "    x := f(7);\n"
        "    output(g);\n"
        "end p.\n");

    // Inside f, `g := 99` targets the parameter (frame slot 0), not the global.
    EXPECT_TRUE(hasLine(asmLines, "save_local 0"));
}

// Two functions each number their own locals from 0 -- frame-relative indices
// reset per scope, which is exactly what makes simultaneously-live frames safe.
TEST(CodeGeneratorFrameTest, LocalIndicesResetPerFunctionScope) {
    auto asmLines = generateAsm(
        "program p:\n"
        "var g : integer;\n"
        "function a(p:integer):integer;\n"
        "begin\n"
        "    return (p);\n"
        "end a;\n"
        "function b(q:integer):integer;\n"
        "begin\n"
        "    return (q);\n"
        "end b;\n"
        "begin\n"
        "    g := a(1);\n"
        "    g := b(2);\n"
        "    output(g);\n"
        "end p.\n");

    EXPECT_EQ(countLine(asmLines, "enter 1"), 2);        // one prologue per function
    EXPECT_EQ(countLine(asmLines, "load_local 0"), 2);   // both params are slot 0
}
