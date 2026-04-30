#include "BitEngine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "--- BitEngine Project Compiler ---" << std::endl;
    
    std::string configPath = "res/configs.json";
    std::string outputPath = "data.bin";

    if (argc > 1) configPath = argv[1];
    if (argc > 2) outputPath = argv[2];

    DialogEngine engine;
    std::cout << "[BitTool] Loading project from: " << configPath << std::endl;
    
    if (engine.LoadProject(configPath)) {
        std::cout << "[BitTool] Success! Compiling to: " << outputPath << std::endl;
        engine.CompileProject(outputPath);
        std::cout << "[BitTool] Done." << std::endl;
    } else {
        std::cerr << "[BitTool] ERROR: Failed to load project from " << configPath << std::endl;
        return 1;
    }

    return 0;
}
