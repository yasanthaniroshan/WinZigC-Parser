#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include <iostream>
#include <fstream>
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
    CodeGenerator(TreeNode* ast, SymbolTable symbolTable,std::string outputFile = "output.asm");
    ~CodeGenerator();
    Result<void> generate();    
    void printGeneratedCode() const {
        std::cout << "Generated Code:" << std::endl;
        for (const auto& line : generatedCode) {
            std::cout << line << std::endl;
        }
    }
    void saveGeneratedCode() const {
        std::ofstream outFile(outputFile);
        if (!outFile) {
            LOG_ERROR("Failed to open output file: " + outputFile);
            return;
        }
        for (const auto& line : generatedCode) {
            outFile << line << std::endl;
        }
        LOG_INFO("Generated code saved to " + outputFile);
    }

private:
    TreeNode* ast; // The abstract syntax tree to generate code from.
    SymbolTable symbolTable; // The symbol table for code generation.
    std::string outputFile; // The output file for the generated code
    std::vector<std::string> generatedCode; // Store generated code lines
    Result<CodeResult> generateProgram(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateConsts(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateTypes(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateDclns(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateSubprogs(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateBody(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateStatement(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateExpression(TreeNode* node, CodeInput input = CodeInput(0, 0)); // Assuming this will be implemented to handle expressions
    Result<CodeResult> generateOutputStatement(TreeNode* node, CodeInput input = CodeInput(0, 0));
    Result<CodeResult> generateString(TreeNode* node, CodeInput input);
    Result<CodeResult> generateAssignment(TreeNode* node, CodeInput input);
    Result<CodeResult> generateSwap(TreeNode* node, CodeInput input);
    Result<CodeResult> generateIfStatement(TreeNode* node, CodeInput input);
    Result<CodeResult> generateWhileStatement(TreeNode* node, CodeInput input);
    Result<CodeResult> generateReadStatement(TreeNode* node, CodeInput input);
    Result<CodeResult> generateRepeatStatement(TreeNode* node, CodeInput input);
    Result<CodeResult> generateForStatement(TreeNode* node, CodeInput input);
    Result<CodeResult> generateCaseStatement(TreeNode* node, CodeInput input);

    // helper functions for code generation
    void emit (const std::string& instr);
    void emit (const std::string& instr, int operand);
    void emit (const std::string& intr, const std::string& operand);
};

#endif // CODE_GENERATOR_H