#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue> 
#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/diagnostics.h"
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

struct SemanticError : public Error {
    std::string msg;
    int line;
    int column;
    SemanticError(std::string m, int l = -1, int c = -1) : msg(std::move(m)), line(l), column(c) {}
    std::string message() const override {
        std::string locationInfo = (line >= 0 && column >= 0) ? " at line " + std::to_string(line) + ", column " + std::to_string(column) : "";
        return "SemanticError: " + msg + locationInfo;
    }
};
class SemanticAnalyzer {
public:
    SemanticAnalyzer(TreeNode* ast);
    ~SemanticAnalyzer();
    SymbolTable getSymbolTable() const { return symbolTable; }
    Result<void> analyze();

private:
    TreeNode* ast; // The abstract syntax tree to analyze.
    SymbolTable symbolTable; // The symbol table for semantic analysis.
    std::vector<SemanticError> errors; // A list to store semantic errors encountered during analysis.

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
    void analyzeAssignment(TreeNode* node);
    void analyzeForStatement(TreeNode* node);
    
    void analyzeOutputStatement(TreeNode* node);
    
    
    
    
    SemanticType analyzeReturnType(TreeNode* node);
    SemanticType analyzeExpression(TreeNode* node);
    SemanticType analyzeTerm(TreeNode* node);
    SemanticType analyzeFactor(TreeNode* node);
    SemanticType analyzePrimary(TreeNode* node);
    SemanticType analyzeForExpression(TreeNode* node);


    SemanticType analyzeCall(TreeNode* node);

    SemanticType findReturnNodes(TreeNode *node);


    void analyzeAssign(TreeNode* node);
    int countChildren(TreeNode* node);

    bool isIntegerLiteral(TreeNode* node);
    bool isCharLiteral(TreeNode* node);

    static SemanticType getSemanticTypeFromSymbolType(SymbolType type) {
        switch (type) {
            case SymbolType::Integer: return SemanticType::Integer;
            case SymbolType::Char: return SemanticType::Char;
            case SymbolType::String: return SemanticType::String;
            case SymbolType::Boolean: return SemanticType::Boolean;
            case SymbolType::UserDefined: return SemanticType::UserDefined;
            default: return SemanticType::Unknown;
        }
    }

    static SymbolType getSymbolTypeFromSemanticType(SemanticType type) {
        switch (type) {
            case SemanticType::Integer: return SymbolType::Integer;
            case SemanticType::Char: return SymbolType::Char;
            case SemanticType::String: return SymbolType::String;
            case SemanticType::Boolean: return SymbolType::Boolean;
            case SemanticType::UserDefined: return SymbolType::UserDefined;
            default: return SymbolType::UserDefined; // Default to UserDefined for unknown types
        }
    }

    // Helper functions for semantic analysis
};
#endif // SEMANTIC_ANALYZER_H