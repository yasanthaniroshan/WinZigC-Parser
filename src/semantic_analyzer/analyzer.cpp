#include "semantic_analyzer/analyzer.h"

SemanticAnalyzer::SemanticAnalyzer(TreeNode* ast) : ast(ast), symbolTable() {
}

SemanticAnalyzer::~SemanticAnalyzer() {
    // The destructor does not need to delete `ast` because it is owned by the caller.
}

bool SemanticAnalyzer::isIntegerLiteral(TreeNode* node) {
    return node != nullptr && node->value == "<integer>";
}

bool SemanticAnalyzer::isCharLiteral(TreeNode* node) {
    return node != nullptr && node->value == "<char>";
}


Result<void> SemanticAnalyzer::analyze() {
    std::cout << "Semantic analysis successful!" << std::endl;
    std::cout << "Abstract Syntax Tree:" << std::endl;
    analyzeProgram(ast);
    return Result<void>::Ok();
}

int SemanticAnalyzer::countChildren(TreeNode* node) {
    TreeNode* current = node;
    int count = 0;
    while (current != nullptr) {
        count++;
        current = current->right; // Move to the next sibling
    }
    return count;
}

void SemanticAnalyzer::analyzeProgram(TreeNode* node) {
    TreeNode* name = node->left; // First child is program name
    TreeNode* consts = name->right; // First sibling is const declarations
    TreeNode* types = consts->right; // Next sibling is type declarations
    TreeNode* dclns = types->right; // Next sibling is variable and function declarations
    TreeNode* subprogs = dclns->right; // Next sibling is subprogram declarations
    if(subprogs->right == nullptr) {
        LOG_ERROR("Program body is missing.");
        return;
    }
    TreeNode* body = subprogs->right; // Next sibling is the body of the program
    TreeNode* endName = body->right; // Next sibling is the end name of the program
    analyzeConsts(consts);
    analyzeTypes(types);
}

void SemanticAnalyzer::analyzeConsts(TreeNode* node) {
    TreeNode* current = node;
    if (current->left == nullptr) {
        LOG_INFO("No constants declared.");
        return;
    }
    current = current->left; // Move to the first constant declaration
    while (current != nullptr) {
        analyzeConst(current); // Analyze each constant declaration
        current = current->right; // Move to the next sibling
    }
    LOG_INFO("Finished analyzing constants.");
}

void SemanticAnalyzer::analyzeConst(TreeNode* node) {
    TreeNode* identifierNode = node->left; // First child is the identifier
    Symbol constSymbol;
    constSymbol.name = identifierNode->left->value; // The actual constant name is the left child of the identifier node
    constSymbol.kind = SymbolKind::Constant;
    auto isDefined = symbolTable.lookup(constSymbol.name);
    if (isDefined != nullptr) {
        LOG_ERROR("Constant '" + constSymbol.name + "' is already declared in the current scope.");
        return;
    }
    TreeNode* constValue = identifierNode->right; // Next sibling is the constant value
    if (constValue == nullptr) {
        LOG_ERROR("Constant '" + constSymbol.name + "' is missing a value.");
        return;
    }
    if (isIntegerLiteral(constValue)) {
        constSymbol.type = SymbolType::Integer;
    } else if (isCharLiteral(constValue)) {
        constSymbol.type = SymbolType::Char;
    } else {
        auto typeSymbol = symbolTable.lookup(constValue->left->value);
        if (typeSymbol == nullptr) {
            LOG_ERROR("Type '" + constValue->left->value + "' for constant '" + constSymbol.name + "' is not declared.");
            return; 
        }
        constSymbol.type = typeSymbol->type;
    }
    if (!symbolTable.declare(constSymbol)) {
        LOG_ERROR("Constant '" + constSymbol.name + "' is already declared in the current scope.");
        return;
    }
    LOG_INFO("Declared constant '" + constSymbol.name + "' of type '" + symbolTypeToString(constSymbol.type) + "'.");
    return;
}

void SemanticAnalyzer::analyzeTypes(TreeNode* node) {
    TreeNode* current = node;
    if (current->left == nullptr) {
        LOG_INFO("No types declared.");
        return;
    }
    current = current->left; // Move to the first type declaration
    while (current != nullptr) {
        analyzeType(current); // Analyze each type declaration
        current = current->right; // Move to the next sibling
    }
}

void SemanticAnalyzer::analyzeType(TreeNode* node) {
    TreeNode* identifierNode = node->left; // First child is the identifier
    TreeNode* literalListNode = identifierNode->right; // Next sibling is the literal list
    std::string typeName = identifierNode->left->value; // The actual type name is the left child of the identifier node
    Symbol typeSymbol;
    typeSymbol.name = typeName;
    typeSymbol.kind = SymbolKind::Type;
    typeSymbol.type = SymbolType::UserDefined;
    TreeNode* currentLiteral = literalListNode->left; // First child of literal list is the first literal
    while (currentLiteral != nullptr) {
        typeSymbol.members.push_back(currentLiteral->left->value); // Add the literal to the list
        currentLiteral = currentLiteral->right; // Move to the next sibling
    }
    if (!symbolTable.declare(typeSymbol)) {
        LOG_ERROR("Type '" + typeName + "' is already declared in the current scope.");
        return;
    }
    LOG_INFO("Declared type '" + typeName + "' with literals: " + std::to_string(typeSymbol.members.size()));
    // symbolTable.printAllScopes();
    return;
}