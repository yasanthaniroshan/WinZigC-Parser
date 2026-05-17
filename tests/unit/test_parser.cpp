#include <gtest/gtest.h>

#include <string>

#include "parser/parser.h"
#include "utils/filereader.h"
#include "utils/logger.h"
#include "tokenizer/tokenizer.h"
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

// Types  ->              => "types"
//       | 'type' (Type ';')+ => "types"
// Type   -> Name '=' LitList => "type"
// LitList -> '(' Name list ',' ')' => "lit"
INSTANTIATE_TEST_SUITE_P(
    Types, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"types_empty", "types_empty"},
        GrammarFileCase{"types_one", "types_one"},
        GrammarFileCase{"types_two", "types_two"},
        GrammarFileCase{"types_three", "types_three"},
        GrammarFileCase{"types_lit_two", "types_lit_two"},
        GrammarFileCase{"types_lit_three", "types_lit_three"},
        GrammarFileCase{"types_lit_four", "types_lit_four"},
        GrammarFileCase{"types_four", "types_four"},
        GrammarFileCase{"types_two_lit_sizes", "types_two_lit_sizes"},
        GrammarFileCase{"types_with_consts", "types_with_consts"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// SubProgs -> Fcn* => "subprogs"
// Fcn -> 'function' Name '(' Params ')' ':' Name ';' Consts Types Dclns Body Name ';'
INSTANTIATE_TEST_SUITE_P(
    SubProgs, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"subprogs_empty", "subprogs_empty"},
        GrammarFileCase{"subprogs_one", "subprogs_one"},
        GrammarFileCase{"subprogs_two", "subprogs_two"},
        GrammarFileCase{"subprogs_three", "subprogs_three"},
        GrammarFileCase{"subprogs_four", "subprogs_four"},
        GrammarFileCase{"subprogs_fcn_types", "subprogs_fcn_types"},
        GrammarFileCase{"subprogs_fcn_consts", "subprogs_fcn_consts"},
        GrammarFileCase{"subprogs_fcn_dclns", "subprogs_fcn_dclns"},
        GrammarFileCase{"subprogs_params_two", "subprogs_params_two"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Dclns -> | 'var' (Dcln ';')+ => "dclns"
// Dcln  -> Name list ',' ':' Name => "var"
INSTANTIATE_TEST_SUITE_P(
    Dclns, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"dclns_empty", "dclns_empty"},
        GrammarFileCase{"dclns_one", "dclns_one"},
        GrammarFileCase{"dclns_two", "dclns_two"},
        GrammarFileCase{"dclns_three", "dclns_three"},
        GrammarFileCase{"dcln_names_two", "dcln_names_two"},
        GrammarFileCase{"dcln_names_three", "dcln_names_three"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Params -> Dcln list ';' => "params"
INSTANTIATE_TEST_SUITE_P(
    Params, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"params_one", "params_one"},
        GrammarFileCase{"params_two", "params_two"},
        GrammarFileCase{"params_names_two", "params_names_two"},
        GrammarFileCase{"params_names_three", "params_names_three"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Fcn -> 'function' Name '(' Params ')' ':' Name ';' Consts Types Dclns Body Name ';' => "fcn"
INSTANTIATE_TEST_SUITE_P(
    Fcn, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"fcn_minimal", "fcn_minimal"},
        GrammarFileCase{"fcn_with_consts", "fcn_with_consts"},
        GrammarFileCase{"fcn_with_types", "fcn_with_types"},
        GrammarFileCase{"fcn_with_dclns", "fcn_with_dclns"},
        GrammarFileCase{"fcn_with_body", "fcn_with_body"},
        GrammarFileCase{"fcn_full", "fcn_full"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Body -> 'begin' Statement list ';' 'end' => "block"
INSTANTIATE_TEST_SUITE_P(
    Body, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"body_empty", "body_empty"},
        GrammarFileCase{"body_one", "body_one"},
        GrammarFileCase{"body_two", "body_two"},
        GrammarFileCase{"body_three", "body_three"},
        GrammarFileCase{"body_nested", "body_nested"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Statement -> assign | output | if | while | repeat | for | loop | case | read |
//              exit | return | Body | <null>
INSTANTIATE_TEST_SUITE_P(
    Statement, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"stmt_assign", "stmt_assign"},
        GrammarFileCase{"stmt_swap", "stmt_swap"},
        GrammarFileCase{"stmt_output_expr", "stmt_output_expr"},
        GrammarFileCase{"stmt_output_string", "stmt_output_string"},
        GrammarFileCase{"stmt_output_two", "stmt_output_two"},
        GrammarFileCase{"stmt_if", "stmt_if"},
        GrammarFileCase{"stmt_if_else", "stmt_if_else"},
        GrammarFileCase{"stmt_while", "stmt_while"},
        GrammarFileCase{"stmt_repeat_one", "stmt_repeat_one"},
        GrammarFileCase{"stmt_repeat_two", "stmt_repeat_two"},
        GrammarFileCase{"stmt_repeat_null", "stmt_repeat_null"},
        GrammarFileCase{"stmt_for_full", "stmt_for_full"},
        GrammarFileCase{"stmt_for_null_init", "stmt_for_null_init"},
        GrammarFileCase{"stmt_for_true_exp", "stmt_for_true_exp"},
        GrammarFileCase{"stmt_for_null_step", "stmt_for_null_step"},
        GrammarFileCase{"stmt_loop_one", "stmt_loop_one"},
        GrammarFileCase{"stmt_loop_two", "stmt_loop_two"},
        GrammarFileCase{"stmt_loop_null", "stmt_loop_null"},
        GrammarFileCase{"stmt_case_one", "stmt_case_one"},
        GrammarFileCase{"stmt_case_two", "stmt_case_two"},
        GrammarFileCase{"stmt_case_range", "stmt_case_range"},
        GrammarFileCase{"stmt_case_label_list", "stmt_case_label_list"},
        GrammarFileCase{"stmt_case_otherwise", "stmt_case_otherwise"},
        GrammarFileCase{"stmt_read_one", "stmt_read_one"},
        GrammarFileCase{"stmt_read_two", "stmt_read_two"},
        GrammarFileCase{"stmt_exit", "stmt_exit"},
        GrammarFileCase{"stmt_return", "stmt_return"}),
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
