#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include <iostream>
#include <vector>
#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/tree.h"
#include "tokenizer/tokenizer.h"
#include "semantic_analyzer/symbol.h"

struct CodeGeneratorError : public Error {
    std::string msg;
    CodeGeneratorError(std::string m) : msg(std::move(m)) {}
    std::string message() const override { return "CodeGeneratorError: " + msg; }
};

struct CodeResult {
    int stackPointer;
    int nextInstruction;
    CodeResult(int stackPointer, int nextInstruction)
        : stackPointer(stackPointer), nextInstruction(nextInstruction) {}
    CodeResult() = default;
};

struct CodeInput {
    int stackPointer;
    int nextInstruction;
    CodeInput(int stackPointer, int nextInstruction) : stackPointer(stackPointer), nextInstruction(nextInstruction) {}
};

class CodeGenerator {
public:
    CodeGenerator(TreeNode* ast, SymbolTable symbolTable);
    ~CodeGenerator();
    Result<void> generate();    
    void printGeneratedCode() const {
        std::cout << "Generated Code:" << std::endl;
        for (const auto& line : generatedCode) {
            std::cout << line << std::endl;
        }
    }

private:
    TreeNode* ast; // The abstract syntax tree to generate code from.
    SymbolTable symbolTable; // The symbol table for code generation.
    std::vector<std::string> generatedCode; // Store generated code lines
    Result<CodeResult> generateProgram(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateConsts(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateTypes(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateDclns(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateSubprogs(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateBody(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateStatement(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateOutputStatement(TreeNode* node, CodeInput input = CodeInput(0, 0));

    Result<CodeResult> generateString(TreeNode* node, CodeInput input);
};

#endif // CODE_GENERATOR_H