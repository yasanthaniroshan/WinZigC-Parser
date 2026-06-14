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
    if (constValue == nullptr)
    {
        LOG_ERROR("Constant '" + constSymbol.name + "' is missing a value.");
        return;
    }
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
        memberSymbol.typeName = typeName; // Set the type name for the member
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
        // auto isDefined = symbolTable.lookup(varSymbol.name);
        // if (isDefined != nullptr) {
        //     LOG_ERROR("Variable '" + varSymbol.name + "' is already declared in the current scope.");
        //     return;
        // }
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
    symbolTable.enterScope();                     // Enter a new scope for the function
    analyzeParams(paramsNode);
    // Need to analyze return type
    analyzeConsts(constsNode);
    analyzeTypes(typesNode);
    analyzeDclns(dclnsNode);
    analyzeBody(bodyNode);
    symbolTable.exitScope(); // Leave the function scope
    if (identifierNode->left->value != endNameNode->left->value)
    {
        LOG_ERROR("Function '" + identifierNode->left->value + "' end name '" + endNameNode->left->value + "' does not match.");
        return;
    }
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
        analyzeOutputStatement(node);
    }
    else if (node->value == "<null>")
    {
        LOG_INFO("Analyzing null statement.");
    }
    else
    {
        LOG_ERROR("Unknown statement type: " + node->value);
    }
}

void SemanticAnalyzer::analyzeOutputStatement(TreeNode *node)
{
    // Implementation for analyzing an output statement
    TreeNode *current = node->left; // First child is the expression to output
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
}

SemanticType SemanticAnalyzer::analyzeExpression(TreeNode *node)
{
    // Implementation for analyzing an expression and determining its type
    LOG_INFO("Analyzing expression with node value: " + node->value);
    return SemanticType::Unknown;
}