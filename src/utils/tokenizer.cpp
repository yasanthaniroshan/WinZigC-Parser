#include "utils/tokenizer.h"

#include <string>

# include <cctype> // for isalpha, isdigit, etc.
#include <map> // keyword map


namespace {
    // For getting the token type for keywords. Used by `getNextToken()`. 
    TokensType getKeywordType (std::string_view id) {
        // map initialized only once at first call.
        // for better optimization, can try unordered map or hash-map or custom trie ...
        static const std::map<std::string, TokensType, std::less<>> keywords = {
            {"program", TokensType::Key_program},
            {"var", TokensType::Key_var},
            {"const", TokensType::Key_const},
            {"type", TokensType::Key_type},
            {"function", TokensType::Key_function},
            {"return", TokensType::Key_return},
            {"begin", TokensType::Key_begin},
            {"end", TokensType::Key_end},
            {"output", TokensType::Key_output},
            {"if", TokensType::Key_if},
            {"then", TokensType::Key_then},
            {"else", TokensType::Key_else},
            {"while", TokensType::Key_while},
            {"do", TokensType::Key_do},
            {"case", TokensType::Key_case},
            {"of", TokensType::Key_of},
            {"otherwise", TokensType::Key_otherwise},
            {"repeat", TokensType::Key_repeat},
            {"for", TokensType::Key_for},
            {"until", TokensType::Key_until},
            {"loop", TokensType::Key_loop},
            {"pool", TokensType::Key_pool},
            {"exit", TokensType::Key_exit},
            {"read", TokensType::Key_read},
            {"succ", TokensType::Key_succ},
            {"pred", TokensType::Key_pred},
            {"chr", TokensType::Key_chr},
            {"ord", TokensType::Key_ord},

            // also including alphabetic operators and delimiters
            {"mod", TokensType::Modulus},
            {"and", TokensType::And},
            {"or", TokensType::Or}, 
            {"not", TokensType::Not},
            {"eof", TokensType::EndOfFile}
        };

        // NOTE: we used transparent lookup (with std::less<>)
        auto iter = keywords.find(id);
        if (iter != keywords.end()) {
            return iter->second;
        }
        return TokensType::Identifier; // default to Identifier if not a keyword
    }
} // end namespace

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
        case TokensType::Key_program: return "Key_program"; // keyword
        case TokensType::Key_var: return "Key_var"; // keyword
        case TokensType::Key_const: return "Key_const"; // keyword
        case TokensType::Key_type: return "Key_type"; // keyword
        case TokensType::Key_function: return "Key_function"; // keyword
        case TokensType::Key_return: return "Key_return"; // keyword
        case TokensType::Key_begin: return "Key_begin"; // keyword
        case TokensType::Key_end: return "Key_end"; // keyword
        case TokensType::Key_output: return "Key_output"; // keyword
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
        case TokensType::Dots: return "Dots";
        case TokensType::Colon: return "Colon";
        case TokensType::Semicolon: return "Semicolon";
        case TokensType::SingleDot: return "SingleDot";
        case TokensType::Comma: return "Comma";
        case TokensType::OpenParen: return "OpenParen";
        case TokensType::CloseParen: return "CloseParen";
        default: return "Unknown";
    }
}

///// TokenizerError constructor
TokenizerError::TokenizerError(std::string message, int line, int column)
    : error_message(std::move(message)), line(line), column(column) {}

// TokenizerError message() implementation, declared in tokenizer.h
std::string TokenizerError::message() const {
    return "TokenizerError: " + error_message + 
            " at line " + std::to_string(line) + 
            ", column " + std::to_string(column);
}


///// Tokenizer implementation
Tokenizer::Tokenizer(std::string source) : source(source) {}

// Go through the source input file and return a Result type with vector of `Token`s.
Result<std::vector<Token>> Tokenizer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        Result<Token> tokenResult = nextToken();
        if (!tokenResult.success) {
            // if token has errored
            return Result<std::vector<Token>>::Err(TokenizerError(tokenResult.error_message.value(), line, column));
        }
        Token token = tokenResult.value.value();
        tokens.push_back(std::move(token));
        // `push_back()` has overload that takes rvalue and moves instead of copying.

        if (tokens.back().type == TokensType::EndOfFile) {
            break; // we've come to the end of the input file.
        }
    }
    // We need to use `std::move` here since the `Ok()` function itself uses
    // move() internally. if we only passed `Ok(tokens)` we would be 
    // passing an `lvalue` so it would be copied first anyway.
    return Result<std::vector<Token>>::Ok(std::move(tokens));
}

// Return the next recognizable token from the input file. 
// depends on skipWhitespace, isAtEnd, peek, advance, skipLineCOmment, match, 
Result<Token> Tokenizer::nextToken() {
    while (true) {
        skipWhitespace(); 

        if (isAtEnd()) {
            // We've reached the EOF
            return Result<Token>::Ok(Token(TokensType::EndOfFile, "", line, column));
        }

        int startLine = line;
        int startColumn = column;
        char c = peek(); // look at the current char without consuming it.

        // Newline
        if (c == '\n') {
            skipNewlines();
            continue;
        }

        // Single-line comment
        if (c == '#') {
            skipLineComment();
            continue;
        }

        // Multi-line comment
        if (c == '{') {
            advance(); // consume opening '{'. 
            while (!isAtEnd() && peek() != '}') {
                advance(); 
                // consume the comment until we either come to the EOF or find the ending curly.
            }
            if (isAtEnd()) {
                return Result<Token>::Err(TokenizerError("Unterminated multi-line comment", startLine, startColumn));
            }
            advance(); // consume the closing curly '}'. 
            continue;
        }

        // String literal (within "")
        if (c == '"') {
            advance(); // consume opening quote
            std::string lexeme;
            while (!isAtEnd() && peek() != '"') {
                if (peek() == '\n') {
                    return Result<Token>::Err(TokenizerError("Unterminated literal: reached newline before ending quote of literal", startLine, startColumn));
                }
                lexeme.push_back(advance()); // consume chars of the string literal
            }
            if (isAtEnd()) {
                return Result<Token>::Err(TokenizerError("Unterminated literal: reached EOF before ending quote of literal", startLine, startColumn));
            }
            advance(); // consume closing quote
            return Result<Token>::Ok(Token(TokensType::String, std::move(lexeme), startLine, startColumn));
        }

        // Character literal (within '')
        if (c == '\'') {
            advance(); // consume opening single-quote
            if (isAtEnd() || peek() == '\n') {
                return Result<Token>::Err(TokenizerError("Unterminate character literal: reached newline before end quote", startLine, startColumn));
            }
            char ch_lexeme = advance(); // consume the character literal
            if (peek() != '\'') {
                return Result<Token>::Err(TokenizerError("Invalid literal: Character literal should end with a single-quote", startLine, startColumn));
            }
            advance(); // consume closing single-quote
            return Result<Token>::Ok(Token(TokensType::CharLiteral, std::string(1, ch_lexeme), startLine, startColumn));
        }

        // integers - no need to handle negative or floating points
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string int_lexeme; // NOTE: Storing as string
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
                int_lexeme.push_back(advance()); // consume the digits of the integer literal
            }
            return Result<Token>::Ok(Token(TokensType::IntegerLiteral, std::move(int_lexeme), startLine, startColumn));
        }

        // identifiers and keywords
        if (std::isalpha(static_cast<unsigned char>(c))) {
            // identifiers must necessarily begin with an alphabetic character
            std::string lexeme;
            while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                // NOTE: use of `isalnum` since identifier can have digits too
                lexeme.push_back(advance());
            }

            TokensType type = getKeywordType(lexeme);
            return Result<Token>::Ok(Token(type, std::move(lexeme), startLine, startColumn));
        }

        // Operators and delimiters (non-alphanumeric stuff). 
        char first = advance(); // consume first char of unidentified token.
        switch (first) {

            case ':':
                // could be one of three cases ":", ":=", ":=:". 
                if (match('=')) {
                    // now could be either ":=" or ":=:"
                    if (match(':')) {
                        // means definitely ":=:"
                        return Result<Token>::Ok(Token(TokensType::Swap, ":=:", startLine, startColumn));
                    } else {
                        // means definitely ":="
                        return Result<Token>::Ok(Token(TokensType::Assignment, ":=", startLine, startColumn));
                    }
                } else {
                    // means must be just a colon :
                    return Result<Token>::Ok(Token(TokensType::Colon, ":", startLine, startColumn));
                }
            // end :

            case '<':
                // could be one of three cases "<=", "<>", "<"
                if (match('=')) return Result<Token>::Ok(Token(TokensType::LessThanEqual, "<", startLine, startColumn));
                if (match('>')) return Result<Token>::Ok(Token(TokensType::NotEqual, "<>", startLine, startColumn));
                return Result<Token>::Ok(Token(TokensType::LessThan, "<", startLine, startColumn));
            // end <

            case '>':
                // could be one of two cases ">=", ">"
                if (match('=')) return Result<Token>::Ok(Token(TokensType::GreaterThanEqual, ">=", startLine, startColumn));
                return Result<Token>::Ok(Token(TokensType::GreaterThan, ">", startLine, startColumn));
            // end >

            case '.':
                // could be one of two cases "..", "."
                if (match('.')) return Result<Token>::Ok(Token(TokensType::Dots, "..", startLine, startColumn));
                return Result<Token>::Ok(Token(TokensType::SingleDot, ".", startLine, startColumn));
            // end .

            case '=':
                return Result<Token>::Ok(Token(TokensType::Equal, "=", startLine, startColumn));
            // end =

            case '+':
                return Result<Token>::Ok(Token(TokensType::Plus, "+", startLine, startColumn));
            // end +

            case '-':
                return Result<Token>::Ok(Token(TokensType::Minus, "-", startLine, startColumn));
            // end -

            case '*':
                return Result<Token>::Ok(Token(TokensType::Multiply, "*", startLine, startColumn));
            // end *

            case '/':
                return Result<Token>::Ok(Token(TokensType::Divide, "/", startLine, startColumn));
            // end /

            case ';':
                return Result<Token>::Ok(Token(TokensType::Semicolon, ";", startLine, startColumn));
            // end ;

            case ',':
                return Result<Token>::Ok(Token(TokensType::Comma, ",", startLine, startColumn));
            // end ,

            case '(':
                return Result<Token>::Ok(Token(TokensType::OpenParen, "(", startLine, startColumn));
            // end (

            case ')':
                return Result<Token>::Ok(Token(TokensType::CloseParen, ")", startLine, startColumn));
            // end )
                
            default:
                return Result<Token>::Err(TokenizerError("Unexpected character: '" + std::string(1, first) + "'", startLine, startColumn));
        }

    }
}

// Take a look at the current character in the input source without "consuming" it.
char Tokenizer::peek() const {
    if (isAtEnd()) {
        return '\0';
    }
    return source[current];
}

// Look at the next character in the input source without consuming.
char Tokenizer::peekNext() const {
    if (current + 1 >= source.size()) return '\0';

    return source[current + 1];
}

// Check whether `current` is pointing to EOF
bool Tokenizer::isAtEnd() const {
    return (current >= source.size());
}

// Move to the next character by consuming the current character
char Tokenizer::advance() {
    // Since we use >= for `isAtEnd` we can increment `current` here without
    // checking as `nextToken` uses `isAtEnd` for finding end of input
    char c = source[current++];

    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

// Check whether the current character matches the `expected` character.
// NOTE: Will consume the current character in the process of checking. Returns
// false by default if at the end of the source input. 
bool Tokenizer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    // NOTE: CONSUMES CHARACTER
    advance();
    return true;
}

void Tokenizer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = source[current]; // since we already check `isAtEnd()` using 
        // `peek()` would be redundant

        if (c == ' ' || c == '\t' || c == '\r') advance();
        else break;
    }
}

// We use this when `source[current]` is detected as '#'. 
void Tokenizer::skipLineComment() {
    while (!isAtEnd()) {
        if (peek() != '\n') advance();
        else break;
    }
    // advance(); // We don't consume the final newline character.
}

void Tokenizer::skipNewlines() {
    while (!isAtEnd() && peek() == '\n') advance();
}