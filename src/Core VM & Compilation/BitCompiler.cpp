#include "BitCompiler.hpp"
#include "BitRuntime.hpp"
#include <fstream>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Compilation Pipeline
// ─────────────────────────────────────────────────────────────────────────────
// Three-stage compilation: Lexing → Parsing → Bytecode Generation
// ─────────────────────────────────────────────────────────────────────────────

bool BitCompiler::CompileFile(const std::string& path, BitProject& project) {
    std::ifstream file(path);
    if (!file) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return CompileString(buffer.str(), project);
}

bool BitCompiler::CompileString(const std::string& source, BitProject& project) {
    // Stage 1: Lexical Analysis
    std::vector<Token> tokens;
    if (!Lex(source, tokens)) {
        return false;
    }
    
    // Stage 2: Parsing & Bytecode Generation
    if (!Parse(tokens, project)) {
        return false;
    }
    
    return true;
}

bool BitCompiler::Lex(const std::string& source, std::vector<Token>& tokens) {
    BitLexer lexer(source);
    tokens = lexer.Tokenize();
    return true;
}

bool BitCompiler::Parse(const std::vector<Token>& tokens, BitProject& project) {
    BitParser parser(tokens, project);
    parser.Parse();
    return project.parseErrors.empty();
}
