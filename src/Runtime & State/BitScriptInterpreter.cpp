#include "BitScriptInterpreter.hpp"
#include <fstream>
#include <sstream>

bool BitScriptInterpreter::LoadScriptFile(const std::string& path, BitProject& p) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    
    return ParseScriptString(buffer.str(), p);
}

bool BitScriptInterpreter::ParseScriptString(const std::string& src, BitProject& p) {
    BitLexer lexer(src);
    auto tokens = lexer.Tokenize();
    
    BitParser parser(tokens, p);
    parser.Parse();
    
    return p.parseErrors.empty();
}
