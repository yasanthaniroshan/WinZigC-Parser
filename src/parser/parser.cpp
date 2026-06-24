#include "parser/parser.h"
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
    // for (TreeNode* node : stack) {
    //     delete node;
    // }
}

// ==================== Helper Functions ====================

// A human-readable description of the current token, for error messages.
std::string Parser::describeCurrent() const {
    Token token = peek();
    if (token.type == TokensType::EndOfFile) return "end of input";
    if (token.lexeme.empty()) return tokenTypeToString(token.type);
    return "'" + token.lexeme + "'";
}

// Records the syntax error at the current token. Only the first is kept, since parsing can't recover.
void Parser::recordError(const std::string& message) {
    if (!syntaxError.has_value()) {
        Token token = peek();
        syntaxError = ParserError(message, token.line, token.column);
    }
}

// Records an error at the current token and returns a failing Result for propagation.
Result<void> Parser::fail(const std::string& message) {
    recordError(message);
    return Result<void>::Err(ParserError(message));
}

Result<TreeNode*> Parser::parseTree() {
    winzig();
    if (!syntaxError.has_value() && !isAtEnd() && peek().type != TokensType::EndOfFile) {
        recordError("expected end of file, found " + describeCurrent());
    }
    if (syntaxError.has_value()) {
        return Result<TreeNode*>::ErrMsg("ParserError: " + syntaxError->msg);
    }
    if (stack.empty()) {
        return Result<TreeNode*>::ErrMsg("ParserError: empty parse stack");
    }
    return Result<TreeNode*>::Ok(stack.back());
}

// Parse the WinZigC program.
Result<TreeNode*> Parser::parse(bool printAbstractSyntaxTree) {
    auto result = parseTree();
    if (syntaxError.has_value()) {
        // Report the single syntax error with its source location, like a real compiler.
        std::cerr << "\n";
        diagnostics::error(syntaxError->msg, syntaxError->line, syntaxError->column);
        diagnostics::summary("Parsing failed with a syntax error.");
        return Result<TreeNode*>::ErrMsg("Parsing failed with a syntax error.");
    }
    if (printAbstractSyntaxTree) {
        printTree(result.value.value(), 0);
    }
    return Result<TreeNode*>::Ok(result.value.value());
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


// Consume the current token if it matches the type; records an error otherwise.
Result<void> Parser::consume(TokensType type,const std::string& what) {
    if(check(type)) {
        Token token = advance();
        LOG_DEBUG("Consumed " + what + ": " + token.toString());
        return Result<void>::Ok();
    }
    recordError("expected " + what + ", found " + describeCurrent());
    return Result<void>::Err(ParserError(syntaxError->msg));
}

// ==================== Parsing Functions ====================

/**
* @brief Parses the consts statement.
* @details following the grammar is parsed,
* consts -> 'const' Const list ',' ';'
* consts ->
* @return void
*/
Result<void> Parser::consts() {
    LOG_DEBUG("Parsing constants");
    // Check whether there are any consts in the program
    if(!check(TokensType::Key_const)) {
        push(new TreeNode("consts", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // consts -> 'const' Const list ',' ';'
    auto result = consume(TokensType::Key_const, "const");
    if (result.isErr()) {
        return result;
    }
    result = constDeclaration();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        result = constDeclaration();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    buildTree("consts", n, peek().line, peek().column);
    result = consume(TokensType::Semicolon, "semicolon");
    if (result.isErr()) {
        return result;
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the const declaration.
* @details following the grammar is parsed,
* constDeclaration -> identifier '=' constValue
* @return void
*/
Result<void> Parser::constDeclaration() {
    LOG_DEBUG("Parsing const declaration");
    // constDeclaration -> identifier '=' constValue
    auto result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Equal, "equal");
    if (result.isErr()) {
        return result;
    }
    result = constValue();
    if (result.isErr()) {
        return result;
    }
    buildTree("const", 2, peek().line, peek().column);
    return Result<void>::Ok();
}
/**
* @brief Parses the const value.
* @details following the grammar is parsed,
* constValue -> integerLiteral | charLiteral | identifier
* @return void
*/
Result<void> Parser::constValue() {
    LOG_DEBUG("Parsing const value");
    // constValue -> integerLiteral | charLiteral | identifier
    Result<void> result;
    switch (peek().type) {
        case TokensType::IntegerLiteral:
            result = integerLiteral();
            break;
        case TokensType::CharLiteral:
            result = charLiteral();
            break;
        case TokensType::Identifier:
            result = identifier();
            break;
        default:
            return fail("expected an integer, char, or identifier constant, found " + describeCurrent());
    }
    if (result.isErr()) {
        return result;
    }
    return Result<void>::Ok();
}
/**
* @brief Parses the identifier.
* @details following the grammar is parsed,
* identifier -> Identifier
* @return void
*/
Result<void> Parser::identifier() {
    if(check(TokensType::Identifier)) {
        Token token = advance();
        LOG_DEBUG("Identifier found: " + token.toString());
        TreeNode* node = new TreeNode("<identifier>", token.line, token.column);
        TreeNode* child = new TreeNode(token.lexeme, token.line, token.column);
        node->left = child;
        push(node);
        return Result<void>::Ok(); // TODO: Need to handle errors here
    }
    return fail("expected an identifier, found " + describeCurrent());
}

/**
* @brief Parses the types.
* @details following the grammar is parsed,
* types -> 'type' Type list ';'
* types ->
* @return void
*/
Result<void> Parser::types() {
    LOG_DEBUG("Parsing types");
    // Check whether there are any types in the program
    if(!check(TokensType::Key_type)) {
        push(new TreeNode("types", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // types -> 'type' Type list ';'
    auto result = consume(TokensType::Key_type, "type");
    if (result.isErr()) {
        return result;
    }
    int n = 0;
    do {
        result = typeDeclaration();
        if (result.isErr()) {
            return result;
        }
        n++;
    } while (check(TokensType::Identifier));
    buildTree("types", n, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the type declaration.
* @details following the grammar is parsed,
* typeDeclaration -> identifier '=' litlist
* @return void
*/
Result<void> Parser::typeDeclaration() {
    LOG_DEBUG("Parsing type declaration");
    auto result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Equal, "equal");
    if (result.isErr()) {
        return result;
    }
    result = litlist();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Semicolon, "semicolon");
    if (result.isErr()) {
        return result;
    }
    buildTree("type", 2, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the litlist.
* @details following the grammar is parsed,
*   litlist -> '(' identifier list ',' ')'
* @return void
*/
Result<void> Parser::litlist() {
    LOG_DEBUG("Parsing litlist");
    auto result = consume(TokensType::OpenParen, "open parenthesis");
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        result = identifier();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    result = consume(TokensType::CloseParen, "close parenthesis");
    if (result.isErr()) {
        return result;
    }
    buildTree("lit", n, peek().line, peek().column);
    return Result<void>::Ok();
}
/** 
* @brief Parses the subprograms.
* @details following the grammar is parsed,
* subprogs -> Fcn
* subprogs ->
* @return void
*/
Result<void> Parser::subprogs() {
    LOG_DEBUG("Parsing subprograms");
    // Check whether there are any subprograms in the program
    if(!check(TokensType::Key_function)) {
        push(new TreeNode("subprogs", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // subprogs -> Fcn+
    int n = 0;
    while(check(TokensType::Key_function)) {
        auto result = fcn();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    buildTree("subprogs", n, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the fcn statement.
* @details following the grammar is parsed,
* fcn -> 'function' <identifier> '(' Params ')' ':' <identifier> ';' Consts Types Dclns Body Name ';'
* @return void
*/
Result<void> Parser::fcn() {
    LOG_DEBUG("Parsing fcn");
    // fcn -> 'function' <identifier> '(' Params ')' ':' <identifier> ';' Consts Types Dclns Body Name ';'
    auto result = consume(TokensType::Key_function, "function");
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::OpenParen, "open parenthesis");
    if (result.isErr()) {
        return result;
    }
    result = params();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::CloseParen, "close parenthesis");
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Colon, "colon");
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Semicolon, "semicolon");
    if (result.isErr()) {
        return result;
    }
    result = consts();
    if (result.isErr()) {
        return result;
    }
    result = types();
    if (result.isErr()) {
        return result;
    }
    result = dclns();
    if (result.isErr()) {
        return result;
    }
    result = body();
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Semicolon, "semicolon");
    if (result.isErr()) {
        return result;
    }
    buildTree("fcn", 8, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the params.
* @details following the grammar is parsed,
* Params -> Dcln list ';'
* @return void
*/
Result<void> Parser::params() {
    LOG_DEBUG("Parsing params");
    // params -> Dcln list ';'
    auto result = dcln();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while (check(TokensType::Semicolon)) {
        advance(); // consume the semicolon
        result = dcln();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    buildTree("params", n, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the dcln.
* @details following the grammar is parsed,
* dcln -> <identifier> list ',' ':' <identifier> 
* @return void
*/
Result<void> Parser::dcln() {
    LOG_DEBUG("Parsing dcln");
    auto result = identifier();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while (check(TokensType::Comma)) {
        advance(); // consume the comma
        result = identifier();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    result = consume(TokensType::Colon, "colon");
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    buildTree("var", n+1, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the dclns.
* @details following the grammar is parsed,
* dclns -> 'var' (Dcln ';' )+
* dclns ->
* @return void
*/
Result<void> Parser::dclns() {
    LOG_DEBUG("Parsing declarations");
    // Check whether there are any declarations in the program
    if(!check(TokensType::Key_var)) {
        push(new TreeNode("dclns", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // dclns -> 'var' (Dcln ';')+
    auto result = consume(TokensType::Key_var, "var");
    if (result.isErr()) {
        return result;
    }
    int n = 0;
    do {
        result = dcln();
        if (result.isErr()) {
            return result;
        }
        result = consume(TokensType::Semicolon, "semicolon");
        if (result.isErr()) {
            return result;
        }
        n++;
    } while (check(TokensType::Identifier));
    buildTree("dclns", n, peek().line, peek().column);
    return Result<void>::Ok();
}

/**
* @brief Parses the body.
* @details following the grammar is parsed,
* body -> 'begin' Statement list ';' 'end'
* @return void
*/
Result<void> Parser::body() {
    LOG_DEBUG("Parsing body");
    auto result = consume(TokensType::Key_begin, "begin");
    if (result.isErr()) {
        return result;
    }
    // body -> 'begin' 'end'
    if (check(TokensType::Key_end)) {
        result = consume(TokensType::Key_end, "end");
        if (result.isErr()) {
            return result;
        }
        buildTree("block", 0, peek().line, peek().column);
        return Result<void>::Ok();
    }
    // body -> 'begin' Statement list ';' 'end'
    result = statement();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while (check(TokensType::Semicolon)) {
        advance(); // consume the semicolon
        result = statement();   // may produce <null> when next token is 'end'
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    result = consume(TokensType::Key_end, "end");
    if (result.isErr()) {
        return result;
    }
    buildTree("block", n, peek().line, peek().column);
    return Result<void>::Ok();
}

// True if the token can legally BEGIN a statement (i.e. dispatches into statement()'s switch).
bool Parser::startsStatement(TokensType type) {
    switch (type) {
        case TokensType::Identifier: case TokensType::Key_output:
        case TokensType::Key_if:     case TokensType::Key_while:
        case TokensType::Key_repeat: case TokensType::Key_for:
        case TokensType::Key_loop:   case TokensType::Key_case:
        case TokensType::Key_read:   case TokensType::Key_exit:
        case TokensType::Key_return: case TokensType::Key_begin:
            return true;
        default:
            return false;
    }
}

// True if the token can legally FOLLOW a statement, i.e. where an empty statement (<null>) is valid.
bool Parser::followsStatement(TokensType type) {
    switch (type) {
        case TokensType::Semicolon: case TokensType::Key_end:
        case TokensType::Key_until: case TokensType::Key_pool:
        case TokensType::Key_else:  case TokensType::Key_otherwise:
        case TokensType::EndOfFile:
            return true;
        default:
            return false;
    }
}

/**
* @brief Parses the statement.
* @details following the grammar is parsed,
* statement -> Assignment
* statement -> 'output'  '(' OutExp list ',' ')'
* statement -> 'if' Expression 'then' Statement ('else' Statement)?
* statement -> 'while' Expression 'do' Statement
* statement -> 'repeat' Statement list ';' 'until' Expression
* statement -> 'for' '(' ForStatement ';' ForExpression ';' ForStatement ')' Statement
* statement -> 'loop' Statement list ';' 'pool'
* statement -> 'case' Expression 'of' CaseClauses 'otherwise' CaseExpression 'end'
* statement -> 'read' '(' Identifier list ',' ')'
* statement -> 'exit'
* statement -> 'return' Expression
* statement -> Body
* statement -> <null>
* @return void
*/
Result<void> Parser::statement() {
    LOG_DEBUG("Parsing statement");
    TokensType t = peek().type;
    if (!startsStatement(t)) {
        if (followsStatement(t)) {
            // Legitimately empty statement (e.g. `begin end`, or a trailing `;` before `end`).
            // Parsing statement -> <null>
            push(new TreeNode("<null>", peek().line, peek().column));
            return Result<void>::Ok();
        }
        // The token neither starts nor can follow a statement -> a real syntax error.
        return fail("expected a statement, found " + describeCurrent());
    }
    switch (peek().type) {
        // Parsing statement -> Assignment
        case TokensType::Identifier:
            {
                auto result = assignment();
                if (result.isErr()) {
                    return result;
                }
                break;
            }
        // Parsing statement -> 'output'  '(' OutExp list ',' ')'
        case TokensType::Key_output:
            {
                auto result = consume(TokensType::Key_output, "output");
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::OpenParen, "open parenthesis");
                if (result.isErr()) {
                    return result;
                }
                int n = 1;
                result = outexp();
                if (result.isErr()) {
                    return result;
                }
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma
                    result = outexp();
                    if (result.isErr()) {
                        return result;
                    }
                    n++;
                }
                result = consume(TokensType::CloseParen, "close parenthesis");
                if (result.isErr()) {
                    return result;
                }
                buildTree("output", n, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'if' Expression 'then' Statement ('else' Statement)?
        case TokensType::Key_if:
            {
                auto result = consume(TokensType::Key_if, "if");
                if (result.isErr()) {
                    return result;
                }
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Key_then, "then");
                if (result.isErr()) {
                    return result;
                }
                result = statement();
                if (result.isErr()) {
                    return result;
                }
                if(check(TokensType::Key_else)) {
                    advance(); // consume the else keyword
                    result = statement();
                    if (result.isErr()) {
                        return result;
                    }
                    buildTree("if", 3, peek().line, peek().column);
                } else {
                    buildTree("if", 2, peek().line, peek().column);
                }
                break;
            }
        // Parsing statement -> 'while' Expression 'do' Statement
        case TokensType::Key_while:
            {
                auto result = consume(TokensType::Key_while, "while");
                if (result.isErr()) {
                    return result;
                }
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Key_do, "do");
                if (result.isErr()) {
                    return result;
                }
                result = statement();
                if (result.isErr()) {
                    return result;
                }
                buildTree("while", 2, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'repeat' Statement list ';' 'until' Expression
        case TokensType::Key_repeat:
            {
                auto result = consume(TokensType::Key_repeat, "repeat");
                if (result.isErr()) {
                    return result;
                }
                int n = 1;
                result = statement();
                if (result.isErr()) {
                    return result;
                }
                while(check(TokensType::Semicolon)) {
                    advance(); // consume the semicolon
                    result = statement();
                    if (result.isErr()) {
                        return result;
                    }
                    n++;
                }
                result = consume(TokensType::Key_until, "until");
                if (result.isErr()) {
                    return result;
                }
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                buildTree("repeat", n + 1, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'for' '(' ForStatement ';' ForExpression ';' ForStatement ')' Statement
        case TokensType::Key_for:
            {
                auto result = consume(TokensType::Key_for, "for");
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::OpenParen, "open parenthesis");
                if (result.isErr()) {
                    return result;
                }
                result = forstatement();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Semicolon, "semicolon");
                if (result.isErr()) {
                    return result;
                }
                result = forexpression();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Semicolon, "semicolon");
                if (result.isErr()) {
                    return result;
                }
                result = forstatement();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::CloseParen, "close parenthesis");
                if (result.isErr()) {
                    return result;
                }
                result = statement();
                if (result.isErr()) {
                    return result;
                }
                buildTree("for", 4, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'loop' Statement list ';' 'pool'
        case TokensType::Key_loop:
            {
                auto result = consume(TokensType::Key_loop, "loop");
                if (result.isErr()) {
                    return result;
                }
                int n = 1;
                result = statement();
                if (result.isErr()) {
                    return result;
                }
                while(check(TokensType::Semicolon)) {
                    advance(); // consume the semicolon
                    result = statement();
                    if (result.isErr()) {
                        return result;
                    }
                    n++;
                }
                result = consume(TokensType::Key_pool, "pool");
                if (result.isErr()) {
                    return result;
                }
                buildTree("loop",n, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'case' Expression 'of' CaseClauses 'otherwise' CaseExpression 'end'
        case TokensType::Key_case:
            {
                bool hasOtherwise = false;
                auto result = consume(TokensType::Key_case, "case");
                if (result.isErr()) {
                    return result;
                }
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Key_of, "of");
                if (result.isErr()) {
                    return result;
                }
                auto clauses = caseclauses(); // parse the case clauses
                if (clauses.isErr()) {
                    return Result<void>::ErrMsg(clauses.error_message.value_or("ParserError: parse error"));
                }
                int nClauses = clauses.value.value();
                hasOtherwise = check(TokensType::Key_otherwise);
                result = otherwiseclause();
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::Key_end, "end");
                if (result.isErr()) {
                    return result;
                }
                buildTree("case", 1 + nClauses + (hasOtherwise ? 1 : 0), peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'read' '(' Identifier list ',' ')'
        case TokensType::Key_read:
            {
                auto result = consume(TokensType::Key_read, "read");
                if (result.isErr()) {
                    return result;
                }
                result = consume(TokensType::OpenParen, "open parenthesis");
                if (result.isErr()) {
                    return result;
                }
                int n = 1;
                result = identifier();
                if (result.isErr()) {
                    return result;
                }
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma
                    result = identifier();
                    if (result.isErr()) {
                        return result;
                    }
                    n++;
                }
                result = consume(TokensType::CloseParen, "close parenthesis");
                if (result.isErr()) {
                    return result;
                }
                buildTree("read", n, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'exit'
        case TokensType::Key_exit:
            {
                auto result = consume(TokensType::Key_exit, "exit");
                if (result.isErr()) {
                    return result;
                }
                buildTree("exit", 0, peek().line, peek().column);
                break;
            }
        // Parsing statement -> 'return' Expression
        case TokensType::Key_return:
            {
                auto result = consume(TokensType::Key_return, "return");
                if (result.isErr()) {
                    return result;
                }
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                buildTree("return", 1, peek().line, peek().column);
                break;
            }
        // Parsing statement -> Body
        case TokensType::Key_begin:
            {
                auto result = body();
                if (result.isErr()) {
                    return result;
                }
                break;
            }
        default:
            {
            return fail("expected a statement, found " + describeCurrent());
            }
    }
    return Result<void>::Ok(); // TODO: Need to handle errors here
}

/**
* @brief Parses the case clauses.
* @details following the grammar is parsed,
* CaseClauses -> (CaseClause ';')+
* @return int: number of case clauses
*/
Result<int> Parser::caseclauses() {
    LOG_DEBUG("Parsing case clauses");
    // CaseClauses -> (CaseClause ';')+
    auto result = caseclause();
    if (result.isErr()) {
        return Result<int>::ErrMsg(result.error_message.value_or("ParserError: parse error"));
    }
    int n = 1;
    while (check(TokensType::Semicolon)) {
        advance(); // consume the semicolon
        if (check(TokensType::Key_end) || check(TokensType::Key_otherwise))
            break; // empty trailing clause before 'end'/'otherwise'
        result = caseclause();
        if (result.isErr()) {
            return Result<int>::ErrMsg(result.error_message.value_or("ParserError: parse error"));
        }
        n++;
    }
    return Result<int>::Ok(n);
}

/**
* @brief Parses the case clause.
* @details following the grammar is parsed,
* CaseClause -> CaseExpression list ',' ':' Statement
* @return void
*/
Result<void> Parser::caseclause() {
    LOG_DEBUG("Parsing case clause");
    auto result = caseexpression();
    if (result.isErr()) {
        return result;
    }
    int n = 1;
    while(check(TokensType::Comma)) {
        advance(); // consume the comma
        result = caseexpression();
        if (result.isErr()) {
            return result;
        }
        n++;
    }
    result = consume(TokensType::Colon, "colon");
    if (result.isErr()) {
        return result;
    }
    result = statement();
    if (result.isErr()) {
        return result;
    }
    buildTree("case_clause", n+1, peek().line, peek().column);
    return Result<void>::Ok();
}
/**
* @brief Parses the case expression.
* @details following the grammar is parsed,
* CaseExpression -> ConstValue
* CaseExpression -> ConstValue '..' ConstValue
* @return void
*/
Result<void> Parser::caseexpression() {
    LOG_DEBUG("Parsing case expression");
    // CaseExpression -> ConstValue
    auto result = constValue();
    if (result.isErr()) {
        return result;
    }
    // CaseExpression -> ConstValue '..' ConstValue
    if(check(TokensType::Dots)) {
        advance(); // consume the double dot
        result = constValue();
        if (result.isErr()) {
            return result;
        }
        buildTree("..", 2);
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the otherwise clause.
* @details following the grammar is parsed,
* OtherwiseClause -> 'otherwise' Expression
* @return void
*/
Result<void> Parser::otherwiseclause() {
    if (!check(TokensType::Key_otherwise))
        return Result<void>::Ok();  // ε — push nothing
    auto result = consume(TokensType::Key_otherwise, "otherwise");
    if (result.isErr()) {
        return result;
    }
    result = statement();
    if (result.isErr()) {
        return result;
    }
    buildTree("otherwise", 1);
    return Result<void>::Ok();
}


/**
* @brief Parses the for statement.
* @details following the grammar is parsed,
* ForStatement -> Assignment
* ForStatment -> 
* @return void
*/
Result<void> Parser::forstatement() {
    LOG_DEBUG("Parsing for statement");
    // ForStatement -> <null>
    if(!check(TokensType::Identifier)) {
        push(new TreeNode("<null>", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // ForStatement -> Assignment
    auto result = assignment();
    if (result.isErr()) {
        return result;
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the for expression.
* @details following the grammar is parsed,
* ForExpression -> Expression
* ForExpression ->
* @return void
*/
Result<void> Parser::forexpression() {
    // ForExpression -> <null>
    if (check(TokensType::Semicolon)) {
        push(new TreeNode("true", peek().line, peek().column));
        return Result<void>::Ok();
    }
    // ForExpression -> Expression
    auto result = expression();
    if (result.isErr()) {
        return result;
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the assignment.
* @details following the grammar is parsed,
* Assignment -> Identifier ':=' Expression
* Assignment -> Identifier ':=:' Identifier
* @return void
*/
Result<void> Parser::assignment() {
    LOG_DEBUG("Parsing assignment");
    // Parsing Identifier since both assignment and swap start with an identifier
    auto result = identifier();
    if (result.isErr()) {
        return result;
    }

    switch (peek().type) {
        // Parsing Assignment -> Identifier ':=' Expression
        case TokensType::Assignment:
            {
                advance(); // consume the assignment keyword
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                buildTree("assign", 2, peek().line, peek().column);
                break;
            }
        // Parsing Assignment -> Identifier ':=:' Identifier
        case TokensType::Swap:
            {
                advance(); // consume the swap keyword
                result = identifier();
                if (result.isErr()) {
                    return result;
                }
                buildTree("swap", 2, peek().line, peek().column);
                break;
            }
        default:
            {
                return fail("expected ':=' or ':=:', found " + describeCurrent());
            }
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the expression.
* @details following the grammar is parsed,
* Expression -> Term 
* Expression -> Term '<' Term
* Expression -> Term '>' Term
* Expression -> Term '<=' Term
* Expression -> Term '>=' Term
* Expression -> Term '=' Term
* Expression -> Term '<>' Term
* @return void
*/
Result<void> Parser::expression() {
    LOG_DEBUG("Parsing expression");
    // Expression -> Term
    auto result = term();
    if (result.isErr()) {
        return result;
    }
    while (
        check(TokensType::LessThanEqual) ||
        check(TokensType::LessThan) ||
        check(TokensType::GreaterThanEqual) ||
        check(TokensType::GreaterThan) ||
        check(TokensType::Equal) ||
        check(TokensType::NotEqual)
    ) {
        Token token = advance(); // get relational operator
        result = term();
        if (result.isErr()) {
            return result;
        }
        switch (token.type) {
            // Parsing Expression -> Term '<=' Term
            case TokensType::LessThanEqual:
                buildTree("<=", 2, peek().line, peek().column); 
                break;
            // Parsing Expression -> Term '<' Term
            case TokensType::LessThan:
                buildTree("<", 2, peek().line, peek().column);
                break;
            // Parsing Expression -> Term '>=' Term
            case TokensType::GreaterThanEqual:
                buildTree(">=", 2, peek().line, peek().column);
                break;
            // Parsing Expression -> Term '>' Term
            case TokensType::GreaterThan:
                buildTree(">", 2, peek().line, peek().column);
                break;
            // Parsing Expression -> Term '=' Term
            case TokensType::Equal:
                buildTree("=", 2, peek().line, peek().column);
                break;
            // Parsing Expression -> Term '<>' Term
            case TokensType::NotEqual:
                buildTree("<>", 2, peek().line, peek().column); 
                break;
            default:
                return fail("expected a relational operator, found " + describeCurrent());
        }
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the term.
* @details following the grammar is parsed,
* Term -> Factor
* Term -> Term '+' Factor
* Term -> Term '-' Factor
* Term -> Term 'or' Factor
* @return void
*/
Result<void> Parser::term() {
    LOG_DEBUG("Parsing term");
    // Term -> Factor
    auto result = factor();
    if (result.isErr()) {
        return result;
    }
    while(
        check(TokensType::Plus) ||
        check(TokensType::Minus) ||
        check(TokensType::Or)
    )
    {
        Token token = advance(); // get arithmetic operator
        result = factor();
        if (result.isErr()) {
            return result;
        }
        // Parsing Term -> Term '+' Factor
        if (token.type == TokensType::Plus) {
            buildTree("+", 2, peek().line, peek().column);
        } 
        // Parsing Term -> Term '-' Factor
        else if (token.type == TokensType::Minus) {
            buildTree("-", 2, peek().line, peek().column);
        } 
        // Parsing Term -> Term 'or' Factor
        else if (token.type == TokensType::Or) {
            buildTree("or", 2, peek().line, peek().column);
        } 
        else {
            return fail("expected '+', '-', or 'or', found " + describeCurrent());
        }
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the factor.
* @details following the grammar is parsed,
* Factor -> Factor '*' Primary
* Factor -> Factor '/' Primary
* Factor -> Factor 'and' Primary
* Factor -> Factor 'mod' Primary
* Factor -> Primary
* @return void
*/
Result<void> Parser::factor() {
    LOG_DEBUG("Parsing factor");
    // Factor -> Primary
    auto result = primary();
    if (result.isErr()) {
        return result;
    }
    while (
        check(TokensType::Multiply) || 
        check(TokensType::Divide) || 
        check(TokensType::And) || 
        check(TokensType::Modulus)
    ) {
        std::string op; // get the operator
        // Parsing Factor -> Factor '*' Primary
        if (check(TokensType::Multiply))      op = "*";
        // Parsing Factor -> Factor '/' Primary
        else if (check(TokensType::Divide))   op = "/";
        // Parsing Factor -> Factor 'and' Primary
        else if (check(TokensType::And))      op = "and";
        // Parsing Factor -> Factor 'mod' Primary
        else if (check(TokensType::Modulus)) op = "mod";
        else {
            return fail("expected '*', '/', 'and', or 'mod', found " + describeCurrent());
        }
        advance(); // consume the operator keyword
        result = primary();
        if (result.isErr()) {
            return result;
        }
        buildTree(op, 2, peek().line, peek().column);
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the primary.
* @details following the grammar is parsed,
* Primary -> '-' Primary
* Primary -> '+' Primary
* Primary -> 'not' Primary
* Primary -> 'eof'
* Primary -> Identifier
* Primary -> IntegerLiteral
* Primary -> CharLiteral
* Primary -> Identifier '(' Expression list ',' ')'
* Primary ->  '(' Expression ')'
* Primary -> 'succ' '(' Expression ')'
* Primary -> 'pred' '(' Expression ')'
* Primary -> 'chr' '(' Expression ')'
* Primary -> 'ord' '(' Expression ')'
* @return void
*/
Result<void> Parser::primary() {
    Result<void> result;
    switch (peek().type) {
        // Parsing Primary -> '-' Primary
        case TokensType::Minus: {
            advance(); // consume the minus keyword
            result = primary();
            if (result.isErr()) {
                return result;
            }
            buildTree("-", 1, peek().line, peek().column);
            break;
        }
        // Parsing Primary -> '+' Primary
        case TokensType::Plus: {
            advance(); // consume the plus keyword
            result = primary();
            if (result.isErr()) {
                return result;
            }
            break;
        }
        // Parsing Primary -> 'not' Primary
        case TokensType::Not: {
            advance(); // consume the not keyword
            result = primary();
            if (result.isErr()) {
                return result;
            }
            buildTree("not", 1, peek().line, peek().column);
            break;
        }
        // Parsing Primary -> 'eof'
        case TokensType::Key_eof:
        {   
            advance(); // consume the eof keyword
            push(new TreeNode("eof", peek().line, peek().column));
            break;
        }
        // Parsing Primary -> Identifier
        // Parsing Primary -> Identifier '(' Expression list ',' ')'
        case TokensType::Identifier:
        {
            result = identifier();
            if (result.isErr()) {
                return result;
            }
            // Primary -> Identifier '(' Expression list ',' ')'
            if (check(TokensType::OpenParen)) {
                advance(); // consume the open parenthesis
                int n = 1;
                result = expression();
                if (result.isErr()) {
                    return result;
                }
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma
                    result = expression();
                    if (result.isErr()) {
                        return result;
                    }
                    n++;
                }
                result = consume(TokensType::CloseParen, "close parenthesis");
                if (result.isErr()) {
                    return result;
                }
                buildTree("call", n+1, peek().line, peek().column); // n+1 because the function name is also included
            }
            break;
        }
        // Parsing Primary -> IntegerLiteral
        case TokensType::IntegerLiteral:
        {
            result = integerLiteral();
            if (result.isErr()) {
                return result;
            }
            break;
        }
        // Parsing Primary -> CharLiteral
        case TokensType::CharLiteral:
        {
            result = charLiteral();
            if (result.isErr()) {
                return result;
            }
            break;
        }
        // Parsing Primary -> '(' Expression ')'
        case TokensType::OpenParen:
        {
            advance(); // consume the open parenthesis keyword
            result = expression();
            if (result.isErr()) {
                return result;
            }
            result = consume(TokensType::CloseParen, "close parenthesis");
            if (result.isErr()) {
                return result;
            }
            break;
        }
        // Parsing Primary -> 'succ' '(' Expression ')'
        case TokensType::Key_succ:
        {
            advance(); // consume the succ keyword
            result = consume(TokensType::OpenParen, "open parenthesis");
            if (result.isErr()) {
                return result;
            }
            result = expression();
            if (result.isErr()) {
                return result;
            }
            result = consume(TokensType::CloseParen, "close parenthesis");
            if (result.isErr()) {
                return result;
            }
            buildTree("succ", 1, peek().line, peek().column);
            break;
        }
        // Parsing Primary -> 'pred' '(' Expression ')'
        case TokensType::Key_pred:
        {
            advance(); // consume the pred keyword
            result = consume(TokensType::OpenParen, "open parenthesis");
            if (result.isErr()) {
                return result;
            }
            result = expression();
            if (result.isErr()) {
                return result;
            }
            result = consume(TokensType::CloseParen, "close parenthesis");
            if (result.isErr()) {
                return result;
            }
            buildTree("pred", 1, peek().line, peek().column);
            break;
        }
        // Parsing Primary -> 'chr' '(' Expression ')'
        case TokensType::Key_chr:
        {
            advance(); // consume the chr keyword
            result = consume(TokensType::OpenParen, "open parenthesis");
            if (result.isErr()) {
                return result;
            }
            result = expression();
            if (result.isErr()) {
                return result;
            }
            result = consume(TokensType::CloseParen, "close parenthesis");
            if (result.isErr()) {
                return result;
            }
            buildTree("chr", 1, peek().line, peek().column);
            break;
        }
        // Parsing Primary -> 'ord' '(' Expression ')'
        case TokensType::Key_ord:
        {
            advance(); // consume the ord keyword
            result = consume(TokensType::OpenParen, "open parenthesis");
            if (result.isErr()) {
                return result;
            }
            result = expression();
            if (result.isErr()) {
                return result;
            }
            result = consume(TokensType::CloseParen, "close parenthesis");
            if (result.isErr()) {
                return result;
            }
            buildTree("ord", 1, peek().line, peek().column);
            break;
        }
        default:
            return fail("expected an expression, found " + describeCurrent());
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the output expression.
* @details following the grammar is parsed,
* OutExp -> StringLiteral
* OutExp -> Expression
* @return void
*/
Result<void> Parser::outexp() {
    if (check(TokensType::String)) {
        // OutExp -> StringLiteral
        auto result = stringLiteral();
        if (result.isErr()) {
            return result;
        }
        buildTree("string", 1, peek().line, peek().column);
    } else {
        // OutExp -> Expression
        auto result = expression();
        if (result.isErr()) {
            return result;
        }
        buildTree("integer", 1, peek().line, peek().column);
    }
    return Result<void>::Ok();
}

/**
* @brief Parses the string literal.
* @details following the grammar is parsed,
* StringLiteral -> '<string>' 
* @return void
*/
Result<void> Parser::stringLiteral() {
    LOG_DEBUG("Parsing string literal");
    // StringLiteral -> '<string>'
    Token token = peek();
    auto result = consume(TokensType::String, "string literal");
    if (result.isErr()) {
        return result;
    }
    TreeNode* node = new TreeNode("<string>", token.line, token.column);
    TreeNode* child = new TreeNode(token.lexeme, token.line, token.column);
    node->left = child;
    push(node);
    return Result<void>::Ok();
}

/**
* @brief Parses the integer literal.
* @details following the grammar is parsed,
* IntegerLiteral -> '<integer>'
* @return void
*/
Result<void> Parser::integerLiteral() {
    LOG_DEBUG("Parsing integer literal");
    // IntegerLiteral -> '<integer>'
    Token token = peek();
    auto result = consume(TokensType::IntegerLiteral, "integer literal");
    if (result.isErr()) {
        return result;
    }
    TreeNode* node = new TreeNode("<integer>", token.line, token.column);
    TreeNode* child = new TreeNode(token.lexeme, token.line, token.column);
    node->left = child;
    push(node);
    return Result<void>::Ok();
}

/**
* @brief Parses the char literal.
* @details following the grammar is parsed,
* CharLiteral -> '<char>'
* @return void
*/
Result<void> Parser::charLiteral() {
    LOG_DEBUG("Parsing char literal");
    // CharLiteral -> '<char>'
    Token token = peek();
    auto result = consume(TokensType::CharLiteral, "char literal");
    if (result.isErr()) {
        return result;
    }
    TreeNode* node = new TreeNode("<char>", token.line, token.column);
    std::string label = token.lexeme;
    if (label.length() == 1) {
        label = "'" + label + "'";
    }
    TreeNode* child = new TreeNode(label, token.line, token.column);
    node->left = child;
    push(node);
    return Result<void>::Ok();
}



/**
* @brief Parses the WinZigC program.
* @details following the grammar is parsed,
* WinZig -> 'program' Identifier ':' Consts Types Dclns Subprogs Body Identifier '.'
* @return void
*/
Result<void> Parser::winzig() {
    LOG_DEBUG("Parsing WinZigC program");
    // WinZig -> 'program' Identifier ':' Consts Types Dclns Subprogs Body Identifier '.'
    auto result = consume(TokensType::Key_program, "program");
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::Colon, "colon");
    if (result.isErr()) {
        return result;
    }
    result = consts();
    if (result.isErr()) {
        return result;
    }
    result = types();
    if (result.isErr()) {
        return result;
    }
    result = dclns();
    if (result.isErr()) {
        return result;
    }
    result = subprogs();
    if (result.isErr()) {
        return result;
    }
    result = body();
    if (result.isErr()) {
        return result;
    }
    result = identifier();
    if (result.isErr()) {
        return result;
    }
    result = consume(TokensType::SingleDot, "single dot");
    if (result.isErr()) {
        return result;
    }
    buildTree("program", 7, peek().line, peek().column);
    return Result<void>::Ok();
}


// ==================== Stack Operations ====================

// Push a node onto the stack.
void Parser::push(TreeNode* node) {
    stack.push_back(node);
}

// Pop a node from the stack.
TreeNode* Parser::pop() {
    if (stack.empty()) {
        LOG_ERROR("Stack is empty");
        return nullptr;
    }
    TreeNode* node = stack.back();
    stack.pop_back();
    return node;
}


// Build the tree.
void Parser::buildTree(std::string x, int n,int line, int column) {
    TreeNode* parent = nullptr;
    for (int i = 0; i < n; i++) {
        TreeNode* child = pop();
        if (child == nullptr) {
            LOG_ERROR("Child is null");
            return;
        }
        child->right = parent;
        parent = child;
    }
    push(new TreeNode(std::move(x), line, column, parent, nullptr));
}

