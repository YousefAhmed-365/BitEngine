#ifndef BIT_PARSER_HPP
#define BIT_PARSER_HPP

#include "BitLexer.hpp"
#include "BitOp.hpp"
#include <string>
#include <vector>

// Include BitEngine for BitProject definition
#include "BitRuntime.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Syntax Analysis & Bytecode Generation
// ─────────────────────────────────────────────────────────────────────────────
// Parses token stream into an abstract syntax tree and generates bytecode.
// This is the second and third stages of the compilation pipeline.
// ─────────────────────────────────────────────────────────────────────────────

struct Operand {
    bool isRef;
    std::string val;
    bool isString = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// BitParser: Tokens → Bytecode
// ─────────────────────────────────────────────────────────────────────────────
// Parses token stream according to BitScript grammar rules.
// Produces BitOperation bytecode instructions and populates BitProject.
//
// Parsing stages:
//   1. Configuration (set statements, entity defs, asset paths)
//   2. Timeline definitions (animated event sequences)
//   3. Scene blocks (dialogue, branching, conditionals)
//   4. Expression evaluation (operators, variable references)
// ─────────────────────────────────────────────────────────────────────────────
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
    
    void ParseVariable();
    void ParseEvent();
    void ParseScene();
    std::string ParseTimeline();
    void ParseStatement();
    void ParseAssignment(const std::string& var, std::vector<BitInstruction>& output, bool isLocal = false);
    void ParseDialogueBlock(const std::string& entityId, std::vector<BitInstruction>& output);
    
    // Expression Parsing
    Operand ParseExpression(std::vector<BitInstruction>& output);
    Operand ParseTernaryExpr(std::vector<BitInstruction>& output);
    Operand ParseAddExpr(std::vector<BitInstruction>& output);
    Operand ParseMulExpr(std::vector<BitInstruction>& output);
    Operand ParsePrimary();
    
    // Condition Parsing
    void ParseIfStatement(std::vector<BitInstruction>& output);
    
    int ParseTime(const std::string& s);
    Token previous();

    // Helpers for new syntax
    std::string ParseAssetId();        // bare id OR {var} OR "string"
    Operand     ParseCinematicArg();   // bare literal/id OR (expr)
    std::string ParseModifierValue();  // bare value OR (cond ? a : b) ternary
    void        ParseModifierBlock(nlohmann::json& meta); // parse [...] block
};

#endif
