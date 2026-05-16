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
Parser::~Parser() = default;

// ==================== Helper Functions ====================

// Parse the WinZigC program.
Result<void> Parser::parse() {
    winzig();
    if (!isAtEnd() && peek().type != TokensType::EndOfFile) {
        LOG_ERROR("Expected end of file");
        return Result<void>::Err(ParserError("Expected end of file"));
    }
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

// Skip newlines.
void Parser::skipNewlines() {
    while (check(TokensType::Newline)) {
        advance();
    }
}

// Consume the current token if it matches the type.
Token Parser::consume(TokensType type,const std::string& what) {
    skipNewlines();
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
    skipNewlines();
    if(check(TokensType::Identifier)) {
        Token token = advance();
        LOG_INFO("Identifier found: " + token.toString());
        return; // TODO: Update the implementation
    }
    LOG_ERROR("Expected identifier but found " + peek().toString());
    return; // TODO: Update the implementation
}

// Parse the consts.
void Parser::consts() {
    LOG_INFO("Parsing constants");
    return; // TODO: Implement
}

// Parse the types.
void Parser::types() {
    LOG_INFO("Parsing types");
    return; // TODO: Implement
}

// Parse the dclns.
void Parser::dclns() {
    LOG_INFO("Parsing declarations");
    return; // TODO: Implement
}

// Parse the subprogs.
void Parser::subprogs() {
    LOG_INFO("Parsing subprograms");
    return; // TODO: Implement
}

// Parse the string literal.
void Parser::stringLiteral() {
    LOG_INFO("Parsing string literal");
    consume(TokensType::String, "string literal");
    return; // TODO: Implement
}

// Parse the output expression.
void Parser::outputExpression() {
    LOG_INFO("Parsing output expression");
    consume(TokensType::Key_output, "output");
    skipNewlines();
    consume(TokensType::OpenParen, "open parenthesis");
    skipNewlines();
    stringLiteral();
    skipNewlines();
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
    skipNewlines();
    consume(TokensType::Key_begin, "begin");
    skipNewlines();
    statement();
    skipNewlines();
    consume(TokensType::Semicolon, "semicolon");
    skipNewlines();
    consume(TokensType::Key_end, "end");
    return; // TODO: Update the implementation
}

// Parse the WinZigC program.
void Parser::winzig() {
    LOG_INFO("Parsing WinZigC program");
    consume(TokensType::Key_program, "program");
    identifier();
    consume(TokensType::Colon, "colon");
    consts();
    types();
    dclns();
    subprogs();
    body();
    identifier();
    consume(TokensType::SingleDot, "single dot");
    return; // TODO: Update the implementation
}
