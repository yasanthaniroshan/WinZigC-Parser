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
    // if (constValue == nullptr)
    // {
    //     LOG_ERROR("Constant '" + constSymbol.name + "' is missing a value.");
    //     return;
    // }
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
        currentLiteral = currentLiteral->right; // Move to the next sibling
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
    symbolTable.enterScope();                     // Enter a new scope for the function
    analyzeParams(paramsNode);
    // Need to analyze return type
    analyzeConsts(constsNode);
    analyzeTypes(typesNode);
    analyzeDclns(dclnsNode);
    analyzeBody(bodyNode);
    symbolTable.exitScope(); // Leave the function scope
    // Implementation for analyzing a function declaration
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
    else if(node->value == "while")
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
        while (temp->right != nullptr) {
            temp = temp->right; // Move to the next sibling
        }
        TreeNode * untilNode = temp; // The last child is the 'until' condition
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
            LOG_ERROR("Identifier '" + leftNode->left->value + "' is not declared.");
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
    // Handle Leave Nodes
    if (current->value == "+")
    {
        LOG_INFO("Analyzing addition expression.");
    }
    else if (current->value == "-")
    {
        LOG_INFO("Analyzing subtraction expression.");
    }
    else if (current->value == "or")
    {
        LOG_INFO("Analyzing logical OR expression.");
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
    // Handle Leave Nodes
    if (current->value == "*")
    {
        LOG_INFO("Analyzing multiplication expression.");
    }
    else if (current->value == "/")
    {
        LOG_INFO("Analyzing division expression.");
    }
    else if (current->value == "and")
    {
        LOG_INFO("Analyzing logical AND expression.");
    }
    else if (current->value == "mod")
    {
        LOG_INFO("Analyzing modulo expression.");
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
    if (current->value == "<integer>")
    {
        LOG_INFO("Analyzing integer literal: " + current->left->value);
        return SemanticType::Integer;
    }
    else if (current->value == "<char>")
    {
        LOG_INFO("Analyzing character literal: " + current->left->value);
        return SemanticType::Char;
    }
    else if (current->value == "<string>")
    {
        LOG_INFO("Analyzing string literal: " + current->left->value);
        return SemanticType::String;
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
    else
    {
        LOG_ERROR("Unknown primary expression with node value: " + current->value);
        return SemanticType::Unknown;
    }
}
