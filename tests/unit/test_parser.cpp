#include <gtest/gtest.h>

#include <cstdlib>
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

// StringNode -> '<string>' (via output; golden uses string(1) child)
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

// OutExp -> Expression => "integer" | StringNode => "string"
INSTANTIATE_TEST_SUITE_P(
    OutExp, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"outexp_integer", "outexp_integer"},
        GrammarFileCase{"outexp_string", "outexp_string"},
        GrammarFileCase{"outexp_two_int", "outexp_two_int"},
        GrammarFileCase{"outexp_two_mixed", "outexp_two_mixed"},
        GrammarFileCase{"outexp_three", "outexp_three"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Caseclauses -> (Caseclause ';')+
INSTANTIATE_TEST_SUITE_P(
    CaseClauses, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"caseclauses_one", "caseclauses_one"},
        GrammarFileCase{"caseclauses_two", "caseclauses_two"},
        GrammarFileCase{"caseclauses_three", "caseclauses_three"},
        GrammarFileCase{"caseclauses_mixed", "caseclauses_mixed"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Caseclause -> CaseExpression list ',' ':' Statement => "case_clause"
INSTANTIATE_TEST_SUITE_P(
    CaseClause, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"caseclause_integer", "caseclause_integer"},
        GrammarFileCase{"caseclause_char", "caseclause_char"},
        GrammarFileCase{"caseclause_name", "caseclause_name"},
        GrammarFileCase{"caseclause_labels_two", "caseclause_labels_two"},
        GrammarFileCase{"caseclause_labels_three", "caseclause_labels_three"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// CaseExpression -> ConstValue | ConstValue '..' ConstValue => ".."
INSTANTIATE_TEST_SUITE_P(
    CaseExpression, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"caseclause_integer", "caseclause_integer"},
        GrammarFileCase{"caseclause_char", "caseclause_char"},
        GrammarFileCase{"caseclause_name", "caseclause_name"},
        GrammarFileCase{"caseexpr_range", "caseexpr_range"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// OtherwiseClause -> 'otherwise' Statement | epsilon
INSTANTIATE_TEST_SUITE_P(
    OtherwiseClause, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"otherwise_absent", "otherwise_absent"},
        GrammarFileCase{"otherwise_present", "otherwise_present"}),
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

// Expression -> Term | Term relop Term  (relop: < > <= >= = <>)
INSTANTIATE_TEST_SUITE_P(
    Expression, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"expr_term", "expr_term"},
        GrammarFileCase{"expr_rel_lt", "expr_rel_lt"},
        GrammarFileCase{"expr_rel_gt", "expr_rel_gt"},
        GrammarFileCase{"expr_rel_le", "expr_rel_le"},
        GrammarFileCase{"expr_rel_ge", "expr_rel_ge"},
        GrammarFileCase{"expr_rel_eq", "expr_rel_eq"},
        GrammarFileCase{"expr_rel_ne", "expr_rel_ne"},
        GrammarFileCase{"expr_rel_chain", "expr_rel_chain"},
        GrammarFileCase{"expr_precedence", "expr_precedence"},
        GrammarFileCase{"expr_paren_nested", "expr_paren_nested"},
        GrammarFileCase{"expr_mixed", "expr_mixed"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Term -> Factor | Term '+' Factor | Term '-' Factor | Term 'or' Factor
INSTANTIATE_TEST_SUITE_P(
    Term, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"term_factor", "term_factor"},
        GrammarFileCase{"term_add", "term_add"},
        GrammarFileCase{"term_sub", "term_sub"},
        GrammarFileCase{"term_or", "term_or"},
        GrammarFileCase{"term_add_chain", "term_add_chain"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Factor -> Primary | Factor '*' Primary | Factor '/' Primary
//         | Factor 'and' Primary | Factor 'mod' Primary
INSTANTIATE_TEST_SUITE_P(
    Factor, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"factor_primary", "factor_primary"},
        GrammarFileCase{"factor_mul", "factor_mul"},
        GrammarFileCase{"factor_div", "factor_div"},
        GrammarFileCase{"factor_and", "factor_and"},
        GrammarFileCase{"factor_mod", "factor_mod"},
        GrammarFileCase{"factor_mul_chain", "factor_mul_chain"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Primary -> unary | eof | Identifier | literals | call | '(' Expression ')'
//          | succ | pred | chr | ord
INSTANTIATE_TEST_SUITE_P(
    Primary, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"primary_integer", "primary_integer"},
        GrammarFileCase{"primary_char", "primary_char"},
        GrammarFileCase{"primary_ident", "primary_ident"},
        GrammarFileCase{"primary_eof", "primary_eof"},
        GrammarFileCase{"primary_neg", "primary_neg"},
        GrammarFileCase{"primary_pos", "primary_pos"},
        GrammarFileCase{"primary_not", "primary_not"},
        GrammarFileCase{"primary_paren", "primary_paren"},
        GrammarFileCase{"primary_call", "primary_call"},
        GrammarFileCase{"primary_call_two", "primary_call_two"},
        GrammarFileCase{"primary_succ", "primary_succ"},
        GrammarFileCase{"primary_pred", "primary_pred"},
        GrammarFileCase{"primary_chr", "primary_chr"},
        GrammarFileCase{"primary_ord", "primary_ord"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// Assignment -> Identifier ':=' Expression => "assign"
//            | Identifier ':=:' Identifier => "swap"
INSTANTIATE_TEST_SUITE_P(
    Assignment, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"assign_integer", "assign_integer"},
        GrammarFileCase{"assign_swap", "assign_swap"},
        GrammarFileCase{"assign_name", "assign_name"},
        GrammarFileCase{"assign_char_lit", "assign_char_lit"},
        GrammarFileCase{"assign_rel_lt", "assign_rel_lt"},
        GrammarFileCase{"assign_rel_gt", "assign_rel_gt"},
        GrammarFileCase{"assign_rel_le", "assign_rel_le"},
        GrammarFileCase{"assign_rel_ge", "assign_rel_ge"},
        GrammarFileCase{"assign_rel_eq", "assign_rel_eq"},
        GrammarFileCase{"assign_rel_ne", "assign_rel_ne"},
        GrammarFileCase{"assign_add", "assign_add"},
        GrammarFileCase{"assign_sub", "assign_sub"},
        GrammarFileCase{"assign_or", "assign_or"},
        GrammarFileCase{"assign_mul", "assign_mul"},
        GrammarFileCase{"assign_div", "assign_div"},
        GrammarFileCase{"assign_and", "assign_and"},
        GrammarFileCase{"assign_mod", "assign_mod"},
        GrammarFileCase{"assign_unary_minus", "assign_unary_minus"},
        GrammarFileCase{"assign_unary_plus", "assign_unary_plus"},
        GrammarFileCase{"assign_unary_not", "assign_unary_not"},
        GrammarFileCase{"assign_paren", "assign_paren"},
        GrammarFileCase{"assign_eof", "assign_eof"},
        GrammarFileCase{"assign_call", "assign_call"},
        GrammarFileCase{"assign_succ", "assign_succ"},
        GrammarFileCase{"assign_pred", "assign_pred"},
        GrammarFileCase{"assign_chr", "assign_chr"},
        GrammarFileCase{"assign_ord", "assign_ord"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// ForStatement -> Assignment | epsilon => "<null>"
INSTANTIATE_TEST_SUITE_P(
    ForStat, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"forstat_init_assign", "forstat_init_assign"},
        GrammarFileCase{"forstat_init_null", "forstat_init_null"},
        GrammarFileCase{"forstat_init_swap", "forstat_init_swap"},
        GrammarFileCase{"forstat_step_assign", "forstat_step_assign"},
        GrammarFileCase{"forstat_step_null", "forstat_step_null"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// ForExpression -> Expression | epsilon => "true"
INSTANTIATE_TEST_SUITE_P(
    ForExp, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"forexp_expression", "forexp_expression"},
        GrammarFileCase{"forexp_true", "forexp_true"}),
    [](const ::testing::TestParamInfo<GrammarFileCase>& info) {
        return info.param.name;
    });

// for '(' ForStatement ';' ForExpression ';' ForStatement ')' Statement => "for"(4)
INSTANTIATE_TEST_SUITE_P(
    ForLoop, ParserGrammarFileTest,
    ::testing::Values(
        GrammarFileCase{"forloop_full", "forloop_full"},
        GrammarFileCase{"forloop_null_step", "forloop_null_step"},
        GrammarFileCase{"forloop_true_exp", "forloop_true_exp"},
        GrammarFileCase{"forloop_init_only", "forloop_init_only"},
        GrammarFileCase{"forloop_null_init", "forloop_null_init"},
        GrammarFileCase{"forloop_exp_only", "forloop_exp_only"},
        GrammarFileCase{"forloop_step_only", "forloop_step_only"},
        GrammarFileCase{"forloop_all_null", "forloop_all_null"}),
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

TEST_F(ParserGrammarTest, DumpGrammarStemFromEnv) {
    const char* stem = std::getenv("GRAMMAR_STEM");
    if (stem == nullptr) {
        GTEST_SKIP() << "Set GRAMMAR_STEM to dump a single grammar golden tree";
    }
    Logger::init("DumpGrammarStem");
    auto result = parseSource(readFile(grammarPath(std::string(stem) + ".winzig")));
    ASSERT_TRUE(result.success) << stem;
    std::cout << treeToString(result.value.value());
    delete result.value.value();
}

TEST_F(ParserGrammarTest, StringNode_UnterminatedFailsTokenize) {
    const char* source = R"(program t:
begin
  output("oops);
end t.)";

    auto tokens = Tokenizer(source).tokenize();
    EXPECT_FALSE(tokens.success);
}
