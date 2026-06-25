#include "code_generator/generator.h"

CodeGenerator::CodeGenerator(TreeNode *ast, SymbolTable symbolTable, std::string outputFile) : ast(ast), symbolTable(symbolTable), outputFile(outputFile)
{
}

CodeGenerator::~CodeGenerator()
{
    // The destructor does not need to delete `ast` because it is owned by the caller.
}

// Overloaded helper functions for generating the instructions
void CodeGenerator::emit(const std::string& instr) {
    generatedCode.push_back(instr);
}

void CodeGenerator::emit(const std::string& instr, int operand) {
    generatedCode.push_back(instr + " " + std::to_string(operand));
}

void CodeGenerator::emit(const std::string& instr, const std::string& operand) {
    generatedCode.push_back(instr + " " + operand);
}

// Globals live in scope 0 at absolute stack slots; function params/locals are
// addressed relative to the current frame pointer (stack[fp + address]).
void CodeGenerator::emitVarLoad(const Symbol* sym) {
    emit(sym->scopeIndex == 0 ? "load" : "load_local", sym->address);
}

void CodeGenerator::emitVarSave(const Symbol* sym) {
    emit(sym->scopeIndex == 0 ? "save" : "save_local", sym->address);
}

//===== Code generation =====
Result<void> CodeGenerator::generate()
{
    LOG_DEBUG("Starting code generation.");
    // Surface code-generation failures instead of silently writing whatever
    // partial output was emitted so far (which produces broken assembly).
    auto programResult = generateProgram(ast, CodeInput(0, 1));
    if (!programResult.success) {
        return Result<void>::ErrMsg(programResult.error_message.value_or("Code generation failed"));
    }
    // printGeneratedCode();
    saveGeneratedCode();
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
    Result<CodeResult> dclnsCode = generateDclns(dclns, input); // Does nothing -- see function docstring
    
    LOG_DEBUG("Stack Pointer and Next Instruction after consts, types and dclns generation: " + std::to_string(input.stackPointer) + ", " + std::to_string(input.nextInstruction));
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // Before generating functions we need an unconditional jump - otherwise program
    // will sequentially execute function and sub-program code as well
    int mainJumpIndex = currentRes.nextInstruction;
    emit("goto -1"); // placeholder unconditional jump - supposed to point beyond all sub-program instrs
    currentRes.nextInstruction++;

    // Generate functions and sub-program instructions    
    Result<CodeResult> subprogsCode = generateSubprogs(subprogs, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!subprogsCode.success) return subprogsCode;
    currentRes = subprogsCode.value.value();

    // Patch eariler goto to point HERE - start of the main body
    generatedCode[mainJumpIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);

    // Generate Main program body
    Result<CodeResult> bodyCode = generateBody(body, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!bodyCode.success) return bodyCode;
    currentRes = bodyCode.value.value();

    // end of the program
    emit("stop");
    currentRes.nextInstruction++;

    return Result<CodeResult>::Ok(currentRes); // fin.
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

/**
 * DOES NOTHING
 * Variable declaration is alreday implicitly handled by the SymbolTable. Therefore
 * we don't need to actually do anything here. I've kept the function call
 * because it makes it easier when reading the `generateProgram` code.
 * Instruction counter and stack-pointer remain unchanged. 
 */
Result<CodeResult> CodeGenerator::generateDclns(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("generateDclns called - but Variable declaration implicitly handled by SymbolTable");

    // therefore simply exiting
    return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
}

/**
 * Iterate over the Fcn node children
 */
Result<CodeResult> CodeGenerator::generateSubprogs(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("Generating subprogs");
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);
    
    if (node == nullptr || node->value != "subprogs") return Result<CodeResult>::Ok(currentRes);

    TreeNode* fcnNode = node->left;
    while (fcnNode != nullptr) {
        auto fcnRes = generateFcn(fcnNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!fcnRes.success) return fcnRes;
        currentRes = fcnRes.value.value();

        fcnNode = fcnNode->right;
    }

    return Result<CodeResult>::Ok(currentRes);    
}

/**
 * Our convention is that the CALLER must PUSH arguments to the stack before calling
 * the function (callee). I.e., it then the responsibility of the callee (function) 
 * to pop arguments off the stack and save them mapped to relevant parameter names.
 */
Result<CodeResult> CodeGenerator::generateFcn(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("Generating Fcn code.");
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    TreeNode *nameNode = node->left;
    std::string fcnName = nameNode->left->value; 

    // Save the function's starting instruction address
    functionAddresses[fcnName] = currentRes.nextInstruction;

    // Re-enter function scope so we map to correct local variables
    Symbol* fcnSym = symbolTable.lookup(fcnName);
    if (fcnSym) {
        symbolTable.reenterScope(fcnSym->scopeIndex);
    } // errors?

    TreeNode *paramsNode = nameNode->right;
    // Below three (type, consts, types) are not needed but kept commented for clarity of traversal
    // TreeNode* typeNameNode = paramsNode->right;
    // TreeNode* constsNode = paramsNode->right->right;
    // TreeNode* typesNode = paramsNode->right->right->right;
    TreeNode *dclnsNode = paramsNode->right->right->right->right;
    TreeNode *bodyNode = dclnsNode->right;

    // The Prologue - map function arguments pushed on the stack into absolute memory addresses
    std::vector<std::string> paramNames;
    if (paramsNode->value == "params") {
        TreeNode *dclnNode = paramsNode->left;
        while (dclnNode != nullptr) {
            if (dclnNode->value == "var") {
                TreeNode *identNode = dclnNode->left;
                while (identNode != nullptr && identNode->right != nullptr) {
                    paramNames.push_back(identNode->left->value);
                    identNode = identNode->right;
                    // NOTE: Final sibling is the type. Since iteration condition checks
                    // (identNode->right != nullptr), final node won't be iterated
                    // over, making sure type is not added to the paramNames 
                }
            }
            dclnNode = dclnNode->right; // each sibling is a 'var' node
        }
    }

    // The Prologue - establish this function's activation frame.
    // The caller pushed the arguments onto the operand stack; `enter` rebases the
    // frame pointer so those args BECOME the first frame slots (params 0..argc-1),
    // and `reserve` lifts the operand stack above the function's local variables.
    int argc = static_cast<int>(paramNames.size());
    int nlocals = (fcnSym ? symbolTable.scopeLocalCount(fcnSym->scopeIndex) : argc);
    int nvars = nlocals - argc; // non-parameter locals

    emit("enter", argc);
    currentRes.nextInstruction++;
    if (nvars > 0) {
        emit("reserve", nvars);
        currentRes.nextInstruction++;
    }
    // The args are now in their frame slots and the operand stack is empty.
    currentRes.stackPointer = 0;

    // Generate function body
    auto bodyRes = generateBody(bodyNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!bodyRes.success) return bodyRes;
    currentRes = bodyRes.value.value();

    // Fallback path: reached only if the body falls off the end without an
    // explicit return. A value-returning function must still leave exactly one
    // value on the stack so the caller's consume (e.g. `save`) stays balanced;
    // otherwise the operand stack drifts on every call. Default to 0.
    emit("lit", 0);
    currentRes.stackPointer++;
    currentRes.nextInstruction++;
    emit("return");
    currentRes.nextInstruction++;

    // clean up the scope
    if (fcnSym) {
        symbolTable.exitScope();
    }

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Statement -> 'return' Expression
 * Evaluate expression, assuming expression will leave result on top of stack.
 */
Result<CodeResult> CodeGenerator::generateReturnStatement(TreeNode *node, CodeInput input) {
    LOG_DEBUG("Generating return statement");

    // evaluate return expression
    auto exprRes = generateExpression(node->left, input);
    if (!exprRes.success) return exprRes;
    CodeResult currentRes = exprRes.value.value();

    emit("return");
    currentRes.nextInstruction++;

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * 
 */
Result<CodeResult> CodeGenerator::generateBody(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("Generating code for program body with node value: " + node->value);
    TreeNode *current = node->left; // First child is the first statement
    CodeInput currentInput = input;
    CodeResult lastResult(input.stackPointer, input.nextInstruction); // Create CodeResult struct with previous generation functions outputs

    // Iterate across multiple (possible) statements in the body
    while (current != nullptr) {
        auto statementCode = generateStatement(current, currentInput);
        if (!statementCode.success) {
            return statementCode; // pass error up
        }

        // update tracking frames for next `Statement`
        lastResult = statementCode.value.value();
        currentInput.stackPointer = lastResult.stackPointer;
        currentInput.nextInstruction = lastResult.nextInstruction;

        current = current->right; // move to next sibling
    }

    return Result<CodeResult>::Ok(lastResult);
}

// Statement could be output, if, while, repeat, for, loop, case, read, exit, 
// return, and null
// TODOs have
Result<CodeResult> CodeGenerator::generateStatement(TreeNode *node, CodeInput input)
{
    if (node == nullptr) return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
    
    if (node->value == "output") {
        return generateOutputStatement(node, input);
    } else if (node->value == "<null>") {
        // no-op statement
        return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
    } else if (node->value == "assign") {
        return generateAssignment(node, input);
    } else if (node->value == "swap") {
        return generateSwap(node, input);
    } else if (node->value == "if") {
        return generateIfStatement(node, input);
    } else if (node->value == "while") {
        return generateWhileStatement(node, input);
    } else if (node->value == "read") {
        return generateReadStatement(node, input);
    } else if (node->value == "repeat") {
        return generateRepeatStatement(node, input);
    } else if (node->value == "for") {
        return generateForStatement(node, input);
    } else if (node->value == "case") {
        return generateCaseStatement(node, input);
    } else if (node->value == "loop") {
        return generateLoopStatement(node, input);
    } else if (node->value == "exit") {
        return generateExitStatement(node, input);
    } else if (node->value == "return") {
        return generateReturnStatement(node, input);
    } else if (node->value == "block") {
        return generateBody(node, input);
    }

    LOG_ERROR("Unsupported statement node type: " + node->value);
    return Result<CodeResult>::Err(CodeGeneratorError("Unsupported statement node type: " + node->value));
}

/**
 * Generate output statement
 */
Result<CodeResult> CodeGenerator::generateOutputStatement(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("Generating output statement.");
    TreeNode *current = node->left; // First child wrapped in 'string' or 'integer'
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // loop through comma separated expressions in output statement
    while (current != nullptr) {
        if (current->value == "string") {
            auto strRes = generateString(current, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
            if (!strRes.success) return strRes; // generateString creates the error
            currentRes = strRes.value.value();

            // emit custom prints instruction
            emit("prints");
            currentRes.stackPointer--;
            currentRes.nextInstruction++;
        } else if (current->value == "integer") {
            // actual expression is left child of 'integer' wrapper node
            auto exprRes = generateExpression(current->left, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
            if (!exprRes.success) return exprRes;
            currentRes = exprRes.value.value();

            // emit standard integer-print instruction
            emit("print");
            currentRes.stackPointer--;
            currentRes.nextInstruction++;
        }
        current = current->right;
    }                            
    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Generate string for printing
 */
Result<CodeResult> CodeGenerator::generateString(TreeNode *node, CodeInput input)
{
    LOG_DEBUG("Generating code for string");
    TreeNode *stringLiteralNode = node->left;
    std::string stringValue = stringLiteralNode->left->value;

    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    emit("lits", stringValue);

    currentRes.stackPointer++; // pushes literal on to the stack
    currentRes.nextInstruction++;

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Expression could have simple leaf node, unary operator, or binary operators
 * recursing down into more expressions. Recursive approach
 */
Result<CodeResult> CodeGenerator::generateExpression(TreeNode *node, CodeInput input)
{
    if (node == nullptr) return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
    
    // here we go. 

    CodeInput currentInput = input;
    LOG_DEBUG("Generating expression for node: " + node->value);
    
    // === 1. Base case - leaves
    if (node->value == "<integer>") {
        emit("lit", node->left->value);
        return Result<CodeResult>::Ok(CodeResult(currentInput.stackPointer + 1, currentInput.nextInstruction + 1));
    
    } else if (node->value == "<char>") {
        std::string val = node->left->value;
        // specification says characters are always between '', so first and last 
        // characters are therefore single-quotes and thus disregarded.
        int charAscii = (val.length() >= 3) ? static_cast<int>(val[1]) : 0;
        // if no character then 0 is default. 0 corresponds to ASCII null character
        emit("lit", charAscii);
        return Result<CodeResult>::Ok(CodeResult(currentInput.stackPointer + 1, currentInput.nextInstruction + 1));

    } else if (node->value == "<identifier>") {
        std::string name = node->left->value;
        Symbol *sym = symbolTable.lookup(name);

        if (sym == nullptr) return Result<CodeResult>::Err(CodeGeneratorError("Undeclared identifier: " + name));

        if (sym->kind == SymbolKind::Variable) {
            emitVarLoad(sym);
        } else if (sym->kind == SymbolKind::Constant) {
            emit("lit", sym->ordinal); // push constant value directly to stack
        }
        return Result<CodeResult>::Ok(CodeResult(currentInput.stackPointer + 1, currentInput.nextInstruction + 1));
    } else if (node->value == "eof" || node->value == "true") {
        emit("lit", 1);
        return Result<CodeResult>::Ok(CodeResult(currentInput.stackPointer + 1, currentInput.nextInstruction + 1));
    } else if (node->value == "false") {
        emit("lit", 0);
        return Result<CodeResult>::Ok(CodeResult(currentInput.stackPointer + 1, currentInput.nextInstruction + 1));
    } 
    // function call
    else if (node->value == "call") {
        std::string fcnName = node->left->left->value;

        // Evaluate arguments and push to stack - arguments exist as sibling Expression
        // tree nodes with parent call and left-most sibling being function-name (extracted above)
        TreeNode *argNode = node->left->right;
        CodeResult currentRes = CodeResult(currentInput.stackPointer, currentInput.nextInstruction);
        
        // NOTE: left-to-right traversal means top of the stack will contain
        // right-most Expression (function argument).
        while (argNode != nullptr) {
            // iterate along sibling Expression nodes
            auto argRes = generateExpression(argNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
            if (!argRes.success) return argRes;
            currentRes = argRes.value.value();
            
            argNode = argNode->right;
        }

        // verify whether function exists
        if (functionAddresses.find(fcnName) == functionAddresses.end()) {
            return Result<CodeResult>::Err(CodeGeneratorError("Call to undefined function: " + fcnName));
        }

        // emit a call instruction - call the function
        emit("call", functionAddresses[fcnName]);
        currentRes.nextInstruction++;

        // Calculate the overall stack change due to the call 
        // call consumes some 'n' arguments but pushes only 1 return value
        Symbol *sym = symbolTable.lookup(fcnName);
        if (sym && sym->kind == SymbolKind::Function) {
            currentRes.stackPointer = currentRes.stackPointer - sym->paramCount + 1;
        }

        return Result<CodeResult>::Ok(currentRes);
    }

    // === 2. Unary operators (one child)
    else if (node->left != nullptr && node->left->right == nullptr) {
        auto childRes = generateExpression(node->left, currentInput); // recursively call self until it gets to leaves
        if (!childRes.success) return childRes; // TODO hanle errors better
        CodeResult res = childRes.value.value();
        // by now result must be at the top of the VM stack
        if (node->value == "-") {
            emit("negate");
            res.nextInstruction++;
        } else if (node->value == "not") {
            emit("not");
            res.nextInstruction++;
        } else if (node->value == "succ") {
            // successor - successor of an ordinal value
            emit("lit", 1);
            emit("add");
            res.nextInstruction += 2;
        } else if (node->value == "pred") {
            // predecessor - predecessor of an ordinal value
            emit("lit", 1);
            emit("subtract");
            res.nextInstruction += 2;
        }
        return Result<CodeResult>::Ok(res);
    }

    // === 3. Binary operators (2 children)
    else if (node->left != nullptr && node->left->right != nullptr) {
        // left operand
        auto leftRes = generateExpression(node->left, currentInput);
        if (!leftRes.success) return leftRes;

        // right operand
        auto rightRes = generateExpression(node->left->right, CodeInput(leftRes.value.value().stackPointer, leftRes.value.value().nextInstruction));
        if (!rightRes.success) return rightRes;

        CodeResult res = rightRes.value.value();

        // ISA native instructions
        if (node->value == "+") emit("add");
        else if (node->value == "-") emit("subtract");
        else if (node->value == "=") emit("equal");
        else if (node->value == "<=") emit("lessequal");
        else if (node->value == "*") emit("multiply");
        else if (node->value == "/") emit("divide");
        else if (node->value == "mod") emit("mod");
        else if (node->value == "and") emit("and");
        else if (node->value == "or") emit("or");
        else if (node->value == "<") emit("lessthan");
        else if (node->value == ">") emit("greater");
        else if (node->value == ">=") emit("greaterequal");
        else if (node->value == "<>") emit("notequal");

        // a binary operation pops 2 from the stack and pushes one: i.e. net -1
        res.stackPointer--;
        res.nextInstruction++; // only one more instruction after left and right subtrees resolved

        return Result<CodeResult>::Ok(res);
    }
    return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
}

/**
 * Assignment statement. Calls generateExpression, assuming value is at top of stack
 * and emits `save <addr>` instruction for assigned variable
 */
Result<CodeResult> CodeGenerator::generateAssignment(TreeNode *node, CodeInput input) {
    LOG_DEBUG("Generating code for assignment with node value: " + node->value);
    TreeNode *identNode = node->left; // first child of assign is always an identifier (Name)
    TreeNode *exprNode = identNode->right; // sibling of identifier is an expression

    // Evaluate expression
    auto exprRes = generateExpression(exprNode, input); // assume this leaves the result of the expression on the stack
    if (!exprRes.success) return exprRes; // TODO handle errors 
    CodeResult currentRes = exprRes.value.value();

    // Lookup variable in symbol table
    std::string varName = identNode->left->value; 
    Symbol *sym = symbolTable.lookup(varName); // gives saved address for this specific symbol

    if (sym == nullptr || sym->kind != SymbolKind::Variable) {
        LOG_ERROR("Undeclared or invalid variable in assignment: " + varName);
        return Result<CodeResult>::Err(CodeGeneratorError("Invalid variable for assignment: " + varName));
    }

    // Save instruction using precalculated address
    emitVarSave(sym);

    currentRes.stackPointer--; // save instruction pops value off stack
    currentRes.nextInstruction++;

    return Result<CodeResult>::Ok(currentRes);    
}

/**
 * Use the LIFO property of the VM stack machine to execute a swap between two
 * variables.
 * Emits four instructions leaves stackPointer unchanged
 */
Result<CodeResult> CodeGenerator::generateSwap(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating code for swap with node value: " + node->value);
    TreeNode *identNode1 = node->left;
    TreeNode *identNode2 = identNode1->right;

    Symbol *sym1 = symbolTable.lookup(identNode1->left->value);
    Symbol *sym2 = symbolTable.lookup(identNode2->left->value);

    if (sym1 == nullptr || sym2 == nullptr) {
        return Result<CodeResult>::Err(CodeGeneratorError("Undeclared variable in swap"));
    }

    emitVarLoad(sym1); // stack: [... var1] (top)
    emitVarLoad(sym2); // stack: [... var1, var2] (top)
    emitVarSave(sym1); // pop var2 and save in address of var1
    emitVarSave(sym2); // pop var1 and save in address of var2

    // pushed two to stack, popped two from stack - stackPointer unchanged
    // emitted four additional instructions - nextInstruction += 4
    return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction + 4));
}

/**
 * Uses branch patching by directly referring to `generatedCode`
 * 
 * Basic structure in generated instructions:
 * {condition block}    (Expression)
 * iffalse ___          (should point to beginning of else-block)
 * {then block}         (Statement)
 * goto ___             (only if else-block exists; should point to immediately after else-block)
 * {else block}         (Statement)
 * 
 */
Result<CodeResult> CodeGenerator::generateIfStatement(TreeNode *node, CodeInput input) {
    LOG_DEBUG("Generating if-then-else statement");
    TreeNode *conditionNode = node->left;
    TreeNode *thenNode = conditionNode->right;
    TreeNode *elseNode = thenNode->right; // will be nullptr if not existing

    // 1. evaluate the condition - assume this ends with a boolean (0 or 1) on stack
    auto condRes = generateExpression(conditionNode, input);
    if (!condRes.success) return condRes; // generateExpression will create error
    CodeResult currentRes = condRes.value.value();

    // 2. emit a placeholder iffalse instruction - iffalse we jump away (where? don't know yet)
    int ifFalsePatchIndex = currentRes.nextInstruction;
    emit("iffalse -1"); // here -1 is a placeholder since we don't yet know where to jump to if false 
    currentRes.nextInstruction++;
    currentRes.stackPointer--; // iffalse will pop the stack once

    // 3. generate then-statement
    auto thenRes = generateStatement(thenNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!thenRes.success) return thenRes;
    currentRes = thenRes.value.value();

    if (elseNode != nullptr) {
        // 4. emit placeholder goto - after then-block we want to skip the else-block
        int gotoPatchIndex = currentRes.nextInstruction;
        emit("goto -1"); // here -1 is a placeholder - we don't know where to goto yet
        currentRes.nextInstruction++;

        // 5. patch the iffalse jump. It should jump HERE (i.e. to the start of the else-block)
        generatedCode[ifFalsePatchIndex - 1] = "iffalse " + std::to_string(currentRes.nextInstruction);

        // 6. Generate the else-statement
        auto elseRes = generateStatement(elseNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!elseRes.success) return elseRes; // generateStatement creates the error
        currentRes = elseRes.value.value();

        // 7. patch the goto jump. It should jump HERE, immediately after the else block
        generatedCode[gotoPatchIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);

    } else {
        // no else-block - we just patch the iffalse to point HERE to the end of the then-block 
        generatedCode[ifFalsePatchIndex - 1] = "iffalse " + std::to_string(currentRes.nextInstruction);
    }

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * While-do also uses branch patching similar to if-then-else statement
 * 
 * Basic structure of generated instructions:
 * {condition block}    (Expression)
 * iffalse ___          (jump to instr immediately after goto instr)
 * {loop body (do)}     (Statement)
 * goto ___             (jump back to the start of the condition-block)
 * 
 */
Result<CodeResult> CodeGenerator::generateWhileStatement(TreeNode *node, CodeInput input) {
    LOG_DEBUG("Generating while statement");
    TreeNode* conditionNode = node->left;
    TreeNode* doNode = conditionNode->right;

    // We mark the beginning of the loop to jump back
    int loopStartIndex = input.nextInstruction;

    // 1. Evaluate the condition
    auto condRes = generateExpression(conditionNode, input);
    if (!condRes.success) return condRes;
    CodeResult currentRes = condRes.value.value();

    // 2. Emit the placeholder iffalse
    int ifFalsePatchIndex = currentRes.nextInstruction;
    emit("iffalse -1");
    currentRes.nextInstruction++;
    currentRes.stackPointer--; // iffalse will pop from the stack

    // In case an `exit` statement is found inside the do-block
    exitPatchStack.push_back(std::vector<int>());
    // if no `exit`s are found, the above will be empty upon iteration (below)

    // 3. Generate code for the do-block (loop body)
    auto doRes = generateStatement(doNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!doRes.success) return doRes;
    currentRes = doRes.value.value();

    // 4. Jump back to the start of the condition-block
    emit("goto", loopStartIndex);
    currentRes.nextInstruction++;

    // In case there were exit statements
    std::vector<int> exitsToPatch = exitPatchStack.back();
    exitPatchStack.pop_back();
    for (int exitInstrIndex : exitsToPatch) {
        generatedCode[exitInstrIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);
    }
    // We patch the exits within the loop-body to point to the instruction immediately
    // after the loop's `goto LOOP_START` instruction.

    // 5. Patch the iffalse which jumps HERE - immediately after the goto
    generatedCode[ifFalsePatchIndex - 1] = "iffalse " + std::to_string(currentRes.nextInstruction);

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Statement -> 'read' '(' Name list ',' ')'
 * For a list of reads we need to take input from user for each identifier and
 * store its value mapped to the corresponding identifier.
 * 
 * Parser builds subtree such that parent is 'read' and children are identifiers 
 * in the list.
 */
Result<CodeResult> CodeGenerator::generateReadStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating read statement.");
    TreeNode* current = node->left; // first identifer
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // iterate across sibling identifier (if any)
    while (current != nullptr) {
        // Emit read instruction (pushes user input to the stack)
        emit("read");
        currentRes.stackPointer++;
        currentRes.nextInstruction++;

        // Lookup variable and save inputted value
        std::string varName = current->left->value;
        Symbol *sym = symbolTable.lookup(varName);

        if (sym == nullptr || sym->kind != SymbolKind::Variable) {
            return Result<CodeResult>::Err(CodeGeneratorError("Invalid variable for read: " + varName));
        }
        // save the value to the variable
        emitVarSave(sym);
        currentRes.stackPointer--;
        currentRes.nextInstruction++;

        current = current->right; // move to the next identifier sibling
    }

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * repeat...until loops always run the body at least once. Condition is evaluated
 * at the end of the loop-body. 
 * REPEAT UNTIL TRUE - when condition is TRUE we jump OUT of the loop
 * 
 * Basic instruction structure as follows:
 * {Loop-body}   (loop-body consists of multiple Statement blocks)
 * {Condition}   (Expression)
 * iffalse ___   (jump back to the start of the loop-body)
 * 
 */
Result<CodeResult> CodeGenerator::generateRepeatStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating code for the repeat until statement");

    TreeNode* current = node->left;
    // mark the start of the loop
    int loopStartIndex = input.nextInstruction;
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // to facilitate any 'exit' statements found within the loop-body
    exitPatchStack.push_back(std::vector<int>());

    // iterate through the Statements
    for ( /* advertisement space */ ; current->right != nullptr ; current = current->right) {
        auto stmtRes = generateStatement(current, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!stmtRes.success) return stmtRes; // generateStatement creates the required error
        currentRes = stmtRes.value.value();
    }

    // when loop breaks, `current` will be the condition node (i.e., current->right will be nullptr)
    auto condRes = generateExpression(current, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!condRes.success) return condRes;
    currentRes = condRes.value.value();

    // iffalse - jump back to the start of the loop-body. we loop UNTIL TRUE
    emit("iffalse", loopStartIndex);
    currentRes.stackPointer--; // iffalse pops from the stack
    currentRes.nextInstruction++;

    // Patch jumps generated by any 'exit' statements found within loop-body
    std::vector<int> exitsToPatch = exitPatchStack.back();
    exitPatchStack.pop_back();
    for (int exitInstrIndex : exitsToPatch) {
        generatedCode[exitInstrIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);
    }

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * for ({init} ; {condition} ; {update}) Statement
 * 
 * Basic instruction structure:
 * {init block}         (only runs once) (ForStat -> Assignment | <null>)
 * {condition block}    (actual start of the loop) (ForExp -> Expression | <null>)
 * iffalse ___          (jump below the goto)
 * {loop-body}          (Statement)
 * {update}             (ForStat -> Assignment | <null>)
 * goto ___             (jump to the start of the condition block)
 * 
 */
Result<CodeResult> CodeGenerator::generateForStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating code for a for-statement.");
    TreeNode *initNode = node->left;
    TreeNode *condNode = initNode->right;
    TreeNode *updateNode = condNode->right;
    TreeNode *loopNode = updateNode->right;

    // To track and facilitate any 'exit' instructions found within the loop-body
    exitPatchStack.push_back(std::vector<int>());

    // initialization
    // technically this is only either swap OR assignment OR <null>. However,
    // `generateStatement` handles all of this anyway so we can use that instead
    // a long if-elseif-elseif statement
    auto initRes = generateStatement(initNode, input); 
    if (!initRes.success) return initRes; 
    CodeResult currentRes = initRes.value.value();

    // mark start of the loop - just before condition check
    int loopStartIndex = currentRes.nextInstruction;

    // evaluate the condition
    auto condRes = generateExpression(condNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!condRes.success) return condRes;
    currentRes = condRes.value.value();

    // emit the iffalse with a placeholder jump
    int ifFalsePatchIndex = currentRes.nextInstruction;
    emit("iffalse -1");
    currentRes.stackPointer--;
    currentRes.nextInstruction++;

    // loop body
    auto loopRes = generateStatement(loopNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!loopRes.success) return loopRes;
    currentRes = loopRes.value.value();

    // update section
    auto updateRes = generateStatement(updateNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
    if (!updateRes.success) return updateRes;
    currentRes = updateRes.value.value();

    // goto statement - jump back to the top of the condition evaluation section
    emit("goto", loopStartIndex);
    currentRes.nextInstruction++;

    // Patch any jumps from any 'exit' instructions found within loop-body
    std::vector<int> exitsToPatch = exitPatchStack.back();
    exitPatchStack.pop_back();
    for (int exitInstrIndex : exitsToPatch) {
        generatedCode[exitInstrIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);
    }
    
    // patch the iffalse to allow it to jump HERE - immediately after goto
    generatedCode[ifFalsePatchIndex - 1] = "iffalse " + std::to_string(currentRes.nextInstruction);

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Evaluates the Expression repeatedly, before each CaseExpression
 * 
 * Consider an example case-staement as follows:
 *  case Expression of
 *      ConstValue-1a, ConstValue-1b: Statement-1;
 *      ConstValue-2a..ConstValue-2b, ConstValue-2c: Statement-2;
 *  otherwise Statement;
 * 
 * General structure of instructions generated for above example:
 *  {Expression}             (the expression compared against each case)
 *  {CaseExpression-1a}      (CaseExpression -> ConstValue-1a)
 *  iftrue Statment-1        (if above)
 *  {Expression}
 *  {CaseExpression-1b}      (CaseExpression -> ConstValue)
 *  iftrue Statement-1
 *  {Expression}
 *  {CaseExpression-2a}      (CaseExpression -> ConstValue..ConstValue (left constvalue))
 *  greaterequal
 *  {Expression}
 *  {CaseExpression-2b}      (CaseExpression -> ConstValue..ConstValue (right constvalue))
 *  lessequal
 *  and
 *  iftrue Statement-2
 *  {Expression}
 *  {CaseExpression-2c}
 *  iftrue Statement-2
 *  goto DEFAULT             (this goto executes only if none of preceding cases matched. either points to 'otherwise' or beyond entire case-block)
 *  {Statement-1}
 *  goto END                 (skip all the other Statement blocks and jump to the instruction beyond entire case-block)
 *  {Statement-2}
 *  goto END
 *  {Statement-3}
 *  goto END
 *  {Otherwise Statement}    (optional. If exists, 'goto DEFAULT' points here)
 * 
 */
Result<CodeResult> CodeGenerator::generateCaseStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating code for a case-statement.");

    TreeNode *caseExprNode = node->left;
    TreeNode *currentClause = caseExprNode->right;
    TreeNode *otherwiseClause = nullptr;

    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // each case Statement needs to unconditionally jump to the very end once it
    // finishes. We will only know the 'end' instruction once everything is generated
    // So we use `endJumps` to keep track of all the 'goto End' instances to patch later
    std::vector<int> endJumps;

    // to pair clause Statements with their incoming jump indices
    std::vector<std::pair<TreeNode*, std::vector<int>>> clauseBodies;

    // 1. Generate condition check
    while (currentClause != nullptr) {
        if (currentClause->value == "case_clause") {
            TreeNode *labelNode = currentClause->left;
            std::vector<int> bodyJumps; // track all `iftrue` jumps pointing to this specific Statement body

            // loop through the labels (<int>, <chr>, <..>, etc.). Last node will be Statement
            while (labelNode->right != nullptr) {

                if (labelNode->value == "..") {
                    // means we have a RANGE match on our hands (Exp >= left) AND (Exp <= Right)
                    // Check left-bound
                    auto exp1 = generateExpression(caseExprNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
                    currentRes = exp1.value.value();
                    auto leftBound = generateExpression(labelNode->left, CodeInput(currentRes.stackPointer, currentRes.nextInstruction)); 
                    // (above) technically not an Expression, but
                    //  base case of generateExpression handles all possible scenarios
                    currentRes = leftBound.value.value();
                    // now the constvalue on the left bound will be on top of stack
                    emit("greaterequal"); 
                    currentRes.stackPointer--; currentRes.nextInstruction++;

                    // Check right-bound
                    auto exp2 = generateExpression(caseExprNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
                    currentRes = exp2.value.value();
                    auto rightBound = generateExpression(labelNode->left->right, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
                    currentRes = rightBound.value.value();
                    // by now constvalue of right-bound is at top of stack
                    emit("lessequal");
                    currentRes.stackPointer--; currentRes.nextInstruction++;

                    // Both evaluations (greaterequal left-bound AND lessequal right-bound)
                    // need to evaluate to true for Statement to be chosen. 
                    // These evaluataions are at the top two positions of the stack.
                    emit("and"); 
                    // pop two items from stack, do binary-AND, push result on stack
                    currentRes.stackPointer--; currentRes.nextInstruction++;
                } else {
                    // means we have an exact match: Exp == Label
                    auto exp = generateExpression(caseExprNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
                    currentRes = exp.value.value();
                    auto label = generateExpression(labelNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
                    currentRes = label.value.value();
                    // Top-two positions in stack is constvalue and evaluation of 
                    // expression respectively. We check for equality
                    emit("equal");
                    currentRes.stackPointer--; currentRes.nextInstruction++;
                }
                // If above comparison leaves a `true` on stack, we jump to the
                // corresponding Statement. 
                // So we emit a `iftrue` but leave the Statement address to be patched
                bodyJumps.push_back(currentRes.nextInstruction);
                emit("iftrue -1"); // Placeholder
                currentRes.stackPointer--; // iftrue will pop the stack
                currentRes.nextInstruction++;

                labelNode = labelNode->right; // iterate
            }
            // We've broken out of the loop implies `labelNode` is now Statement
            clauseBodies.push_back( {labelNode, bodyJumps} );
            // This pair shows that for the `labelNode` Statement block, there 
            // are `iftrue` instructions at `bodyJumps` index positions of 
            // `generatedCode` which need to be patched with the actual start 
            // instruction index of the `labelNode` Statement. -- NOTE: We haven't
            // generated `labelNode` Statement instructions yet.

        } else if (currentClause->value == "otherwise") {
            otherwiseClause = currentClause;
        }
        currentClause = currentClause->right; // iterate to the next sibling
    }
    // NOTE: at the end of the above while loop `currentClause` now points to 
    // the otherwise clause if it exists

    // We have now generated all the code for the case clause checks and jumps. 
    // However, the Statement blocks of each remain.

    int defaultJumpIndex = currentRes.nextInstruction;
    emit("goto -1"); // basically 'goto DEFAULT'
    currentRes.nextInstruction++;
    // This `goto` happens only if none of the previous `iftrue` instructions 
    // were taken. Therefore this `goto` should point to the Otherwiseclause or 
    // (if otherwise isn't there) to the very end of all the generated Statement 
    // blocks.

    // Generating clause bodies
    for (auto& clause : clauseBodies) {
        TreeNode *stmtNode = clause.first; // first in the pair (node)
        std::vector<int>& jumps = clause.second;

        // patch all the `iftrue` instructions that need to jump to this
        for (int jumpIndex : jumps) {
            generatedCode[jumpIndex - 1] = "iftrue " + std::to_string(currentRes.nextInstruction);
        }

        // Generate the body Statement
        auto stmtRes = generateStatement(stmtNode, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!stmtRes.success) return stmtRes;
        currentRes = stmtRes.value.value();
        // Now we need a 'goto End' which has to be patched later.
        endJumps.push_back(currentRes.nextInstruction);
        emit("goto -1");
        currentRes.nextInstruction++;
    }

    // Generate the OTHERWISE block if it exists
    // The 'goto DEFAULT' jump above has to be patched to point HERE
    generatedCode[defaultJumpIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);

    if (otherwiseClause != nullptr) {
        auto otherwRes = generateStatement(otherwiseClause->left, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!otherwRes.success) return otherwRes;
        currentRes = otherwRes.value.value();
    }

    // FINALLY we patch all the end jumps from the Statement bodies to end up here
    for (int jumpIndex : endJumps) {
        generatedCode[jumpIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);
    }

    return Result<CodeResult>::Ok(currentRes);
}

/**
 * Checks whether we're in a loop, if so outputs a single placeholder
 * 'goto' instruction and register's its index in `generatedCode` in order
 * to patch it later - we don't know where the end of the loop is yet.
 */
Result<CodeResult> CodeGenerator::generateExitStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating exit statement");

    // rain-check: are we inside a loop?
    if (exitPatchStack.empty()) {
        LOG_ERROR("Encountered 'exit' outside of a loop.");
        return Result<CodeResult>::Err(CodeGeneratorError("Exit statement used outside of loop"));
    }

    // emit placeholder jump
    int exitJumpIndex = input.nextInstruction;
    emit("goto -1"); // we don't know where the end of the loop is yet
    input.nextInstruction++;

    // register jump to be patched in the future
    exitPatchStack.back().push_back(exitJumpIndex);

    return Result<CodeResult>::Ok(CodeResult(input.stackPointer, input.nextInstruction));
}

/**
 * Generate instructions for loop statements. 
 * Also handles exiting via 'exit' statements and tracking loop nesting using 
 * CodeGenerator::exitPatchStack
 */
Result<CodeResult> CodeGenerator::generateLoopStatement(TreeNode* node, CodeInput input) {
    LOG_DEBUG("Generating loop..pool statement");

    // Add this loop to the back of the exit-tracking vector. Any 'exit's that 
    // take control-flow out of this specific loop will therefore be added as 
    // members of the std::vector<int>() array initialized below.
    exitPatchStack.push_back(std::vector<int>());

    int loopStartIndex = input.nextInstruction; // mark top of loop
    CodeResult currentRes = CodeResult(input.stackPointer, input.nextInstruction);

    // Generate code for the loop-body statements
    TreeNode *currentStmt = node->left;
    while (currentStmt != nullptr) {
        auto stmtRes = generateStatement(currentStmt, CodeInput(currentRes.stackPointer, currentRes.nextInstruction));
        if (!stmtRes.success) return stmtRes;
        currentRes = stmtRes.value.value();

        currentStmt = currentStmt->right;
    }

    // unconditionally jump back to the top
    emit("goto", loopStartIndex);
    currentRes.nextInstruction++;

    // retrieve all the 'exit' statements that came up inside the statement blocks
    std::vector<int> exits = exitPatchStack.back();
    exitPatchStack.pop_back();

    for (int patchIndex : exits) {
        generatedCode[patchIndex - 1] = "goto " + std::to_string(currentRes.nextInstruction);
    }

    return Result<CodeResult>::Ok(currentRes);
}