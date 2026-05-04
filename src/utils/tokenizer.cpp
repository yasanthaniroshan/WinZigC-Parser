#include "utils/tokenizer.h"

#include <string>

// TokenizerError constructor
TokenizerError::TokenizerError(std::string message, int line, int column)
    : error_message(std::move(message)), line(line), column(column) {}

// TokenizerError message() implementation, declared in tokenizer.h
std::string TokenizerError::message() const {
    return "TokenizerError: " + error_message + 
            " at line " + std::to_string(line) + 
            ", column " + std::to_string(column);
}


// For printing out the tokens 
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