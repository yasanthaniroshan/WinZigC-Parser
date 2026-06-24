#ifndef PARSER_H
#define PARSER_H

#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"
#include "utils/diagnostics.h"
#include "utils/tree.h"
#include "tokenizer/tokenizer.h"

struct ParserError : public Error {
    std::string msg;
    int line;
    int column;
    ParserError(std::string m, int l = -1, int c = -1) : msg(std::move(m)), line(l), column(c) {}
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
    std::optional<ParserError> syntaxError; // The single error that stopped parsing (first one wins).
    size_t current = 0; // current position in tokens vector

    // A human-readable description of the current token, for error messages (e.g. "'2'", "end of input").
    std::string describeCurrent() const;
    // Records the syntax error at the current token. Only the first is kept, since parsing can't recover.
    void recordError(const std::string& message);
    // Records an error at the current token and returns a failing Result for propagation.
    Result<void> fail(const std::string& message);


    bool isAtEnd() const; // Check if the end of the tokens is reached.
    bool check(TokensType type) const; // Check if the current token matches the type.
    bool match(const std::vector<TokensType>& types); // check if the current token matches any of the types

    static bool startsStatement(TokensType type); // True if the token can begin a statement.
    static bool followsStatement(TokensType type); // True if the token can legally follow a statement (empty statement allowed).

    void skipNewlines(); // Skip newlines.
    
    Token peek() const; // Peek at the current token.
    Token previous() const; // Get the previous token.
    Token advance(); // Advance to the next token.
    Result<void> consume(TokensType type,const std::string& what); // Consume the current token; records an error on mismatch.
    
    // Parsing functions. Each returns Result<void>: Ok on success, Err to unwind the
    // recursive-descent stack once a syntax error has been recorded in `syntaxError`.
    Result<void> winzig(); // Entry point for parsing the WinZigC program.

    Result<void> consts(); // Parse the consts statement.
    Result<void> constDeclaration(); // Parse the const declaration.
    Result<void> constValue(); // Parse the const value.

    Result<void> types(); // Parse the types statement.
    Result<void> typeDeclaration(); // Parse the type declaration.
    Result<void> litlist(); // Parse the litlist.

    Result<void> subprogs(); // Parse the subprogs statement.
    Result<void> fcn(); // Parse the fcn statement.
    Result<void> params(); // Parse the params statement.
    Result<void> dcln(); // Parse the dcln statement.
    Result<void> dclns(); // Parse the dclns statement.
    Result<void> body(); // Parse the body statement.
    Result<void> statement(); // Parse the statement.
    Result<void> forstatement(); // Parse the for statement.
    Result<void> forexpression(); // Parse the for expression.
    Result<void> assignment(); // Parse the assignment.
    Result<void> outexp(); // Parse the output expression.
    Result<int> caseclauses(); // Parse the case clauses; carries the clause count.
    Result<void> otherwiseclause(); // Parse the otherwise clause.
    Result<void> caseclause(); // Parse the case clause.
    Result<void> caseexpression(); // Parse the case expression.

    Result<void> expression(); // Parse the expression.
    Result<void> term(); // Parse the term.
    Result<void> factor(); // Parse the factor.
    Result<void> primary(); // Parse the primary.

    Result<void> identifier(); // Parse the identifier.
    Result<void> stringLiteral(); // Parse the string literal.

    Result<void> integerLiteral(); // Parse the integer literal.
    Result<void> charLiteral(); // Parse the char literal.

    // Stack Operations
    void push(TreeNode* node); // Push a node onto the stack.
    TreeNode* pop(); // Pop a node from the stack.

    void buildTree(std::string x, int n,int line = -1, int column = -1); // Build the tree.
};
#endif