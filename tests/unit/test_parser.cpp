#include <gtest/gtest.h>

#include <string>

#include "parser.h"
#include "utils/filereader.h"
#include "utils/logger.h"
#include "utils/tokenizer.h"
#include "utils/tree.h"

namespace {

std::string readFile(const std::string& path) {
    auto result = FileReader(path).read();
    if (!result.success) {
        throw std::runtime_error("failed to read " + path);
    }
    return result.value->content;
}

Result<TreeNode*> parseSource(const std::string& source) {
    auto tokens = Tokenizer(source).tokenize();
    if (!tokens.success) {
        return Result<TreeNode*>::Err(
            ParserError(tokens.error_message.value_or("tokenize failed")));
    }
    return Parser(tokens.value.value()).parseTree();
}

std::string grammarPath(const std::string& filename) {
    return std::string(TEST_GRAMMAR_DIR) + "/" + filename;
}

}  // namespace

struct GrammarFileCase {
    const char* name;
    const char* stem;
};

class ParserGrammarFileTest : public ::testing::TestWithParam<GrammarFileCase> {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("ParserGrammarTest");
            initialized = true;
        }
    }
};

TEST_P(ParserGrammarFileTest, MatchesGoldenTree) {
    const auto& p = GetParam();
    const std::string source =
        readFile(grammarPath(std::string(p.stem) + ".winzig"));
    const std::string expected =
        readFile(grammarPath(std::string(p.stem) + ".tree"));

    auto result = parseSource(source);
    ASSERT_TRUE(result.success) << p.name;
    ASSERT_TRUE(result.value.has_value()) << p.name;

    TreeNode* root = result.value.value();
    EXPECT_EQ(treeToString(root), expected) << p.name;
    delete root;
}

INSTANTIATE_TEST_SUITE_P(
    StringNode, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"basic", "string_node"},
        GrammarFileCase{"empty", "string_empty"},
        GrammarFileCase{"spaces", "string_spaces"},
        GrammarFileCase{"two_in_output", "string_two"},
        GrammarFileCase{"special_chars", "string_special"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Grammar: Consts -> (empty) | 'const' Const list ',' ';'
//          Const -> Name '=' ConstValue => "const"
//          ConstValue -> '<integer>' | '<char>' | Name
INSTANTIATE_TEST_SUITE_P(
    Const, ParserGrammarFileTest,
    ::testing::Values(
        // Consts (epsilon)
        GrammarFileCase{"consts_empty", "consts_empty"},
        // Consts (one Const)
        GrammarFileCase{"const_integer", "const_integer"},
        GrammarFileCase{"const_zero", "const_zero"},
        GrammarFileCase{"const_char", "const_char"},
        GrammarFileCase{"const_char_zero", "const_char_zero"},
        GrammarFileCase{"const_name", "const_name"},
        GrammarFileCase{"const_ident_underscore", "const_ident_underscore"},
        // Consts (comma list: 2 and 3 Const nodes)
        GrammarFileCase{"const_two", "const_two"},
        GrammarFileCase{"const_pair", "const_pair"},
        GrammarFileCase{"const_list", "const_list"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

class ParserGrammarTest : public ::testing::Test {};

TEST_F(ParserGrammarTest, StringNode_UnterminatedFailsTokenize) {
    const char* source = R"(program t:
begin
  output("oops);
end t.)";

    auto tokens = Tokenizer(source).tokenize();
    EXPECT_FALSE(tokens.success);
}
