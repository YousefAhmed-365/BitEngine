#ifndef BIT_SCRIPT_INTERPRETER_HPP
#define BIT_SCRIPT_INTERPRETER_HPP

#include "BitEngine.hpp"
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

class BitScriptLexer {
public:
    BitScriptLexer(const std::string& src);
    std::vector<Token> Tokenize();

private:
    std::string src;
    size_t pos;
    int line;
    bool IsKeyword(const std::string& s);
};

struct Operand {
    bool isRef;
    std::string val;
};

class BitScriptParser {
public:
    BitScriptParser(const std::vector<Token>& tokens, DialogProject& p);
    void Parse();

private:
    std::vector<Token> tokens;
    DialogProject& p;
    size_t pos;
    int tempVarCount;
    std::string currentScene;
    
    Token peek();
    Token peekNext();
    Token consume();
    bool match(TokenType t, const std::string& v = "");
    void expect(TokenType t, const std::string& v = "");
    
    std::string genTempVar();
    
    void ParseConfig();
    void ParseVariable();
    void ParseEntities();
    void ParseAssets();
    void ParseScene();
    
    // Expression Parsing
    Operand ParseExpression(std::vector<BitInstruction>& output);
    Operand ParseAddExpr(std::vector<BitInstruction>& output);
    Operand ParseMulExpr(std::vector<BitInstruction>& output);
    Operand ParsePrimary();
    
    // Condition Parsing
    void ParseIfStatement(std::vector<BitInstruction>& output);
    
    Token previous();
};

class BitScriptInterpreter {
public:
    static bool LoadScriptFile(const std::string& path, DialogProject& p);
};

#endif
