#include "headers/BitCompiler.hpp"
#include "headers/BitEngine.hpp"
#include <fstream>
#include <sstream>

bool BitCompiler::CompileFile(const std::string& path, DialogProject& project) {
    std::ifstream file(path);
    if (!file) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return CompileString(buffer.str(), project);
}

bool BitCompiler::CompileString(const std::string& source, DialogProject& project) {
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

bool BitCompiler::Parse(const std::vector<Token>& tokens, DialogProject& project) {
    BitParser parser(tokens, project);
    parser.Parse();
    return project.parseErrors.empty();
}
