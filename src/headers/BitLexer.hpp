#ifndef BIT_LEXER_HPP
#define BIT_LEXER_HPP

#include <string>
#include <vector>

enum class TokenType {
    Identifier, Number, String, Keyword, Symbol, EndOfFile
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

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
