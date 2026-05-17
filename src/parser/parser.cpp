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

// Parse the identifier.
void Parser::identifier() {
    if(check(TokensType::Identifier)) {
        Token token = advance();
        LOG_INFO("Identifier found: " + token.toString());
        TreeNode* node = new TreeNode("<identifier>");
        TreeNode* child = new TreeNode(token.lexeme);
        node->left = child;
        push(node);
        return; // TODO: Update the implementation
    }
    LOG_ERROR("Expected identifier but found " + peek().toString());
    return; // TODO: Update the implementation
}

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

void Parser::constValue() {
    LOG_INFO("Parsing const value");
    switch (peek().type) {
        case TokensType::IntegerLiteral:
        {
            integerLiteral();
            break;
        }
        case TokensType::CharLiteral:
        {
            charLiteral();
            break;
        }
        case TokensType::Identifier:
        {
            identifier();
            break;
        }
        default:
            LOG_ERROR("Expected integer literal or string but found " + peek().toString());
            return;
    }
}

// Parse the types.
void Parser::types() {
    LOG_INFO("Parsing types");
    push(new TreeNode("types"));
    return; // TODO: Implement
}

// Parse the dclns.
void Parser::dclns() {
    LOG_INFO("Parsing declarations");
    push(new TreeNode("dclns"));
    return; // TODO: Implement
}

// Parse the subprogs.
void Parser::subprogs() {
    LOG_INFO("Parsing subprograms");
    push(new TreeNode("subprogs"));
    return; // TODO: Implement
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
    push(new TreeNode("program"));
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

