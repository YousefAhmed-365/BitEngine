#ifndef BIT_LEXER_HPP
#define BIT_LEXER_HPP

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Lexical Analysis (Tokenization)
// ─────────────────────────────────────────────────────────────────────────────
// Converts raw BitScript source text into a stream of tokens.
// This is the first stage of the compilation pipeline.
// ─────────────────────────────────────────────────────────────────────────────

enum class TokenType {
    Identifier, Number, String, Keyword, Symbol, EndOfFile
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

// ─────────────────────────────────────────────────────────────────────────────
// BitLexer: Source → Tokens
// ─────────────────────────────────────────────────────────────────────────────
// Scans BitScript source code and produces tokens.
// - Recognizes keywords, identifiers, numbers, strings, and symbols
// - Tracks line numbers for error reporting
// - Validates token sequences for basic syntax errors
// ─────────────────────────────────────────────────────────────────────────────
class BitLexer {
public:
    BitLexer(const std::string& src);
    std::vector<Token> Tokenize();

private:
    std::string src;
    size_t pos;
    int line;
    bool IsKeyword(const std::string& s);
};

#endif
