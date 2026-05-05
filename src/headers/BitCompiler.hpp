#ifndef BIT_COMPILER_HPP
#define BIT_COMPILER_HPP

#include "BitLexer.hpp"
#include "BitParser.hpp"
#include "BitOp.hpp"
#include <string>

// Forward declaration
class DialogProject;

/**
 * BitCompiler: Compilation pipeline for BitScript
 * 
 * Orchestrates:
 * - Lexical analysis (BitLexer)
 * - Parsing (BitParser)
 * - Bytecode generation
 * 
 * Provides a simple interface: source text → bytecode
 */
class BitCompiler {
public:
    // Compile from file
    static bool CompileFile(const std::string& path, DialogProject& project);
    
    // Compile from string
    static bool CompileString(const std::string& source, DialogProject& project);

private:
    // Internal pipeline stages
    static bool Lex(const std::string& source, std::vector<Token>& tokens);
    static bool Parse(const std::vector<Token>& tokens, DialogProject& project);
};

#endif
