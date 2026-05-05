#ifndef BIT_SCRIPT_INTERPRETER_HPP
#define BIT_SCRIPT_INTERPRETER_HPP

#include "BitLexer.hpp"
#include "BitParser.hpp"
#include "BitRuntime.hpp"
#include <string>

/**
 * BitScriptInterpreter: Legacy compiler interface
 * 
 * Combines lexing, parsing, and compilation into a single interface.
 * This is primarily a convenience wrapper now that these are separated.
 */
class BitScriptInterpreter {
public:
    static bool LoadScriptFile(const std::string& path, BitProject& p);
    static bool ParseScriptString(const std::string& src, BitProject& p);
};

#endif
