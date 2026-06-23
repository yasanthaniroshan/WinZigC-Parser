#ifndef PARSER_H
#define PARSER_H

#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/tree.h"
#include "tokenizer/tokenizer.h"

struct ParserError : public Error {
    std::string msg;
    ParserError(std::string m) : msg(std::move(m)) {}
    std::string message() const override { return "ParserError: " + msg; }
};

class Parser {
public: 
    Parser(const std::vector<Token>& tokens);
    ~Parser();
    Result<TreeNode*> parse(bool printAbstractSyntaxTree = false);
    Result<TreeNode*> parseTree();

private:
    std::vector<Token> tokens; // The tokens to parse.
    std::vector<TreeNode*> stack; // The stack of nodes.
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

    void consts(); // Parse the consts statement.
    void constDeclaration(); // Parse the const declaration.
    void constValue(); // Parse the const value.
    
    void types(); // Parse the types statement.
    void typeDeclaration(); // Parse the type declaration.
    void litlist(); // Parse the litlist.
     
    void subprogs(); // Parse the subprogs statement.
    void fcn(); // Parse the fcn statement.
    void params(); // Parse the params statement.
    void dcln(); // Parse the dcln statement.
    void dclns(); // Parse the dclns statement.
    void body(); // Parse the body statement.
    void statement(); // Parse the statement.
    void forstatement(); // Parse the for statement.
    void forexpression(); // Parse the for expression.
    void assignment(); // Parse the assignment.
    void outexp(); // Parse the output expression.
    int caseclauses(); // Parse the case clauses. Need to find how many case clauses are there.
    void otherwiseclause(); // Parse the otherwise clause.
    void caseclause(); // Parse the case clause.
    void caseexpression(); // Parse the case expression.

    void expression(); // Parse the expression.
    void term(); // Parse the term.
    void factor(); // Parse the factor.
    void primary(); // Parse the primary.

    void identifier(); // Parse the identifier.
    void stringLiteral(); // Parse the string literal.

    void integerLiteral(); // Parse the integer literal.
    void charLiteral(); // Parse the char literal.

    // Stack Operations
    void push(TreeNode* node); // Push a node onto the stack.
    TreeNode* pop(); // Pop a node from the stack.

    void buildTree(std::string x, int n); // Build the tree.
};
#endif