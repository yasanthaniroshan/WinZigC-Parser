#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "common/result.h"
#include "common/error.h"
#include "utils/tree.h"
#include "utils/diagnostics.h"
#include "semantic_analyzer/symbol.h"
#include "utils/logger.h"


#define MAX_PASSES 5

struct OptimizerError : public Error {
    std::string msg;
    int line;
    int column;
    OptimizerError(std::string m, int l = -1, int c = -1) : msg(std::move(m)), line(l), column(c) {}
    std::string message() const override {
        std::string locationInfo = (line >= 0 && column >= 0) ? " at line " + std::to_string(line) + ", column " + std::to_string(column) : "";
        return "OptimizerError: " + msg + locationInfo;
    }
};

struct OptimizerWarning : public Error {
    std::string msg;
    int line;
    int column;
    OptimizerWarning(std::string m, int l = -1, int c = -1) : msg(std::move(m)), line(l), column(c) {}
    std::string message() const override {
        std::string locationInfo = (line >= 0 && column >= 0) ? " at line " + std::to_string(line) + ", column " + std::to_string(column) : "";
        return "OptimizerWarning: " + msg + locationInfo;
    }
};


class Optimizer {
    public:
        Optimizer(TreeNode* ast, SymbolTable symbolTable, std::string optimizationLevel)
            : ast(ast), symbolTable(symbolTable), optimizationLevel(std::move(optimizationLevel)) {
            treeTraveler.setSearchMethod(SearchMethod::DEPTH_FIRST);
            }
        ~Optimizer() = default;

        Result<TreeNode*> optimize();

        // The symbol table after optimization (e.g. dead globals removed). Code generation
        // must use THIS table, not the analyzer's, so removed variables drop out of .data.
        SymbolTable getSymbolTable() const { return symbolTable; }

    private:
            
        TreeNode* ast; // The root of the AST to optimize.
        SymbolTable symbolTable; // The symbol table for semantic analysis.
        std::string optimizationLevel; // e.g. "O0", "O1" — selects which passes run.
        TreeTraveler treeTraveler{ast}; // For traversing the AST during optimization passes.
        std::vector<OptimizerWarning> warnings; // Accumulated warnings during optimization passes.
        int instructionRemovedCount = 0; // Count of instructions removed during optimization passes.
        int instructionAddedCount = 0; // Count of instructions added during optimization passes.
        int spaceSavedCount = 0; // Count of space saved during optimization passes.
        int spaceAddedCount = 0; // Count of space added during optimization passes.
        void addWarning(const std::string& message, int line = -1, int column = -1) {
            warnings.emplace_back(message, line, column);
        }
        Result<void> constantFoldingPass();
        Result<void> removeMinusNode(TreeNode* node);
        Result<void> removeConstantAddition(TreeNode* node);
        Result<void> removeConstantSubtraction(TreeNode* node);
        Result<void> removeConstantMultiplication(TreeNode* node);
        Result<void> removeConstantDivision(TreeNode* node);

        Result<void> removeConstantModulus(TreeNode* node);

        // A global is dead only if it is referenced in NEITHER the subprograms nor the body. Removes such globals from both the symbol table and the declaration list.
        Result<void> removeUnusedVariables(TreeNode* dclns, TreeNode* subprogs, TreeNode* body);
        // For each function, remove locals that are never referenced in that function's body. Each function's locals are scoped to its own body, so they're checked there only.
        Result<void> removeUnusedLocalVariables(TreeNode* subprogs);
        bool isVariableUsed(TreeNode* subtreeRoot, const std::string& name);
        TreeNode* spliceDeclaration(TreeNode* dclns, const std::string& name);
        std::vector<std::string> collectDeclaredNames(TreeNode* dclns);



};

#endif // OPTIMIZER_H