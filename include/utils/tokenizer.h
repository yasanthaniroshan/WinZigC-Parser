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
    Key_dots,   // .. for case expression
    Colon,      // :
    Semicolon,  // ;
    SingleDot,  // .
    Comma,      // ,
    OpenParen,  // (
    CloseParen  // )
};

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

// Can move this into `tokenizer.cpp` when we have one. 
std::string tokenTypeToString(TokensType type) {
    switch (type) {
        case TokensType::EndOfFile: return "EndOfFile";
        // case TokensType::Unknown: return "Unknown"; // handled as the default at the end
        case TokensType::Newline: return "Newline";
        case TokensType::CommentTypeOne: return "CommentTypeOneSingleLine";
        case TokensType::CommentTypeTwo: return "CommentTypeTwoMultiLine";
        case TokensType::Identifier: return "Identifier";
        case TokensType::IntegerLiteral: return "IntegerLiteral";
        case TokensType::String: return "String";
        case TokensType::CharLiteral: return "CharLiteral";
        case TokensType::Key_program: return "Key_program";
        case TokensType::Key_var: return "Key_var";
        case TokensType::Key_const: return "Key_const";
        case TokensType::Key_type: return "Key_type";
        case TokensType::Key_function: return "Key_function";
        case TokensType::Key_return: return "Key_return";
        case TokensType::Key_begin: return "Key_begin";
        case TokensType::Key_end: return "Key_end";
        case TokensType::Key_output: return "Key_output";
        case TokensType::Key_if: return "Key_if";
        case TokensType::Key_then: return "Key_then";
        case TokensType::Key_else: return "Key_else";
        case TokensType::Key_while: return "Key_while";
        case TokensType::Key_do: return "Key_do";
        case TokensType::Key_case: return "Key_case";
        case TokensType::Key_of: return "Key_of";
        case TokensType::Key_otherwise: return "Key_otherwise";
        case TokensType::Key_repeat: return "Key_repeat";
        case TokensType::Key_for: return "Key_for";
        case TokensType::Key_until: return "Key_until";
        case TokensType::Key_loop: return "Key_loop";
        case TokensType::Key_pool: return "Key_pool";
        case TokensType::Key_exit: return "Key_exit";
        case TokensType::Key_read: return "Key_read";
        case TokensType::Key_succ: return "Key_succ";
        case TokensType::Key_pred: return "Key_pred";
        case TokensType::Key_chr: return "Key_chr";
        case TokensType::Key_ord: return "Key_ord";
        case TokensType::Swap: return "Swap";
        case TokensType::Assignment: return "Assignment";
        case TokensType::LessThanEqual: return "LessThanEqual";
        case TokensType::NotEqual: return "NotEqual";
        case TokensType::LessThan: return "LessThan";
        case TokensType::GreaterThanEqual: return "GreaterThanEqual";
        case TokensType::GreaterThan: return "GreaterThan";
        case TokensType::Equal: return "Equal";
        case TokensType::Modulus: return "Modulus";
        case TokensType::And: return "And";
        case TokensType::Or: return "Or";
        case TokensType::Not: return "Not";
        case TokensType::Plus: return "Plus";
        case TokensType::Minus: return "Minus";
        case TokensType::Multiply: return "Multiply";
        case TokensType::Divide: return "Divide";
        case TokensType::Key_dots: return "Key_dots";
        case TokensType::Colon: return "Colon";
        case TokensType::Semicolon: return "Semicolon";
        case TokensType::SingleDot: return "SingleDot";
        case TokensType::Comma: return "Comma";
        case TokensType::OpenParen: return "OpenParen";
        case TokensType::CloseParen: return "CloseParen";
        default: return "Unknown";
    }
}

#endif