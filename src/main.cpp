#include "headers/BitApp.hpp"

int main(int argc, char** argv) {
    std::string projectPath = (argc > 1) ? argv[1] : "res/configs.json";
    BitApp app(projectPath);
    app.Run();
    return 0;
}