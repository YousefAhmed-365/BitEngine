#ifndef BIT_PARSER_HPP
#define BIT_PARSER_HPP

#include "BitLexer.hpp"
#include "BitOp.hpp"
#include <string>
#include <vector>

// Include BitEngine for BitProject definition
#include "BitRuntime.hpp"

struct Operand {
    bool isRef;
    std::string val;
};

class BitParser {
public:
    BitParser(const std::vector<Token>& tokens, BitProject& p);
    void Parse();

private:
    std::vector<Token> tokens;
    BitProject& p;
    size_t pos;
    int tempVarCount;
    std::string currentScene;
    std::vector<BitInstruction>* m_currentOutput;
    
    Token peek();
    Token peekNext();
    Token consume();
    bool match(TokenType t, const std::string& v = "");
    void expect(TokenType t, const std::string& v = "");
    
    std::string genTempVar();
    void emit(BitOp op, std::vector<std::string> args = {}, nlohmann::json meta = {});
    
    void ParseConfig();
    void ParseVariable();
    void ParseEntities();
    void ParseAssets();
    void ParseEvent();
    void ParseScene();
    std::string ParseTimeline();
    void ParseStatement();
    void ParseAssignment(const std::string& var, std::vector<BitInstruction>& output, bool isLocal = false);
    void ParseDialogueBlock(const std::string& entityId, std::vector<BitInstruction>& output);
    
    // Expression Parsing
    Operand ParseExpression(std::vector<BitInstruction>& output);
    Operand ParseAddExpr(std::vector<BitInstruction>& output);
    Operand ParseMulExpr(std::vector<BitInstruction>& output);
    Operand ParsePrimary();
    
    // Condition Parsing
    void ParseIfStatement(std::vector<BitInstruction>& output);
    
    int ParseTime(const std::string& s);
    Token previous();
};

#endif
