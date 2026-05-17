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
    // for (TreeNode* node : stack) {
    //     delete node;
    // }
}

// ==================== Helper Functions ====================

Result<TreeNode*> Parser::parseTree() {
    winzig();
    if (!isAtEnd() && peek().type != TokensType::EndOfFile) {
        LOG_ERROR("Expected end of file");
        return Result<TreeNode*>::Err(ParserError("Expected end of file"));
    }
    if (stack.empty()) {
        return Result<TreeNode*>::Err(ParserError("empty parse stack"));
    }
    return Result<TreeNode*>::Ok(stack.back());
}

// Parse the WinZigC program.
Result<void> Parser::parse() {
    auto result = parseTree();
    if (!result.success) {
        return Result<void>::Err(ParserError(result.error_message.value_or("parse failed")));
    }
    printTree(result.value.value(), 0);
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
    // do {
    //     fcn(); // parse the function declaration
    //     n++; // increment the number of function declarations
    // } while (check(TokensType::Identifier));
    while(check(TokensType::Key_function)) {
        fcn(); // parse the function declaration
        n++; // increment the number of function declarations
    }
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
    buildTree("fcn", 8);
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
    buildTree("var", n+1);
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

/**
* @brief Parses the body.
* @details following the grammar is parsed,
* body -> 'begin' Statement list ';' 'end'
* @return void
*/
void Parser::body() {
    LOG_INFO("Parsing body");
    consume(TokensType::Key_begin, "begin");
    // Check if the body is empty
    if (check(TokensType::Key_end)) {
        // If the body is empty, push a block node to the stack
        // Parsing body -> 'begin' 'end'
        consume(TokensType::Key_end, "end"); // consume the end keyword
        buildTree("block", 0);
        return; // TODO: Need to handle errors here
    }
    // If the body is not empty, parse the body
    // Parsing body -> 'begin' Statement list ';' 'end'
    int n = 0;
    while(
        check(TokensType::Identifier) ||
        check(TokensType::Key_output) ||
        check(TokensType::Key_if) ||
        check(TokensType::Key_while) ||
        check(TokensType::Key_repeat) ||
        check(TokensType::Key_for) ||
        check(TokensType::Key_loop) ||
        check(TokensType::Key_case) ||
        check(TokensType::Key_read) ||
        check(TokensType::Key_exit) ||
        check(TokensType::Key_return) ||
        check(TokensType::Key_begin)
    ) {
        // Parse the statement
        statement();
        if(check(TokensType::Semicolon)) {
            advance(); // consume the semicolon
        }
        n++; // increment the number of statements
    }
    consume(TokensType::Key_end, "end"); // consume the end keyword
    buildTree("block", n); // Build the tree for the body statement
    return; // TODO: Need to handle errors here
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
void Parser::statement() {
    LOG_INFO("Parsing statement");
    // Check if the statement is empty
    if (!(check(TokensType::Identifier) || check(TokensType::Key_output) || check(TokensType::Key_if) || check(TokensType::Key_while) || check(TokensType::Key_repeat) || check(TokensType::Key_for) || check(TokensType::Key_loop) || check(TokensType::Key_case) || check(TokensType::Key_read) || check(TokensType::Key_exit) || check(TokensType::Key_return) || check(TokensType::Key_begin))) {
        // If the statement is empty, push a null node to the stack
        // Parsing statement -> <null>
        push(new TreeNode("<null>"));
        return; // TODO: Need to handle errors here
    }
    switch (peek().type) {
        // Parsing statement -> Assignment
        case TokensType::Identifier:
            {
                assignment();
                break;
            }
        // Parsing statement -> 'output'  '(' OutExp list ',' ')'
        case TokensType::Key_output:
            {
                consume(TokensType::Key_output, "output");
                consume(TokensType::OpenParen, "open parenthesis");
                int n = 1;
                outexp(); // parse the output expression
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma
                    outexp(); // parse the output expression
                    n++; // increment the number of output expressions
                }
                consume(TokensType::CloseParen, "close parenthesis");
                buildTree("output",n);
                break;
            }
        // Parsing statement -> 'if' Expression 'then' Statement ('else' Statement)?
        case TokensType::Key_if:
            {
                consume(TokensType::Key_if, "if"); // consume the if keyword
                expression(); // parse the expression
                consume(TokensType::Key_then, "then"); // consume the then keyword
                statement();
                if(check(TokensType::Key_else)) {
                    advance(); // consume the else keyword
                    statement();
                    buildTree("if", 3);
                } else {
                    buildTree("if", 2);
                }
                break;
            }
        // Parsing statement -> 'while' Expression 'do' Statement
        case TokensType::Key_while:
            {
                consume(TokensType::Key_while, "while"); // consume the while keyword
                expression(); // parse the expression
                consume(TokensType::Key_do, "do"); // consume the do keyword
                statement();
                buildTree("while", 2);
                break;
            }
        // Parsing statement -> 'repeat' Statement list ';' 'until' Expression
        case TokensType::Key_repeat:
            {
                consume(TokensType::Key_repeat, "repeat"); // consume the repeat keyword
                int n = 1;
                statement(); // parse the statement
                while(check(TokensType::Semicolon)) {
                    advance(); // consume the semicolon
                    statement(); // parse the statement
                    n++; // increment the number of statements
                }
                consume(TokensType::Key_until, "until"); // consume the until keyword
                expression();
                buildTree("repeat", n + 1);
                break;
            }
        // Parsing statement -> 'for' '(' ForStatement ';' ForExpression ';' ForStatement ')' Statement
        case TokensType::Key_for:
            {
                consume(TokensType::Key_for, "for"); // consume the for keyword
                consume(TokensType::OpenParen, "open parenthesis");
                forstatement();
                consume(TokensType::Semicolon, "semicolon");
                forexpression();
                consume(TokensType::Semicolon, "semicolon");
                forstatement();
                consume(TokensType::CloseParen, "close parenthesis");
                statement();
                buildTree("for", 4);
                break;
            }
        // Parsing statement -> 'loop' Statement list ';' 'pool'
        case TokensType::Key_loop:
            {
                consume(TokensType::Key_loop, "loop"); // consume the loop keyword
                int n = 1;
                statement();
                while(check(TokensType::Semicolon)) {
                    advance(); // consume the semicolon
                    statement(); // parse the statement
                    n++; // increment the number of statements
                }
                consume(TokensType::Key_pool, "pool");
                buildTree("loop",n);
                break;
            }
        // Parsing statement -> 'case' Expression 'of' CaseClauses 'otherwise' CaseExpression 'end'
        case TokensType::Key_case:
            {
                consume(TokensType::Key_case, "case"); // consume the case keyword
                expression(); // parse the expression
                consume(TokensType::Key_of, "of"); // consume the of keyword
                caseclauses(); // parse the case clauses
                otherwiseclause(); // parse the otherwise clause
                consume(TokensType::Key_end, "end");
                buildTree("case", 3);
                break;
            }
        // Parsing statement -> 'read' '(' Identifier list ',' ')'
        case TokensType::Key_read:
            {
                consume(TokensType::Key_read, "read"); // consume the read keyword
                consume(TokensType::OpenParen, "open parenthesis");
                int n = 1;
                identifier(); // parse the identifier
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma
                    identifier(); // parse the identifier
                    n++; // increment the number of identifiers
                }
                consume(TokensType::CloseParen, "close parenthesis");
                buildTree("read", n);
                break;
            }
        // Parsing statement -> 'exit'
        case TokensType::Key_exit:
            {
                consume(TokensType::Key_exit, "exit"); // consume the exit keyword
                buildTree("exit", 0);
                break;
            }
        // Parsing statement -> 'return' Expression
        case TokensType::Key_return:
            {
                consume(TokensType::Key_return, "return"); // consume the return keyword
                expression(); // parse the expression
                buildTree("return", 1);
                break;
            }
        // Parsing statement -> Body
        case TokensType::Key_begin:
            {
                body();
                break;
            }
        default:
            {
            LOG_ERROR("Expected output expression, if expression, while expression, repeat expression, for expression, loop expression, case expression, read expression, exit expression, return expression, or begin expression but found " + peek().toString());
            return;
            }
    }
}

/**
* @brief Parses the case clauses.
* @details following the grammar is parsed,
* CaseClauses -> (CaseClause ';')+
* @return void
*/
void Parser::caseclauses() {
    LOG_INFO("Parsing case clauses");
    // Parsing CaseClauses -> (CaseClause ';')+
    caseclause(); // parse the case clause
    while (check(TokensType::Semicolon)) {
        advance(); // consume the semicolon
        if (check(TokensType::Key_end) || check(TokensType::Key_otherwise))
            break; // if the end or otherwise keyword is found, break the loop
        caseclause(); // parse the case clause
    }
    buildTree("case_clauses", 1); // Build the tree for the case clauses statement
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the case clause.
* @details following the grammar is parsed,
* CaseClause -> CaseExpression list ',' ':' Statement
* @return void
*/
void Parser::caseclause() {
    LOG_INFO("Parsing case clause");
    caseexpression();
    int n = 1;
    while(check(TokensType::Comma)) {
        advance(); // consume the comma
        caseexpression();
        n++; // increment the number of case expressions
    }
    consume(TokensType::Colon, "colon");
    statement();
    buildTree("case_clause", n+1);
    return; // TODO: Need to handle errors here
}
/**
* @brief Parses the case expression.
* @details following the grammar is parsed,
* CaseExpression -> ConstValue
* CaseExpression -> ConstValue '..' ConstValue
* @return void
*/
void Parser::caseexpression() {
    LOG_INFO("Parsing case expression");
    // Since both case expression and const value start with a const value, parse the const value
    // Parsing CaseExpression -> ConstValue
    constValue();
    // Check if the case expression is followed by a double dot
    if(check(TokensType::Dots)) {
        // Parsing CaseExpression -> ConstValue '..' ConstValue
        advance(); // consume the double dot
        constValue();
        buildTree("..", 2);
    } else {
        buildTree("const", 1);
    }
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the otherwise clause.
* @details following the grammar is parsed,
* OtherwiseClause -> 'otherwise' Expression
* @return void
*/
void Parser::otherwiseclause() {
    if (!check(TokensType::Key_otherwise))
        return;  // ε — push nothing
    consume(TokensType::Key_otherwise, "otherwise");
    statement();
    buildTree("otherwise", 1);
}


/**
* @brief Parses the for statement.
* @details following the grammar is parsed,
* ForStatement -> Assignment
* ForStatment -> 
* @return void
*/
void Parser::forstatement() {
    LOG_INFO("Parsing for statement");
    // Check if the for statement is empty
    if(!check(TokensType::Identifier)) {
        // Parsing ForStatement -> <null>
        push(new TreeNode("<null>"));
        return; // TODO: Need to handle errors here
    }
    // Parsing ForStatement -> Assignment
    assignment();
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the for expression.
* @details following the grammar is parsed,
* ForExpression -> Expression
* ForExpression ->
* @return void
*/
void Parser::forexpression() {
    // check if the for expression is empty
    if (check(TokensType::Semicolon)) {
        // Parsing ForExpression -> <null>
        push(new TreeNode("true"));
        return;
    }
    // Parsing ForExpression -> Expression
    expression();
}

/**
* @brief Parses the assignment.
* @details following the grammar is parsed,
* Assignment -> Identifier ':=' Expression
* Assignment -> Identifier ':=:' Identifier
* @return void
*/
void Parser::assignment() {
    LOG_INFO("Parsing assignment");
    // Parsing Identifier since both assignment and swap start with an identifier
    identifier();

    switch (peek().type) {
        // Parsing Assignment -> Identifier ':=' Expression
        case TokensType::Assignment:
            {
                advance(); // consume the assignment keyword
                expression(); // parse the expression
                buildTree("assign", 2);
                break;
            }
        // Parsing Assignment -> Identifier ':=:' Identifier
        case TokensType::Swap:
            {
                advance(); // consume the swap keyword
                identifier(); // parse the identifier
                buildTree("swap", 2);
                break;
            }
        default:
            {
                LOG_ERROR("Expected identifier but found " + peek().toString());
                return;
            }
    }
    return; // TODO: Need to handle errors here
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
void Parser::expression() {
    LOG_INFO("Parsing expression");
    // Parsing Expression -> Term
    // Parse term since all expressions start with a term
    term();
    while (
        check(TokensType::LessThanEqual) ||
        check(TokensType::LessThan) ||
        check(TokensType::GreaterThanEqual) ||
        check(TokensType::GreaterThan) ||
        check(TokensType::Equal) ||
        check(TokensType::NotEqual)
    ) {
        Token token = advance(); // get relational operator
        term(); // parse the term
        switch (token.type) {
            // Parsing Expression -> Term '<=' Term
            case TokensType::LessThanEqual:
                buildTree("<=", 2); 
                break;
            // Parsing Expression -> Term '<' Term
            case TokensType::LessThan:
                buildTree("<", 2);
                break;
            // Parsing Expression -> Term '>=' Term
            case TokensType::GreaterThanEqual:
                buildTree(">=", 2);
                break;
            // Parsing Expression -> Term '>' Term
            case TokensType::GreaterThan:
                buildTree(">", 2);
                break;
            // Parsing Expression -> Term '=' Term
            case TokensType::Equal:
                buildTree("=", 2);
                break;
            // Parsing Expression -> Term '<>' Term
            case TokensType::NotEqual:
                buildTree("<>", 2); 
                break;
            default:
                LOG_ERROR("Expected relational operator but found " + token.toString());
                return; // TODO: Need to handle errors here
        }
    }
    return; // TODO: Need to handle errors here
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
void Parser::term() {
    LOG_INFO("Parsing term");
    // Parsing Term -> Factor
    factor();
    while(
        check(TokensType::Plus) ||
        check(TokensType::Minus) ||
        check(TokensType::Or)
    )
    {
        Token token = advance(); // get arithmetic operator
        factor(); // parse the factor
        // Parsing Term -> Term '+' Factor
        if (token.type == TokensType::Plus) {
            buildTree("+", 2);
        } 
        // Parsing Term -> Term '-' Factor
        else if (token.type == TokensType::Minus) {
            buildTree("-", 2);
        } 
        // Parsing Term -> Term 'or' Factor
        else if (token.type == TokensType::Or) {
            buildTree("or", 2);
        } 
        else {
            LOG_ERROR("Expected plus, minus, or or but found " + token.toString());
            return; // TODO: Need to handle errors here
        }
    }
    return; // TODO: Need to handle errors here
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
void Parser::factor() {
    LOG_INFO("Parsing factor");
    // Parsing Factor -> Primary
    primary(); // parse the primary

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
            LOG_ERROR("Expected multiply, divide, and, or, or modulus but found " + peek().toString());
            return; // TODO: Need to handle errors here
        }
        advance(); // consume the operator keyword
        primary(); // parse the primary
        buildTree(op, 2);
    }
    return; // TODO: Need to handle errors here
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
void Parser::primary() {
    switch (peek().type) {
        // Parsing Primary -> '-' Primary
        case TokensType::Minus: {
            advance(); // consume the minus keyword
            primary(); // parse the primary
            buildTree("-", 1);
            break;
        }
        // Parsing Primary -> '+' Primary
        case TokensType::Plus: {
            advance(); // consume the plus keyword
            primary(); // parse the primary
            buildTree("+", 1);
            break;
        }
        // Parsing Primary -> 'not' Primary
        case TokensType::Not: {
            advance(); // consume the not keyword
            primary();
            buildTree("not", 1);
            break;
        }
        // Parsing Primary -> 'eof'
        case TokensType::EndOfFile:
        {   
            advance(); // consume the eof keyword
            push(new TreeNode("eof"));
            break;
        }
        // Parsing Primary -> Identifier
        // Parsing Primary -> Identifier '(' Expression list ',' ')'
        case TokensType::Identifier:
        {
            identifier(); // parse the identifier
            // Check if the identifier is followed by a open parenthesis
            if (check(TokensType::OpenParen)) {
                // Parsing Primary -> Identifier '(' Expression list ',' ')'
                advance(); // consume the open parenthesis keyword
                int n = 1;
                expression(); // parse the expression
                // Check if the expression is followed by a comma
                while(check(TokensType::Comma)) {
                    advance(); // consume the comma keyword
                    expression(); // parse the expression
                    n++; // increment the number of expressions
                }
                consume(TokensType::CloseParen, "close parenthesis");
                buildTree("call", n+1); // n+1 because the function name is also included
            }
            break;
        }
        // Parsing Primary -> IntegerLiteral
        case TokensType::IntegerLiteral:
        {
            integerLiteral(); // parse the integer literal
            break;
        }
        // Parsing Primary -> CharLiteral
        case TokensType::CharLiteral:
        {
            charLiteral(); // parse the char literal
            break;
        }
        // Parsing Primary -> '(' Expression ')'
        case TokensType::OpenParen:
        {
            advance(); // consume the open parenthesis keyword
            expression(); // parse the expression
            consume(TokensType::CloseParen, "close parenthesis");
            break;
        }
        // Parsing Primary -> 'succ' '(' Expression ')'
        case TokensType::Key_succ:
        {
            advance(); // consume the succ keyword
            consume(TokensType::OpenParen, "open parenthesis");
            expression(); // parse the expression
            consume(TokensType::CloseParen, "close parenthesis");
            buildTree("succ", 1);
            break;
        }
        // Parsing Primary -> 'pred' '(' Expression ')'
        case TokensType::Key_pred:
        {
            advance(); // consume the pred keyword
            consume(TokensType::OpenParen, "open parenthesis");
            expression(); // parse the expression
            consume(TokensType::CloseParen, "close parenthesis"); // consume the close parenthesis keyword
            buildTree("pred", 1);
            break;
        }
        // Parsing Primary -> 'chr' '(' Expression ')'
        case TokensType::Key_chr:
        {
            advance(); // consume the chr keyword
            consume(TokensType::OpenParen, "open parenthesis");
            expression(); // parse the expression
            consume(TokensType::CloseParen, "close parenthesis");
            buildTree("chr", 1);
            break;
        }
        // Parsing Primary -> 'ord' '(' Expression ')'
        case TokensType::Key_ord:
        {
            advance(); // consume the ord keyword
            consume(TokensType::OpenParen, "open parenthesis");
            expression(); // parse the expression
            consume(TokensType::CloseParen, "close parenthesis");
            buildTree("ord", 1);
            break;
        }
        default:
            LOG_ERROR("expected primary expression, got " + peek().toString());
            return;
    }
}

/**
* @brief Parses the output expression.
* @details following the grammar is parsed,
* OutExp -> StringLiteral
* OutExp -> Expression
* @return void
*/
void Parser::outexp() {
    if (check(TokensType::String)) {
        // OutExp -> StringNode => "string"
        stringLiteral();
        buildTree("string", 1);
    } else {
        // Parsing OutExp -> Expression
        expression();  // parse the expression
        buildTree("integer", 1);  // build the tree for the output expression
    }
}

/**
* @brief Parses the string literal.
* @details following the grammar is parsed,
* StringLiteral -> '<string>' 
* @return void
*/
void Parser::stringLiteral() {
    LOG_INFO("Parsing string literal");
    // Parsing StringLiteral -> '<string>'
    Token token = consume(TokensType::String, "string literal");
    TreeNode* node = new TreeNode("<string>");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the integer literal.
* @details following the grammar is parsed,
* IntegerLiteral -> '<integer>'
* @return void
*/
void Parser::integerLiteral() {
    LOG_INFO("Parsing integer literal");
    // Parsing IntegerLiteral -> '<integer>'
    Token token = consume(TokensType::IntegerLiteral, "integer literal");
    TreeNode* node = new TreeNode("<integer>");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
    return; // TODO: Need to handle errors here
}

/**
* @brief Parses the char literal.
* @details following the grammar is parsed,
* CharLiteral -> '<char>'
* @return void
*/
void Parser::charLiteral() {
    LOG_INFO("Parsing char literal");
    // Parsing CharLiteral -> '<char>'
    Token token = consume(TokensType::CharLiteral, "char literal");
    TreeNode* node = new TreeNode("<char>");
    TreeNode* child = new TreeNode(token.lexeme);
    node->left = child;
    push(node);
    return; // TODO: Need to handle errors here
}



/**
* @brief Parses the WinZigC program.
* @details following the grammar is parsed,
* WinZig -> 'program' Identifier ':' Consts Types Dclns Subprogs Body Identifier '.'
* @return void
*/
void Parser::winzig() {
    LOG_INFO("Parsing WinZigC program");
    consume(TokensType::Key_program, "program"); // consume the program keyword
    identifier(); // parse the identifier
    consume(TokensType::Colon, "colon");
    consts(); // parse the consts
    types(); // parse the types
    dclns(); // parse the declarations
    subprogs(); // parse the subprograms
    body(); // parse the body
    identifier(); // parse the identifier
    consume(TokensType::SingleDot, "single dot");
    buildTree("program", 7); // Build the tree for the WinZigC program
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

