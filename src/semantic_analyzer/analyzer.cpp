#include "semantic_analyzer/analyzer.h"

SemanticAnalyzer::SemanticAnalyzer(TreeNode *ast) : ast(ast), symbolTable()
{
}

SemanticAnalyzer::~SemanticAnalyzer()
{
    // The destructor does not need to delete `ast` because it is owned by the caller.
}

bool SemanticAnalyzer::isIntegerLiteral(TreeNode *node)
{
    return node != nullptr && node->value == "<integer>";
}

bool SemanticAnalyzer::isCharLiteral(TreeNode *node)
{
    return node != nullptr && node->value == "<char>";
}

Result<void> SemanticAnalyzer::analyze()
{
    std::cout << "Semantic analysis successful!" << std::endl;
    std::cout << "Abstract Syntax Tree:" << std::endl;
    analyzeProgram(ast);
    symbolTable.printAllScopes(); // Print all scopes for debugging
    return Result<void>::Ok();
}

int SemanticAnalyzer::countChildren(TreeNode *node)
{
    TreeNode *current = node;
    int count = 0;
    while (current != nullptr)
    {
        count++;
        current = current->right; // Move to the next sibling
    }
    return count;
}

void SemanticAnalyzer::analyzeProgram(TreeNode *node)
{
    TreeNode *name = node->left;       // First child is program name
    TreeNode *consts = name->right;    // First sibling is const declarations
    TreeNode *types = consts->right;   // Next sibling is type declarations
    TreeNode *dclns = types->right;    // Next sibling is variable and function declarations
    TreeNode *subprogs = dclns->right; // Next sibling is subprogram declarations
    if (subprogs->right == nullptr)
    {
        LOG_ERROR("Program body is missing.");
        return;
    }
    TreeNode *body = subprogs->right; // Next sibling is the body of the program
    TreeNode *endName = body->right;  // Next sibling is the end name of the program
    if (name->left->value != endName->left->value)
    {
        LOG_ERROR("Program name '" + name->left->value + "' end name '" + endName->left->value + "' does not match.");
        return;
    }
    analyzeConsts(consts);
    analyzeTypes(types);
    analyzeDclns(dclns);
    analyzeSubprogs(subprogs);
    analyzeBody(body);
}

void SemanticAnalyzer::analyzeConsts(TreeNode *node)
{
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_INFO("No constants declared.");
        return;
    }
    current = current->left; // Move to the first constant declaration
    while (current != nullptr)
    {
        analyzeConst(current);    // Analyze each constant declaration
        current = current->right; // Move to the next sibling
    }
    LOG_INFO("Finished analyzing constants.");
}

void SemanticAnalyzer::analyzeConst(TreeNode *node)
{
    TreeNode *identifierNode = node->left; // First child is the identifier
    Symbol constSymbol;
    constSymbol.name = identifierNode->left->value; // The actual constant name is the left child of the identifier node
    constSymbol.kind = SymbolKind::Constant;
    auto isDefined = symbolTable.lookup(constSymbol.name);
    if (isDefined != nullptr)
    {
        LOG_ERROR("Constant '" + constSymbol.name + "' is already declared in the current scope.");
        return;
    }
    TreeNode *constValue = identifierNode->right; // Next sibling is the constant value
    if (isIntegerLiteral(constValue))
    {
        constSymbol.type = SymbolType::Integer;
        constSymbol.ordinal = std::stoi(constValue->left->value); // Assign the integer value as ordinal
    }
    else if (isCharLiteral(constValue))
    {
        constSymbol.type = SymbolType::Char;
        constSymbol.ordinal = static_cast<int>(constValue->left->value[0]); // Assign the ASCII value of the character as ordinal
    }
    else
    {
        auto typeSymbol = symbolTable.lookup(constValue->left->value);
        if (typeSymbol == nullptr)
        {
            LOG_ERROR("Type '" + constValue->left->value + "' for constant '" + constSymbol.name + "' is not declared.");
            return;
        }
        constSymbol.type = typeSymbol->type;
        constSymbol.ordinal = typeSymbol->ordinal; // Assign the ordinal value from the type symbol
    }
    if (!symbolTable.declare(constSymbol))
    {
        LOG_ERROR("Constant '" + constSymbol.name + "' is already declared in the current scope.");
        return;
    }
    LOG_INFO("Declared constant '" + constSymbol.name + "' of type '" + symbolTypeToString(constSymbol.type) + "'.");
    return;
}

void SemanticAnalyzer::analyzeTypes(TreeNode *node)
{
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_INFO("No types declared.");
        return;
    }
    current = current->left; // Move to the first type declaration
    while (current != nullptr)
    {
        analyzeType(current);     // Analyze each type declaration
        current = current->right; // Move to the next sibling
    }
}

void SemanticAnalyzer::analyzeType(TreeNode *node)
{
    TreeNode *identifierNode = node->left;              // First child is the identifier
    TreeNode *literalListNode = identifierNode->right;  // Next sibling is the literal list
    std::string typeName = identifierNode->left->value; // The actual type name is the left child of the identifier node
    Symbol typeSymbol;
    typeSymbol.name = typeName;
    typeSymbol.kind = SymbolKind::Type;
    typeSymbol.type = SymbolType::UserDefined;
    TreeNode *currentLiteral = literalListNode->left; // First child of literal list is the first literal
    while (currentLiteral != nullptr)
    {
        typeSymbol.members.push_back(currentLiteral->left->value); // Add the literal to the list
        currentLiteral = currentLiteral->right;                    // Move to the next sibling
    }
    if (!symbolTable.declare(typeSymbol))
    {
        LOG_ERROR("Type '" + typeName + "' is already declared in the current scope.");
        return;
    }
    int ordinalValue = 0;
    for (const auto &member : typeSymbol.members)
    {
        Symbol memberSymbol;
        memberSymbol.name = member;
        memberSymbol.kind = SymbolKind::Constant; // Members of a user-defined type are treated as constants
        memberSymbol.type = SymbolType::UserDefined;
        memberSymbol.typeName = typeName;      // Set the type name for the member
        memberSymbol.ordinal = ordinalValue++; // Assign an ordinal value to the member
        if (!symbolTable.declare(memberSymbol))
        {
            LOG_ERROR("Member '" + member + "' of type '" + typeName + "' is already declared in the current scope.");
            return;
        }
    }
    LOG_INFO("Declared type '" + typeName + "' with literals: " + std::to_string(typeSymbol.members.size()));
    return;
}

void SemanticAnalyzer::analyzeDclns(TreeNode *node)
{
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_INFO("No declarations made.");
        return;
    }
    LOG_INFO("Node value for declarations: " + current->value);
    current = current->left; // Move to the first declaration
    while (current != nullptr)
    {
        analyzeDcln(current);     // Analyze each declaration
        current = current->right; // Move to the next sibling
    }
}

void SemanticAnalyzer::analyzeDcln(TreeNode *node)
{
    LOG_INFO("Analyzing declaration: " + node->value);
    TreeNode *current_node = node->left; // First child is the one of the identifiers
    TreeNode *typeNode = nullptr;        // Last sibling is the type node
    int identifierCount = 0;
    while (current_node != nullptr)
    {
        identifierCount++;
        typeNode = current_node;            // Update type node
        current_node = current_node->right; // Move to the next sibling
    }
    LOG_INFO("Type node value: " + typeNode->left->value);
    TreeNode *variable_node = node->left; // First child is the first variable identifier
    for (int i = 0; i < identifierCount - 1; i++)
    {
        Symbol varSymbol;
        varSymbol.name = variable_node->left->value; // The actual variable name is the left child of the identifier node
        varSymbol.kind = SymbolKind::Variable;
        varSymbol.type = Symbol::getSymbolType(typeNode->left->value);
        if (varSymbol.type == SymbolType::UserDefined)
        {
            auto typeSymbol = symbolTable.lookup(typeNode->left->value);
            if (typeSymbol == nullptr)
            {
                LOG_ERROR("Type '" + typeNode->left->value + "' for variable '" + varSymbol.name + "' is not declared.");
                return;
            }
            varSymbol.typeName = typeNode->left->value;
        }
        if (!symbolTable.declare(varSymbol))
        {
            LOG_ERROR("Variable '" + varSymbol.name + "' is already declared in the current scope.");
            return;
        }
        LOG_INFO("Declared variable '" + varSymbol.name + "' of type '" + symbolTypeToString(varSymbol.type) + "'.");
        variable_node = variable_node->right; // Move to the next sibling
    }
}

void SemanticAnalyzer::analyzeSubprogs(TreeNode *node)
{
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_INFO("No subprograms declared.");
        return;
    }
    LOG_INFO("Node value for subprograms: " + current->value);
    current = current->left; // Move to the first subprogram declaration
    while (current != nullptr)
    {
        analyzeFcn(current);      // Analyze each subprogram declaration
        current = current->right; // Move to the next sibling
    }
    // Implementation for analyzing subprograms (functions/procedures)
}

void SemanticAnalyzer::analyzeFcn(TreeNode *node)
{
    TreeNode *identifierNode = node->left;        // First child is the function name
    TreeNode *paramsNode = identifierNode->right; // Next sibling is the parameters node
    TreeNode *returnTypeNode = paramsNode->right; // Next sibling is the return type node
    TreeNode *constsNode = returnTypeNode->right; // Next sibling is the constants node
    TreeNode *typesNode = constsNode->right;      // Next sibling is the types node
    TreeNode *dclnsNode = typesNode->right;       // Next sibling is the declarations node
    TreeNode *bodyNode = dclnsNode->right;        // Next sibling is the body of the function
    TreeNode *endNameNode = bodyNode->right;      // Next sibling is the end name of the function
    if (identifierNode->left->value != endNameNode->left->value)
    {
        LOG_ERROR("Function '" + identifierNode->left->value + "' end name '" + endNameNode->left->value + "' does not match.");
        return;
    }
    // Declare the function in the enclosing scope BEFORE analyzing its body,
    // so recursive calls inside the body can resolve the function name.
    Symbol funcSymbol;
    funcSymbol.name = identifierNode->left->value; // The actual function name is the left child of the identifier node
    funcSymbol.kind = SymbolKind::Function;
    funcSymbol.type = Symbol::getSymbolType(returnTypeNode->left->value);
    funcSymbol.paramCount = countChildren(paramsNode->left); // Count the number of parameters
    if (!symbolTable.declare(funcSymbol))
    {
        LOG_ERROR("Function '" + funcSymbol.name + "' is already declared in the current scope.");
        return;
    }

    int functionScope = symbolTable.enterScope(); // Enter a new scope for the function
    // Anchor the body scope onto the function symbol so later passes (code gen) can re-enter it.
    if (Symbol *declared = symbolTable.lookup(funcSymbol.name))
    {
        declared->scopeIndex = functionScope;
    }
    analyzeParams(paramsNode);
    analyzeConsts(constsNode);
    analyzeTypes(typesNode);
    analyzeDclns(dclnsNode);
    analyzeBody(bodyNode);
    symbolTable.exitScope(); // Leave the function scope

    LOG_INFO("Declared function '" + funcSymbol.name + "' with return type '" + symbolTypeToString(funcSymbol.type) + "' and " + std::to_string(funcSymbol.paramCount) + " parameters.");
    return;
}

void SemanticAnalyzer::analyzeParams(TreeNode *node)
{
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_ERROR("No parameters declared."); // looks like this language requires parameters for functions, so this is an error
        return;
    }
    LOG_INFO("Node value for parameters: " + current->value);
    current = current->left; // Move to the first parameter declaration
    while (current != nullptr)
    {
        analyzeDcln(current);     // Parameters have the same structure as variable declarations, so we can reuse that analysis
        current = current->right; // Move to the next sibling
    }
    // Implementation for analyzing function parameters
}

void SemanticAnalyzer::analyzeBody(TreeNode *node)
{
    // Implementation for analyzing the body of a function or the main program
    TreeNode *current = node;
    if (current->left == nullptr)
    {
        LOG_ERROR("No statements in body."); // looks like this language requires at least one statement in the body, so this is an error
        return;
    }
    LOG_INFO("Node value for body: " + current->value);
    current = current->left; // Move to the first statement
    while (current != nullptr)
    {
        // Analyze each statement based on its type (e.g., output statement, assignment, etc.)
        analyzeStatement(current);
        current = current->right; // Move to the next sibling
    }
}

void SemanticAnalyzer::analyzeStatement(TreeNode *node)
{
    if (node->value == "output")
    {
        analyzeOutputStatement(node->left); // Since rule exsists go to next level in the tree
    }
    else if (node->value == "if")
    {
        LOG_INFO("Analyzing if statement.");
        int childCount = countChildren(node->left);
        TreeNode *conditionNode = node->left;      // First child is the condition
        TreeNode *thenNode = conditionNode->right; // Next sibling is the 'then' part
        SemanticType conditionType = analyzeExpression(conditionNode);
        if (conditionType != SemanticType::Boolean)
        {
            LOG_ERROR("Condition in 'if' statement must be of boolean type.");
            return;
        }
        analyzeStatement(thenNode);
        if (childCount > 2)
        {
            // Handle the 'else' part if it exists
            TreeNode *elseNode = thenNode->right; // Next sibling is the 'else' part
            analyzeStatement(elseNode);
        }
        LOG_INFO("Finished analyzing if statement.");
    }
    else if (node->value == "while")
    {
        LOG_INFO("Analyzing while statement.");
        TreeNode *conditionNode = node->left;      // First child is the condition
        TreeNode *bodyNode = conditionNode->right; // Next sibling is the body of the while loop
        SemanticType conditionType = analyzeExpression(conditionNode);
        if (conditionType != SemanticType::Boolean)
        {
            LOG_ERROR("Condition in 'while' statement must be of boolean type.");
            return;
        }
        analyzeStatement(bodyNode);
        LOG_INFO("Finished analyzing while statement.");
    }
    else if (node->value == "repeat")
    {
        LOG_INFO("Analyzing repeat statement.");
        int childCount = countChildren(node->left); // Count how many children the repeat statement has
        TreeNode *temp = node->left;
        while (temp->right != nullptr)
        {
            temp = temp->right; // Move to the next sibling
        }
        TreeNode *untilNode = temp; // The last child is the 'until' condition
        SemanticType untilType = analyzeExpression(untilNode);
        if (untilType != SemanticType::Boolean)
        {
            LOG_ERROR("Condition in 'repeat' statement must be of boolean type.");
            return;
        }
        TreeNode *bodyNode = node->left; // First child is the body of the repeat loop
        for (int i = 0; i < childCount - 1; i++)
        {
            analyzeStatement(bodyNode);
            bodyNode = bodyNode->right; // Move to the next sibling for the next iteration
        }
        LOG_INFO("Finished analyzing repeat statement.");
    }
    else if (node->value == "for")
    {
        LOG_INFO("Analyzing for statement.");
        TreeNode *forStatInit = node->left;    // First child is the for statement
        TreeNode *forExp = forStatInit->right; // First child of for statement is the expression
        TreeNode *forStatEnd = forExp->right;  // Next sibling of for statement is the end statement
        LOG_INFO("For statement initialization node value: " + forStatInit->value);
        LOG_INFO("For statement expression node value: " + forExp->value);
        LOG_INFO("For statement end node value: " + forStatEnd->value);
        analyzeForStatement(forStatInit);
        SemanticType forExpType = analyzeForExpression(forExp);
        if (forExpType != SemanticType::Boolean)
        {
            LOG_ERROR("Condition in 'for' statement must be of boolean type.");
            return;
        }
        analyzeStatement(forStatEnd);
        LOG_INFO("Finished analyzing for statement.");
    }
    else if(node->value == "loop")
    {
        LOG_INFO("Analyzing loop statement.");
        TreeNode *bodyNode = node->left; // First child is the body of the loop
        int numberOfStatements = countChildren(bodyNode);
        for (int i = 0; i < numberOfStatements; i++)
        {
            analyzeStatement(bodyNode);
            bodyNode = bodyNode->right; // Move to the next sibling for the next iteration
        }
        LOG_INFO("Finished analyzing loop statement.");

    }
    else if(node->value == "case")
    {
        LOG_INFO("Analyzing case statement.");
        TreeNode *caseExpNode = node->left; // First child is the case expression
        int numberChildren = countChildren(caseExpNode->right); // Following siblings are the case clauses (+ optional otherwise)
        bool hasOtherwise = false; 
        TreeNode *temp = caseExpNode;
        while (temp->right != nullptr)        {
            temp = temp->right; // Move to the next sibling
        }
        if(temp->value == "otherwise")
        {
            numberChildren--; // The 'otherwise' case does not have a condition, so we need to subtract it from the count
            TreeNode *otherwiseNode = temp; // The last child is the 'otherwise' case
            hasOtherwise = true;
        }
        LOG_INFO("Case statement has " + std::to_string(numberChildren) + " cases and " + (hasOtherwise ? "an 'otherwise' case." : "no 'otherwise' case."));
        SemanticType caseExpType = analyzeExpression(caseExpNode);
        if (caseExpType == SemanticType::Integer || caseExpType == SemanticType::Char || caseExpType == SemanticType::UserDefined || caseExpType == SemanticType::Boolean)
        {
            // Validate a single ConstValue node used as a case label.
            auto validateCaseConst = [this](TreeNode *constValue) {
                // Integer / char literals are always valid labels.
                if (isIntegerLiteral(constValue) || isCharLiteral(constValue))
                    return;
                // Otherwise it must be a previously declared identifier (e.g. an enum literal).
                if (symbolTable.lookup(constValue->left->value) == nullptr)
                    LOG_ERROR("Case constant '" + constValue->left->value + "' is not declared.");
            };

            // Each following sibling of the selector is its own "case_clause" subtree
            // whose children are: CaseExpr (',' CaseExpr)* Statement.
            TreeNode *clause = caseExpNode->right; // first case_clause
            for (int i = 0; i < numberChildren && clause != nullptr; i++)
            {
                LOG_INFO("Analyzing case " + std::to_string(i + 1) + " of case statement.");
                int clauseChildCount = countChildren(clause->left);
                TreeNode *caseExpr = clause->left;
                // All children except the last are case labels; the last is the body.
                for (int j = 0; j < clauseChildCount - 1; j++)
                {
                    if (caseExpr->value == "..")
                    {
                        // Range label: lower '..' upper, both ConstValue nodes.
                        validateCaseConst(caseExpr->left);
                        validateCaseConst(caseExpr->left->right);
                    }
                    else
                    {
                        validateCaseConst(caseExpr);
                    }
                    caseExpr = caseExpr->right; // next label in this clause
                }
                LOG_INFO("Finished analyzing case " + std::to_string(i + 1) + " of case statement.");
                LOG_INFO("Analyzing body of case " + std::to_string(i + 1) + " of case statement.");
                analyzeStatement(caseExpr); // the remaining child is the clause body
                clause = clause->right;     // advance to the next case_clause
            }
        }
        else
        {
            LOG_ERROR("Case expression has an unknown type.");
            return;
        }
        if (hasOtherwise)
        {
            LOG_INFO("Analyzing 'otherwise' case of case statement.");
            TreeNode *otherwiseNode = temp; // The last child is the 'otherwise' case
            analyzeStatement(otherwiseNode->left); // First child of 'otherwise' node is the body of the 'otherwise' case
        }
        LOG_INFO("Finished analyzing case statement.");
    }
    else if(node->value == "read")
    {
        TreeNode *current = node->left; // First child is the start of expressions to read into
        int childCount = countChildren(current);
        LOG_INFO("Child count for read statement: " + std::to_string(childCount));
        for (int i = 0; i < childCount; i++)
        {
            LOG_INFO("Analyzing read statement.");
            analyzeExpression(current); // Each child is an expression to read into (probably just identifiers, but could be more complex)
            current = current->right; // Move to the next sibling
        }
        LOG_INFO("Finished analyzing read statement.");
    }
    else if(node->value == "exit")
    {
        LOG_INFO("Analyzing exit statement.");
        return;
    }
    else if(node->value == "return")
    {
        TreeNode *current = node->left; // First child is the expression being returned (if any)
        if (current != nullptr)
        {
            LOG_INFO("Analyzing return statement with expression.");
            analyzeExpression(current);
        }
        else
        {
            LOG_ERROR("Analyzing return statement with no expression.");
        }
    }
    else if (node->value == "<null>")
    {
        LOG_INFO("Analyzing null statement.");
    }
    else
    {
        LOG_INFO("Analyzing assignment statement.");
        analyzeAssignment(node); // if no specific rule exists, pass direct tree
    }
}

void SemanticAnalyzer::analyzeAssignment(TreeNode *node)
{ // Handle Leave Nodes
    TreeNode *current = node;
    if (current->value == "assign")
    {
        TreeNode *leftNode = current->left;    // The actual identifier is the left child of the first child
        TreeNode *rightNode = leftNode->right; // The expression being assigned is the right
        SemanticType exprType = analyzeExpression(rightNode);
        Symbol *sym = symbolTable.lookup(leftNode->left->value);
        if (sym == nullptr)
        {
            Symbol newIdentifier;
            newIdentifier.name = leftNode->left->value;
            newIdentifier.kind = SymbolKind::Variable;
            newIdentifier.type = getSymbolTypeFromSemanticType(exprType);
            if (newIdentifier.type == SymbolType::UserDefined)
            {
                LOG_ERROR("Cannot infer user-defined type for identifier '" + newIdentifier.name + "'. Please declare the variable with an explicit type.");
                return;
            }
            if (!symbolTable.declare(newIdentifier))
            {
                LOG_ERROR("Variable '" + newIdentifier.name + "' is already declared in the current scope.");
                return;
            }
            return;
        }
        if (getSemanticTypeFromSymbolType(sym->type) != exprType)
        {
            LOG_ERROR("Type mismatch in assignment to '" + leftNode->value + "'. Expected type: " + symbolTypeToString(sym->type) + ", but got: " + std::to_string(static_cast<int>(exprType)));
            return;
        }
    }
    else if (current->value == "swap")
    {
        TreeNode *leftNode = current->left;    // The first identifier is the left child of the first child
        TreeNode *rightNode = leftNode->right; // The second identifier is the right child of the first child
        SemanticType leftType, rightType;
        Symbol *leftSym = symbolTable.lookup(leftNode->left->value);
        Symbol *rightSym = symbolTable.lookup(rightNode->left->value);
        if (leftSym == nullptr || rightSym == nullptr)
        {
            LOG_ERROR("One of the identifiers in the swap statement is not declared.");
            return;
        }
        leftType = getSemanticTypeFromSymbolType(leftSym->type);
        rightType = getSemanticTypeFromSymbolType(rightSym->type);
        if (leftType != rightType)
        {
            LOG_ERROR("Type mismatch in swap statement. '" + leftNode->left->value + "' has type '" + symbolTypeToString(leftSym->type) + "', but '" + rightNode->left->value + "' has type '" + symbolTypeToString(rightSym->type) + "'.");
            return;
        }
    }
    return;
}

void SemanticAnalyzer::analyzeOutputStatement(TreeNode *node)
{
    TreeNode *current = node->left; // First child is the expression to output
    int childCount = 0;
    TreeNode *temp = current;
    while (temp != nullptr)
    {
        childCount++;
        temp = temp->right; // Move to the next sibling
    }
    LOG_INFO("Child count for output statement: " + std::to_string(childCount));
    LOG_INFO("Child count for output statement: " + std::to_string(countChildren(node->left)));

    for (int i = 0; i < childCount; i++)
    {
        // Handle Leave Nodes
        LOG_INFO("Analyzing output statement.");
        if (current->value == "string")
        {
            LOG_INFO("Output statement with string literal: " + current->left->value);
            return;
        }
        else
        {
            LOG_INFO("Output statement with expression.");
            SemanticType exprType = analyzeExpression(current);
            if (exprType != SemanticType::Integer && exprType != SemanticType::Char)
            {
                LOG_ERROR("Output statement expects an integer or character expression.");
                return;
            }
        }
        current = current->right; // Move to the next sibling
    }
}
void SemanticAnalyzer::analyzeForStatement(TreeNode *node)
{
    if (node->value != "<null>")
    {
        analyzeAssignment(node); // First child is the for expression
        return;
    }
    // Implementation for analyzing a for statement
}

SemanticType SemanticAnalyzer::analyzeForExpression(TreeNode *node)
{
    if (node->value != "true")
    {
        return analyzeExpression(node); // First child is the for expression
    }
    else
    {
        return SemanticType::Boolean; // If the for expression is 'true', we can treat it as a boolean type
    }
    // Implementation for analyzing the expression in a for statement
}

SemanticType SemanticAnalyzer::analyzeExpression(TreeNode *node)
{
    // Implementation for analyzing an expression and determining its type
    TreeNode *current = node;
    if (current->value == "<=" || current->value == "<" || current->value == ">" || current->value == ">=" || current->value == "<>" || current->value == "=")
    {
        LOG_INFO("Analyzing relational expression with operator: " + current->value);
        TreeNode *leftNode = current->left;    // First child is the left operand
        TreeNode *rightNode = leftNode->right; // Next sibling is the right operand
        // Handle Leave Nodes
        SemanticType leftType = analyzeTerm(leftNode);
        SemanticType rightType = analyzeTerm(rightNode);
        if (leftType != rightType)
        {
            LOG_ERROR("Type mismatch in relational expression. Left operand has type '" + std::to_string(static_cast<int>(leftType)) + "', but right operand has type '" + std::to_string(static_cast<int>(rightType)) + "'.");
            return SemanticType::Unknown;
        }
        if (current->value == "<>" || current->value == "=")
        {
            if (leftType == SemanticType::Char && rightType == SemanticType::Char)
            {
                return SemanticType::Boolean;
            }
            if (leftType == SemanticType::String && rightType == SemanticType::String)
            {
                return SemanticType::Boolean;
            }
            if (leftType == SemanticType::Boolean && rightType == SemanticType::Boolean)
            {
                return SemanticType::Boolean;
            }
            LOG_INFO("Analyzing not equal expression.");
        }
        if (leftType == SemanticType::Integer && rightType == SemanticType::Integer)
        {
            return SemanticType::Boolean;
        }
        else if (leftType == SemanticType::UserDefined && rightType == SemanticType::UserDefined)
        {
            return SemanticType::Boolean;
        }
        return SemanticType::Unknown; // If types are not compatible for relational expressions, return unknown
    }
    else
    {
        LOG_INFO("Analyzing term with operator: " + current->value);
        return analyzeTerm(node);
    }
    LOG_INFO("Analyzing expression with node value: " + node->value);
    return SemanticType::Unknown;
}

SemanticType SemanticAnalyzer::analyzeTerm(TreeNode *node)
{
    // Implementation for analyzing a term and determining its type
    TreeNode *current = node;
    if (current->value == "+" || current->value == "-" || current->value == "or")
    {
        TreeNode *leftNode = current->left;    // First child is the left operand
        TreeNode *rightNode = leftNode->right; // Next sibling is the right operand
        SemanticType leftType = analyzeTerm(leftNode);
        SemanticType rightType = analyzeFactor(rightNode);

        if (current->value == "+" || current->value == "-")
        {
            if (leftType == SemanticType::Integer && rightType == SemanticType::Integer)
            {
                LOG_INFO("Analyzing addition or subtraction expression.");
                return SemanticType::Integer;
            }
            // An enumerated value may be offset by an integer (succ/pred-style), yielding the enum.
            if ((leftType == SemanticType::UserDefined && rightType == SemanticType::Integer) ||
                (leftType == SemanticType::Integer && rightType == SemanticType::UserDefined))
            {
                LOG_INFO("Analyzing enumerated offset expression.");
                return SemanticType::UserDefined;
            }
        }
        else // current->value == "or"
        {
            if (leftType == SemanticType::Boolean && rightType == SemanticType::Boolean)
            {
                LOG_INFO("Analyzing logical OR expression.");
                return SemanticType::Boolean;
            }
        }
        LOG_ERROR("Type mismatch in term expression. Left operand has type '" + std::to_string(static_cast<int>(leftType)) + "', but right operand has type '" + std::to_string(static_cast<int>(rightType)) + "'.");
        return SemanticType::Unknown; // If types are not compatible for term expressions, return unknown
    }
    else
    {
        LOG_INFO("Analyzing factor with operator: " + current->value);
        return analyzeFactor(node);
    }
    LOG_INFO("Analyzing term with node value: " + node->value);
    return SemanticType::Unknown;
}

SemanticType SemanticAnalyzer::analyzeFactor(TreeNode *node)
{
    // Implementation for analyzing a factor and determining its type
    TreeNode *current = node;
    if (current->value == "*" || current->value == "/" || current->value == "and" || current->value == "mod")
    {
        TreeNode *leftNode = current->left;    // First child is the left operand
        TreeNode *rightNode = leftNode->right; // Next sibling is the right operand
        SemanticType leftType = analyzeFactor(leftNode);
        SemanticType rightType = analyzePrimary(rightNode);
        // Handle Leave Nodes
        if (leftType != rightType)
        {
            LOG_ERROR("Type mismatch in factor expression. Left operand has type '" + std::to_string(static_cast<int>(leftType)) + "', but right operand has type '" + std::to_string(static_cast<int>(rightType)) + "'.");
            return SemanticType::Unknown;
        }
        if (current->value == "*" || current->value == "/" || current->value == "mod")
        {
            if (leftType == SemanticType::Integer && rightType == SemanticType::Integer)
            {
                LOG_INFO("Analyzing multiplication or division expression.");
                return SemanticType::Integer;
            }
        }

        if (current->value == "and")
        {
            if (leftType == SemanticType::Boolean && rightType == SemanticType::Boolean)
            {
                LOG_INFO("Analyzing logical AND or modulo expression.");
                return SemanticType::Boolean;
            }
        }
        LOG_ERROR("Type mismatch in factor expression. Left operand has type '" + std::to_string(static_cast<int>(leftType)) + "', but right operand has type '" + std::to_string(static_cast<int>(rightType)) + "'.");
        return SemanticType::Unknown; // If types are not compatible for factor expressions, return unknown
    }
    else
    {
        LOG_INFO("Analyzing primary : " + current->value);
        return analyzePrimary(node);
    }
    LOG_INFO("Analyzing factor with unknown operator: " + current->value);
    return SemanticType::Unknown;
}

SemanticType SemanticAnalyzer::analyzePrimary(TreeNode *node)
{
    // Implementation for analyzing a primary and determining its type
    TreeNode *current = node;
    // Handle Leave Nodes
    if (current->value == "-")
    {
        TreeNode *operandNode = current->left; // The operand of the unary minus is the left child
        SemanticType operandType = analyzePrimary(operandNode);
        if (operandType == SemanticType::Integer)
        {
            LOG_INFO("Analyzing unary minus expression.");
            return SemanticType::Integer;
        }
        LOG_ERROR("Unary minus operator requires an integer operand, but got type '" + std::to_string(static_cast<int>(operandType)) + "'.");
        return SemanticType::Unknown;
    }
    else if (current->value == "not")
    {
        TreeNode *operandNode = current->left; // The operand of the logical NOT is the left child
        SemanticType operandType = analyzePrimary(operandNode);
        if (operandType == SemanticType::Boolean)
        {
            LOG_INFO("Analyzing logical NOT expression.");
            return SemanticType::Boolean;
        }
        LOG_ERROR("Logical NOT operator requires a boolean operand, but got type '" + std::to_string(static_cast<int>(operandType)) + "'.");
        return SemanticType::Unknown;
    }
    else if (current->value == "eof")
    {
        LOG_INFO("Analyzing EOF literal.");
        return SemanticType::Boolean; // Assuming EOF is treated as a boolean type
    }
    else if (current->value == "<identifier>")
    {
        LOG_INFO("Analyzing identifier: " + current->left->value);
        Symbol *sym = symbolTable.lookup(current->left->value);
        if (sym == nullptr)
        {
            LOG_ERROR("Identifier '" + current->left->value + "' is not declared.");
            return SemanticType::Unknown;
        }
        return getSemanticTypeFromSymbolType(sym->type);
    }
    else if (current->value == "<integer>")
    {
        LOG_INFO("Analyzing integer literal: " + current->left->value);
        return SemanticType::Integer;
    }
    else if (current->value == "<char>")
    {
        LOG_INFO("Analyzing character literal: " + current->left->value);
        return SemanticType::Char;
    }
    else if (current->value == "call")
    {
        LOG_INFO("Analyzing function call: " + current->left->left->value);
        Symbol *sym = symbolTable.lookup(current->left->left->value);
        if (sym == nullptr)
        {
            LOG_ERROR("Function '" + current->left->left->value + "' is not declared.");
            return SemanticType::Unknown;
        }
        if (sym->kind != SymbolKind::Function)
        {
            LOG_ERROR("'" + current->left->left->value + "' is not a function.");
            return SemanticType::Unknown;
        }
        return getSemanticTypeFromSymbolType(sym->type);
    }
    else if (current->value == "succ" || current->value == "pred")
    {
        LOG_INFO("Analyzing successor or predecessor function.");
        TreeNode *operandNode = current->left; // The operand of the successor function is the left child
        SemanticType operandType = analyzeExpression(operandNode);
        if (operandType == SemanticType::Integer)
        {
            LOG_INFO("Analyzing successor or predecessor function with integer operand.");
            return SemanticType::Integer;
        }
        LOG_ERROR("Successor or predecessor function requires an integer operand, but got type '" + std::to_string(static_cast<int>(operandType)) + "'.");
        return SemanticType::Unknown;
    }
    else if (current->value == "ord" || current->value == "chr")
    {
        LOG_INFO("Analyzing ordinal or character conversion function.");
        TreeNode *operandNode = current->left; // The operand of the ord/chr function is the left child
        SemanticType operandType = analyzeExpression(operandNode);
        if (current->value == "ord")
        {
            if (operandType == SemanticType::Char)
            {
                LOG_INFO("Analyzing ordinal conversion function with character operand.");
                return SemanticType::Integer;
            }
            LOG_ERROR("Ordinal conversion function requires a character operand, but got type '" + std::to_string(static_cast<int>(operandType)) + "'.");
            return SemanticType::Unknown;
        }
        else // current->value == "chr"
        {
            if (operandType == SemanticType::Integer)
            {
                LOG_INFO("Analyzing character conversion function with integer operand.");
                return SemanticType::Char;
            }
            LOG_ERROR("Character conversion function requires an integer operand, but got type '" + std::to_string(static_cast<int>(operandType)) + "'.");
            return SemanticType::Unknown;
        }
    }
    // else if (current->value == "<string>")
    // {
    //     LOG_INFO("Analyzing string literal: " + current->left->value);
    //     return SemanticType::String;
    // }
    else if (current->value == "<=" || current->value == "<" || current->value == ">" || current->value == ">=" || current->value == "<>" || current->value == "=")
    {
        // A parenthesized relational expression appears where a primary is expected.
        return analyzeExpression(current);
    }
    else
    {
        LOG_ERROR("Unknown primary expression with node value: " + current->value);
        return SemanticType::Unknown;
    }
}

SemanticType SemanticAnalyzer::findReturnNodes(TreeNode *node)
{
    std::vector<TreeNode *> returnNodes;
    int childCount = countChildren(node);
    TreeNode *current = node->left; // Start with the first child
    std::queue<TreeNode *> nodesToVisit;
    nodesToVisit.push(current);
    while (!nodesToVisit.empty())
    {
        TreeNode *currentNode = nodesToVisit.front();
        nodesToVisit.pop();
        if (currentNode->value == "return")
        {
            returnNodes.push_back(currentNode);
        }
        else
        {
            // Add all children of the current node to the queue
            TreeNode *child = currentNode->left;
            while (child != nullptr)
            {
                nodesToVisit.push(child);
                child = child->right;
            }
        }
    }

    if (returnNodes.empty())
    {
        LOG_ERROR("No return statements found in function.");
        return SemanticType::Unknown;
    }
    SemanticType returnType = analyzeExpression(returnNodes[0]->left); // Analyze the expression being returned in the first return statement
    for (auto returnNode : returnNodes)
    {
        SemanticType currentReturnType = analyzeExpression(returnNode->left);
        if (currentReturnType != returnType)
        {
            LOG_ERROR("Type mismatch in return statements. Expected type: " + std::to_string(static_cast<int>(returnType)) + ", but got: " + std::to_string(static_cast<int>(currentReturnType)) + ".");
            return SemanticType::Unknown;
        }
    }
    return returnType;
}