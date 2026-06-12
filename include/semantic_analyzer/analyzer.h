#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include <iostream>
#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/tree.h"
#include "tokenizer/tokenizer.h"
#include "semantic_analyzer/symbol.h"

enum class SemanticType {
    Integer,
    Char,
    String,
    Boolean,
    UserDefined,
    Unknown
};
class SemanticAnalyzer {
public:
    SemanticAnalyzer(TreeNode* ast);
    ~SemanticAnalyzer();
    Result<void> analyze();

private:
    TreeNode* ast; // The abstract syntax tree to analyze.
    SymbolTable symbolTable; // The symbol table for semantic analysis.

    void analyzeProgram(TreeNode* node);

    void analyzeConsts(TreeNode* node);
    void analyzeConst(TreeNode* node);

    void analyzeTypes(TreeNode* node);
    void analyzeType(TreeNode* node);
    void analyzeLiteralList(TreeNode* node);

    void analyzeDclns(TreeNode* node);
    void analyzeDcln(TreeNode* node);

    void analyzeSubprogs(TreeNode* node);
    void analyzeFcn(TreeNode* node);
    void analyzeParams(TreeNode* node);

    void analyzeBody(TreeNode* node);

    void analyzeStatement(TreeNode* node);
    void analyzeOutputStatement(TreeNode* node);


    SemanticType analyzeExpression(TreeNode* node);


    void analyzeAssign(TreeNode* node);
    int countChildren(TreeNode* node);

    bool isIntegerLiteral(TreeNode* node);
    bool isCharLiteral(TreeNode* node);


    // Helper functions for semantic analysis
};
#endif // SEMANTIC_ANALYZER_H