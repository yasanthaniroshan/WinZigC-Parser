// include/utils/tokenizer.h
/**
 * @file tokenizer.h
 * @brief Tokenizer class
 * @version 0.1
 * @date today
 *
 * @copyright Copyright (c) 2026
 */

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <string_view>
#include <vector>
#include <cctype>
#include <map>

#include <common/result.h>
#include <common/error.h>
#include <utils/logger.h>

// NOTE: "TokenType" will clash with another variable from some `winnt.h` or something. 
// Hence, we use "TokensType" instead. It is left as an exercise to the reader to
// visualize an apostraphe before the "s". 
enum class TokensType {
    // special
    EndOfFile,  // EOF
    Unknown,    // catch-all for unrecognized tokens (not included in spec)
    Newline,    // newlines

    // comments
    CommentTypeOne, // begins with # and ends with newline
    CommentTypeTwo, // begins with { and contines with any character (including EOL), ends with }

    // identifiers and literals
    Identifier,     // function names, variable names, etc.
    IntegerLiteral, // integer literals
    String,         // string literals enclosed in double-quotes
    CharLiteral,    // single character enclosed in single-quotes

    // keywords
    Key_program,    // start of program
    Key_var,        // variable 
    Key_const,      // constant
    Key_type,       // to define a datatype
    Key_function,   // to define a function 
    Key_return,     // return from a function
    Key_begin,      // start of a block
    Key_end,        // end of a block
    Key_output,     // output an expression/string
    Key_if,         
    Key_then,
    Key_else,
    Key_while,
    Key_do,
    Key_case,
    Key_of,
    Key_otherwise,  // for case expressions? // ??? NOT SPECIFIED
    Key_repeat,     // for repeat-until loop
    Key_for, 
    Key_until,      // for repeat-until loop
    Key_loop,       // for loop pool
    Key_pool,       // for loop pool - pool would indicate end of loop?
    Key_exit, 
    Key_read,       // specification says "read an identifier".. maybe for taking input? 
    Key_succ,       // return the successor of an ordinal value (successor of an integer, of a list, etc.??)
    Key_pred,       // return the predecessor of an ordinal value, similar idea as succ I guess
    Key_chr,        // specification just says "character function"
    Key_ord,        // specification just says "ordinal function"

    // operators
    Swap,               // :=:
    Assignment,         // :=
    LessThanEqual,      // <=
    NotEqual,           // <>
    LessThan,           // <
    GreaterThanEqual,   // >=
    GreaterThan,        // >
    Equal,              // =
    Modulus,            // mod
    And,                // and
    Or,                 // or
    Not,                // not unary operator
    Plus,               // +
    Minus,              // -
    Multiply,           // *
    Divide,             // /

    // delimiters
    Dots,   // .. for case expression
    Colon,      // :
    Semicolon,  // ;
    SingleDot,  // .
    Comma,      // ,
    OpenParen,  // (
    CloseParen  // )
};

// For printing out the tokens 
std::string tokenTypeToString(TokensType type);

// Token struct 
struct Token {
    TokensType type;
    std::string lexeme;
    int line;
    int column;

    // constructor
    Token (TokensType type, std::string lexeme, int line, int column) 
        : type(type), lexeme(std::move(lexeme)), line(line), column(column) {}

    // For ease of debugging
    std::string toString() const {
        return "<" + tokenTypeToString(type) + " | " + lexeme + " | line:" + std::to_string(line) + " | column:" + std::to_string(column) + " >"; 
    }
};


// Custom error type for specifically tokenizer errors.
struct TokenizerError : Error {
    explicit TokenizerError(std::string message, int line, int column);
    std::string message() const override;

private:
    std::string error_message;
    int line;
    int column;
};


// Tokenizer class definition. 
class Tokenizer {
public:
    // string_view is a non-owning reference (pointer + length) to a string.
    explicit Tokenizer(std::string_view source);

    // the actual tokenizing function. returns vector of Token structs.
    Result<std::vector<Token>> tokenize();
    Result<Token> nextToken();

private:
    std::string_view source;
    size_t current = 0; // current position in source string (internal state of tokenizer)
    int column = 1;
    int line = 1;

    char peek() const;
    char peekNext() const;
    bool isAtEnd() const;
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipLineComment();
};

#endif