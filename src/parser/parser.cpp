#include "parser.h"
/**
 * @file parser.cpp
 * @brief Parser class implementation
 * @version 0.1
 * @date 2026-05-16
 * @author: Yasantha Niroshan
 * @copyright Copyright (c) 2026
*/

// ==================== Constructor and Destructor ====================

// Constructor for the Parser class.
Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

// Destructor for the Parser class.
Parser::~Parser() {
    for (TreeNode* node : stack) {
        delete node;
    }
}

// ==================== Helper Functions ====================

// Parse the WinZigC program.
Result<void> Parser::parse() {
    winzig();
    if (!isAtEnd() && peek().type != TokensType::EndOfFile) {
        LOG_ERROR("Expected end of file");
        return Result<void>::Err(ParserError("Expected end of file"));
    }
    printTree(stack.back(), 0);
    return Result<void>::Ok();
}

// Check if the end of the tokens is reached.
bool Parser::isAtEnd() const {
    return current >= tokens.size();
}

// Check if the current token matches the type.
bool Parser::check(TokensType type) const {
    if (isAtEnd()) return false;
    return tokens[current].type == type;
}

// Peek at the current token.
Token Parser::peek() const {
    return tokens[current];
}

// Get the previous token.
Token Parser::previous() const {
    return tokens[current - 1];
}

// Advance to the next token.
Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}


// Consume the current token if it matches the type.
Token Parser::consume(TokensType type,const std::string& what) {
    if(check(type)) {
        Token token = advance();
        LOG_INFO("Consumed " + what + ": " + token.toString());
        return token;
    }
    LOG_ERROR("Expected " + what + " but found " + peek().toString());
    return Token(TokensType::Unknown, "", 0, 0);
}

// ==================== Parsing Functions ====================

/**
* @brief Parses the consts statement.
* @details following the grammar is parsed,
* consts -> 'const' Const list ',' ';'
* consts ->
* @return void
*/
void Parser::consts() {
    LOG_INFO("Parsing constants");
    // Parsing const ->  
    // Check for actually are there any consts in the program
    if(!check(TokensType::Key_const)) {
        // If there are no consts in the program, push a consts node to the stack
        push(new TreeNode("consts"));
        return; 
    }
    // If there any consts in this program, parse the consts
    // consts -> 'const' Const list ',' ';'
    consume(TokensType::Key_const, "const");
    constDeclaration(); // Parse the const declaration
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        constDeclaration(); // parse the const declaration
        n++; // increment the number of const declarations
    }
    buildTree("consts", n); // Build the tree for the consts statement
    consume(TokensType::Semicolon, "semicolon"); // consume the semicolon
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the const declaration.
* @details following the grammar is parsed,
* constDeclaration -> identifier '=' constValue
* @return void
*/
void Parser::constDeclaration() {
    LOG_INFO("Parsing const declaration");
    // Parsing const -> identifier '=' constValue
    identifier();
    consume(TokensType::Equal, "equal"); // consume the equal sign
    constValue();
    buildTree("const", 2); // Build the tree for the const declaration
    return; // TODO: Need to handle errors here
}
/**
* @brief Parses the const value.
* @details following the grammar is parsed,
* constValue -> integerLiteral | charLiteral | identifier
* @return void
*/
void Parser::constValue() {
    LOG_INFO("Parsing const value");
    // Parsing const value -> integerLiteral | charLiteral | identifier
    switch (peek().type) {
        case TokensType::IntegerLiteral:
        {
            // Parsing integer literal -> integerLiteral
            integerLiteral();
            break;
        }
        case TokensType::CharLiteral:
        {
            // Parsing char literal -> charLiteral
            charLiteral();
            break;
        }
        case TokensType::Identifier:
        {
            // Parsing identifier -> identifier
            identifier();
            break;
        }
        default:
        LOG_ERROR("Expected integer literal or string but found " + peek().toString());
        return;
    }
}
/**
* @brief Parses the identifier.
* @details following the grammar is parsed,
* identifier -> Identifier
* @return void
*/
void Parser::identifier() {
    if(check(TokensType::Identifier)) {
        Token token = advance();
        LOG_INFO("Identifier found: " + token.toString());
        TreeNode* node = new TreeNode("<identifier>");
        TreeNode* child = new TreeNode(token.lexeme);
        node->left = child;
        push(node);
        return; // TODO: Need to handle errors here
    }
    LOG_ERROR("Expected identifier but found " + peek().toString());
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the types.
* @details following the grammar is parsed,
* types -> 'type' Type list ';'
* types ->
* @return void
*/
void Parser::types() {
    LOG_INFO("Parsing types");
    // Check if there are any types in the program
    if(!check(TokensType::Key_type)) {
        // If there are no types in the program, push a types node to the stack
        // Parsing types -> 
        push(new TreeNode("types")); 
        return; // TODO: Need to handle errors here
    }
    // If there are types in the program, parse the types
    consume(TokensType::Key_type, "type"); // consume the type keyword
    // Parsing types -> 'type' Type list ';'
    int n = 0;
    do {
        typeDeclaration(); // parse the type declaration
        n++; // increment the number of type declarations
    } while (check(TokensType::Identifier));
    buildTree("types", n); // Build the tree for the types statement
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the type declaration.
* @details following the grammar is parsed,
* typeDeclaration -> identifier '=' litlist
* @return void
*/
void Parser::typeDeclaration() {
    LOG_INFO("Parsing type declaration");
    identifier();
    consume(TokensType::Equal, "equal");
    litlist();
    consume(TokensType::Semicolon, "semicolon");
    buildTree("type", 2);
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the litlist.
* @details following the grammar is parsed,
*   litlist -> '(' identifier list ',' ')'
* @return void
*/
void Parser::litlist() {
    LOG_INFO("Parsing litlist");
    consume(TokensType::OpenParen, "open parenthesis"); // consume the open parenthesis
    identifier(); // parse the identifier
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        identifier(); // parse the identifier
        n++; // increment the number of identifiers
    }
    consume(TokensType::CloseParen, "close parenthesis"); // consume the close parenthesis
    buildTree("lit", n);
    return; // TODO: Need to handle errors here
}
/** 
* @brief Parses the subprograms.
* @details following the grammar is parsed,
* subprogs -> Fcn
* subprogs ->
* @return void
*/
void Parser::subprogs() {
    LOG_INFO("Parsing subprograms");
    // Check if there are any subprograms in the program
    if(!check(TokensType::Key_function)) {
        // If there are no subprograms in the program, push a subprogs node to the stack
        // Parsing subprogs -> 
        push(new TreeNode("subprogs"));
        return; // TODO: Need to handle errors here
    }
    // If there are subprograms in the program, parse the subprograms
    // subprogs -> Fcn
    int n = 0;
    do {
        fcn(); // parse the function declaration
        n++; // increment the number of function declarations
    } while (check(TokensType::Identifier));
    buildTree("subprogs", n); // Build the tree for the subprograms statement
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the fcn statement.
* @details following the grammar is parsed,
* fcn -> 'function' <identifier> '(' Params ')' ':' <identifier> ';' Consts Types Dclns Body Name ';'
* @return void
*/
void Parser::fcn() {
    LOG_INFO("Parsing fcn");
    // Parsing fcn -> 'function' <identifier> '(' Params ')' ':' <identifier> ';' Consts Types Dclns Body Name ';'
    consume(TokensType::Key_function, "function");
    identifier();
    consume(TokensType::OpenParen, "open parenthesis");
    params();
    consume(TokensType::CloseParen, "close parenthesis");
    consume(TokensType::Colon, "colon");
    identifier();
    consume(TokensType::Semicolon, "semicolon");
    consts();
    types();
    dclns();
    body();
    identifier();
    consume(TokensType::Semicolon, "semicolon");
    buildTree("fcn", 9);
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the params.
* @details following the grammar is parsed,
* Params -> Dcln list ';'
* @return void
*/
void Parser::params() {
    LOG_INFO("Parsing params");
    // Parsing params -> Dcln list ';'
    dcln();
    int n = 1;
    while (check(TokensType::Semicolon)) {
        advance(); // consume the semicolon
        dcln(); // parse the dcln
        n++; // increment the number of dcln
    }
    buildTree("params", n); // Build the tree for the params statement
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the dcln.
* @details following the grammar is parsed,
* dcln -> <identifier> list ',' ':' <identifier> 
* @return void
*/
void Parser::dcln() {
    LOG_INFO("Parsing dcln");
    identifier();
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        identifier(); // parse the identifier
        n++; // increment the number of identifiers
    }
    consume(TokensType::Colon, "colon");
    identifier();
    buildTree("var", n);
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the dclns.
* @details following the grammar is parsed,
* dclns -> 'var' (Dcln ';' )+
* dclns ->
* @return void
*/
void Parser::dclns() {
    LOG_INFO("Parsing declarations");
    // Check if there are any declarations in the program
    if(!check(TokensType::Key_var)) {
        // If there are no declarations in the program, push a dclns node to the stack
        // Parsing dclns -> 
        push(new TreeNode("dclns"));
        return; // TODO: Need to handle errors here
    }
    // If there are declarations in the program, parse the declarations
    consume(TokensType::Key_var, "var");
    int n = 0;
    do {
        dcln();
        consume(TokensType::Semicolon, "semicolon");
        n++; // increment the number of dcln
    } while (check(TokensType::Identifier));
    buildTree("dclns", n); // Build the tree for the dclns statement
    return; // TODO: Need to handle errors here
}


// Parse the string literal.
void Parser::stringLiteral() {
    LOG_INFO("Parsing string literal");
    Token token = consume(TokensType::String, "string literal");
    TreeNode* node = new TreeNode("string");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
}

void Parser::integerLiteral() {
    LOG_INFO("Parsing integer literal");
    Token token = consume(TokensType::IntegerLiteral, "integer literal");
    TreeNode* node = new TreeNode("<integer>");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
}

void Parser::charLiteral() {
    LOG_INFO("Parsing char literal");
    Token token = consume(TokensType::CharLiteral, "char literal");
    TreeNode* node = new TreeNode("<char>");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
}
// Parse the output expression.
void Parser::outputExpression() {
    LOG_INFO("Parsing output expression");
    consume(TokensType::Key_output, "output");
    consume(TokensType::OpenParen, "open parenthesis");
    stringLiteral();
    consume(TokensType::CloseParen, "close parenthesis");
    LOG_INFO("Output expression parsed successfully");
    return; // TODO: Update the implementation
}

// Parse the statement.
void Parser::statement() {
    LOG_INFO("Parsing statement");
    switch (peek().type) {
        case TokensType::Key_output:
            outputExpression();
            buildTree("output", 1);
            break;
        default:
            LOG_ERROR("Expected output expression but found " + peek().toString());
            return;
    }
    return; // TODO: Update the implementation
}

// Parse the body.
void Parser::body() {
    LOG_INFO("Parsing body");
    consume(TokensType::Key_begin, "begin");
    statement();
    if (check(TokensType::Semicolon)) {
        advance();
    }
    consume(TokensType::Key_end, "end");
    buildTree("block", 1);
    return; // TODO: Update the implementation
}

void Parser::program() {
    LOG_INFO("Parsing program");
    consume(TokensType::Key_program, "program");
}

// Parse the WinZigC program.
void Parser::winzig() {
    LOG_INFO("Parsing WinZigC program");
    program();
    identifier();
    consume(TokensType::Colon, "colon");
    consts();
    types();
    dclns();
    subprogs();
    body();
    identifier();
    consume(TokensType::SingleDot, "single dot");
    buildTree("program", 7);
    return; // TODO: Update the implementation
}


// ==================== Stack Operations ====================

// Push a node onto the stack.
void Parser::push(TreeNode* node) {
    stack.push_back(node);
}

// Pop a node from the stack.
TreeNode* Parser::pop() {
    TreeNode* node = stack.back();
    stack.pop_back();
    return node;
}


// Build the tree.
void Parser::buildTree(std::string x, int n) {
    TreeNode* parent = nullptr;
    for (int i = 0; i < n; i++) {
        TreeNode* child = pop();
        child->right = parent;
        parent = child;
    }
    push(new TreeNode(std::move(x), parent, nullptr));
}

