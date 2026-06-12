#include "code_generator/generator.h"

CodeGenerator::CodeGenerator(TreeNode *ast, SymbolTable symbolTable) : ast(ast), symbolTable(symbolTable)
{
}

CodeGenerator::~CodeGenerator()
{
    // The destructor does not need to delete `ast` because it is owned by the caller.
}

Result<void> CodeGenerator::generate()
{
    LOG_INFO("Starting code generation.");
    generateProgram(ast, CodeInput(0, 0));
    printGeneratedCode();
    return Result<void>::Ok();
}

Result<CodeResult> CodeGenerator::generateProgram(TreeNode *node, CodeInput input)
{
    TreeNode *name = node->left;       // First child is program name
    TreeNode *consts = name->right;    // First sibling is const declarations
    TreeNode *types = consts->right;   // Next sibling is type declarations
    TreeNode *dclns = types->right;    // Next sibling is variable and function declarations
    TreeNode *subprogs = dclns->right; // Next sibling is subprogram declarations
    if (subprogs->right == nullptr)
    {
        LOG_ERROR("Program body is missing.");
        return Result<CodeResult>::Err(CodeGeneratorError("Program body is missing."));
    }
    TreeNode *body = subprogs->right; // Next sibling is the body of the program
    TreeNode *endName = body->right;  // Next sibling is the end name of the program
    Result<CodeResult> constsCode = generateConsts(consts, input);
    Result<CodeResult> typesCode = generateTypes(types, input);
    Result<CodeResult> dclnsCode = generateDclns(dclns, input);
    Result<CodeResult> subprogsCode = generateSubprogs(subprogs, input);
    Result<CodeResult> bodyCode = generateBody(body, input);
    // Combine the generated code from all components
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateConsts(TreeNode *node, CodeInput input)
{
    // TODO: implement constant declaration code generation
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateTypes(TreeNode *node, CodeInput input)
{
    // TODO: implement type declaration code generation
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateDclns(TreeNode *node, CodeInput input)
{
    // TODO: implement variable/function declaration code generation
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateSubprogs(TreeNode *node, CodeInput input)
{
    // TODO: implement subprogram code generation
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateBody(TreeNode *node, CodeInput input)
{
    // TODO: implement program body code generation
    LOG_INFO("Generating code for program body with node value: " + node->value);
    TreeNode *current = node->left; // First child is the first statement
    Result<CodeResult> statementCode;
    while (current != nullptr)
    {
        if (current->value == "output")
        {
            statementCode = generateOutputStatement(current, input);
        }
        current = current->right; // Move to the next sibling statement
    }
    CodeResult result;
    result.stackPointer = statementCode.value->stackPointer;       // Example of modifying the stack pointer for the body
    result.nextInstruction = statementCode.value->nextInstruction; // Example of modifying the next instruction for the body
    return Result<CodeResult>::Ok(result);
}

Result<CodeResult> CodeGenerator::generateStatement(TreeNode *node, CodeInput input)
{
    // TODO: implement statement code generation
    return Result<CodeResult>::Ok(CodeResult(0, 0));
}

Result<CodeResult> CodeGenerator::generateOutputStatement(TreeNode *node, CodeInput input)
{
    // TODO: implement output statement code generation
    TreeNode *current = node->left; // First child is the expression to output
    LOG_INFO("Generating code for output statement with node value: " + current->value);
    Result<CodeResult> stringResult = Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
    if (current->value == "string")
    {
        stringResult = generateString(current, input);
    }
    else
    {
        // For simplicity, we will just generate code to print the value of the expression without evaluating it
        LOG_INFO("Generating code for output statement with expression: " + current->value);
    }
    CodeResult result;
    result.stackPointer = stringResult.value->stackPointer + 1;       // Example of modifying the stack pointer for the output statement
    result.nextInstruction = stringResult.value->nextInstruction + 1; // Example of modifying the next instruction
    generatedCode.push_back("print");               // Example of adding a line of code to the generated code
    return Result<CodeResult>::Ok(result);
}

Result<CodeResult> CodeGenerator::generateString(TreeNode *node, CodeInput input)
{
    TreeNode *stringLiteralNode = node->left;           // First child is the string literal
    std::string stringValue = stringLiteralNode->left->value; // Get the string value
    CodeResult result;
    result.stackPointer = input.stackPointer + 1;            // Example of modifying the stack pointer for the string literal
    result.nextInstruction = input.nextInstruction + 1;      // Example of modifying the next instruction
    generatedCode.push_back("load \"" + stringValue + "\""); // Example of adding a line of code to load the string literal
    return Result<CodeResult>::Ok(result);
}