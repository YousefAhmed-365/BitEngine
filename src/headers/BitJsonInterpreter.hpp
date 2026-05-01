#ifndef BIT_JSON_INTERPRETER_HPP
#define BIT_JSON_INTERPRETER_HPP

#include "BitEngine.hpp"
#include <string>

class BitJsonInterpreter {
public:
    static DialogProject ParseConfig(const std::string& path);
    static bool LoadDialogFile(const std::string& path, DialogProject& p);
    static bool LoadEntitiesFile(const std::string& path, DialogProject& p);
    static bool LoadVariablesFile(const std::string& path, DialogProject& p);
    static bool LoadAssetsFile(const std::string& path, DialogProject& p);
};

#endif
