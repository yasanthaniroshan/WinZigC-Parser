#ifndef PARSER_H
#define PARSER_H

#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/tokenizer.h"


struct ParserError : public Error {
    std::string msg;
    ParserError(std::string m) : msg(std::move(m)) {}
    std::string message() const override { return "ParserError: " + msg; }
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    ~Parser();
    Result<void> parse();

private:
    std::vector<Token> tokens; // The tokens to parse.
    size_t current = 0; // current position in tokens vector

    bool isAtEnd() const; // Check if the end of the tokens is reached.
    bool check(TokensType type) const; // Check if the current token matches the type.
    bool match(const std::vector<TokensType>& types); // check if the current token matches any of the types

    void skipNewlines(); // Skip newlines.
    
    Token peek() const; // Peek at the current token.
    Token previous() const; // Get the previous token.
    Token advance(); // Advance to the next token.
    Token consume(TokensType type,const std::string& what); // Consume the current token if it matches the type.
    
    // Parsing functions
    void winzig(); // Entry point for parsing the WinZigC program.
    void program(); // Parse the program statement.
    void consts(); // Parse the consts statement.
    void types(); // Parse the types statement.
    void dclns(); // Parse the dclns statement.
    void subprogs(); // Parse the subprogs statement.
    void body(); // Parse the body statement.
    void statement(); // Parse the statement.
    void identifier(); // Parse the identifier.
    void outputExpression(); // Parse the output expression.
    void stringLiteral(); // Parse the string literal.
};
#endif