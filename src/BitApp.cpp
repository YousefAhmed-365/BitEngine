#include "headers/BitApp.hpp"
#include "headers/BitEngine.hpp"
#include "headers/BitRenderer.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>

#include <chrono>
#include <filesystem>
#include <cstring>

using json = nlohmann::json;

static const char* ENGINE_VERSION = "0.1";
static const char* ENGINE_NAME    = "BitEngine";

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string defaultOutput(const std::string& input) {
    auto dot = input.rfind('.');
    if (dot != std::string::npos) return input.substr(0, dot) + ".bitc";
    return input + ".bitc";
}

BitApp::BitApp() {
    LoadConfig("res/app.json");
}

BitApp::~BitApp() {
    if (IsWindowReady()) CloseWindow();
}

void BitApp::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[BitApp] Warning: " << path << " not found, using default app configuration." << std::endl;
        return;
    }
    try {
        json j; f >> j;
        m_title     = j.value("title", m_title);
        m_width     = j.value("width", m_width);
        m_height    = j.value("height", m_height);
        m_minWidth  = j.value("min_width", m_minWidth);
        m_minHeight = j.value("min_height", m_minHeight);
        m_fps       = j.value("fps", m_fps);
        m_resizable = j.value("resizable", m_resizable);
    } catch (const std::exception& e) {
        std::cerr << "[BitApp] Failed to parse " << path << ": " << e.what() << std::endl;
    }
}

int BitApp::ProcessArgs(int argc, char** argv) {
    if (argc == 1) {
        Run("res_bitscript/main.bitscript");
        return 0;
    }

    const char* arg1 = argv[1];

    if (strcmp(arg1, "-h") == 0 || strcmp(arg1, "--help") == 0) {
        PrintHelp(argv[0]);
        return 0;
    }

    if (strcmp(arg1, "-v") == 0 || strcmp(arg1, "--version") == 0) {
        std::cout << ENGINE_NAME << " v" << ENGINE_VERSION << "\n";
        return 0;
    }

    if (strcmp(arg1, "-c") == 0 || strcmp(arg1, "--compile") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --compile requires a source file.\n"; return 1; }
        std::string src = argv[2];
        if (!endsWith(src, ".bitscript")) { std::cerr << "[ERROR] --compile only supports .bitscript source files.\n"; return 1; }
        std::string dst = (argc >= 4) ? argv[3] : defaultOutput(src);
        return DoCompile(src, dst);
    }

    if (strcmp(arg1, "-d") == 0 || strcmp(arg1, "--dry-run") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --dry-run requires a file.\n"; return 1; }
        if (!endsWith(argv[2], ".bitscript")) { std::cerr << "[ERROR] --dry-run only supports .bitscript files.\n"; return 1; }
        return DoDryRun(argv[2]);
    }

    if (strcmp(arg1, "-l") == 0 || strcmp(arg1, "--list-scenes") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --list-scenes requires a file.\n"; return 1; }
        return DoListScenes(argv[2]);
    }

    if (strcmp(arg1, "-s") == 0 || strcmp(arg1, "--stats") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --stats requires a file.\n"; return 1; }
        return DoStats(argv[2]);
    }

    if (strcmp(arg1, "-r") == 0 || strcmp(arg1, "--run") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --run requires a file.\n"; return 1; }
        Run(argv[2]);
        return 0;
    }

    if (arg1[0] != '-') {
        Run(argv[1]);
        return 0;
    }

    std::cerr << "[ERROR] Unknown option: " << arg1 << "\nRun '" << argv[0] << " --help' for usage.\n";
    return 1;
}

void BitApp::PrintHelp(const char* argv0) {
    std::cout <<
        "\n"
        "  ██████╗ ██╗████████╗███████╗███╗   ██╗ ██████╗ ██╗███╗   ██╗███████╗\n"
        "  ██╔══██╗██║╚══██╔══╝██╔════╝████╗  ██║██╔════╝ ██║████╗  ██║██╔════╝\n"
        "  ██████╔╝██║   ██║   █████╗  ██╔██╗ ██║██║  ███╗██║██╔██╗ ██║█████╗  \n"
        "  ██╔══██╗██║   ██║   ██╔══╝  ██║╚██╗██║██║   ██║██║██║╚██╗██║██╔══╝  \n"
        "  ██████╔╝██║   ██║   ███████╗██║ ╚████║╚██████╔╝██║██║ ╚████║███████╗\n"
        "  ╚═════╝ ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝╚══════╝\n"
        "\n"
        "  Narrative Scripting Engine  v" << ENGINE_VERSION << "\n"
        "\n"
        "USAGE:\n"
        "  " << argv0 << " [options] [file]\n\n"
        "OPTIONS:\n"
        "  -h, --help                 Show this help message and exit\n"
        "  -v, --version              Print version information and exit\n\n"
        "  -r, --run <file>           Run a .bitscript or .bitc file\n"
        "  -c, --compile <src> [dst]  Compile a .bitscript to bytecode (.bitc)\n"
        "  -d, --dry-run <file>       Parse/validate a .bitscript without launching a window\n"
        "  -l, --list-scenes <file>   List all scene labels in a .bitscript file\n"
        "  -s, --stats <file>         Print instruction count and variable stats\n\n";
}

int BitApp::DoCompile(const std::string& src, const std::string& dst) {
    auto startTime = std::chrono::steady_clock::now();
    std::cout << "┌─ " << ENGINE_NAME << " v" << ENGINE_VERSION << " | Compiling: " << src << "\n";
    DialogEngine engine;
    if (!engine.LoadProject(src)) {
        std::cerr << "└─ [ERROR] Failed to parse script.\n";
        for (const auto& e : engine.GetErrors()) std::cerr << "   - " << e << "\n";
        return 1;
    }

    // Static Analysis
    auto validation = engine.ValidateProject(engine.GetProject());
    if (!validation.errors.empty()) {
        std::cout << "│  \n├─ Issues:\n";
        bool hasErrors = false;
        for (const auto& e : validation.errors) {
            std::cout << "│  " << e << "\n";
            if (e.find("[ERROR]") != std::string::npos) hasErrors = true;
        }
        if (hasErrors) {
            std::cerr << "└─ [ERROR] Compilation aborted due to static analysis errors.\n";
            return 1;
        }
    }
    if (!engine.SaveBytecode(dst)) {
        std::cerr << "└─ [ERROR] Failed to write binary: " << dst << "\n";
        return 1;
    }
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    const auto& proj = engine.GetProject();
    const auto& bc   = proj.bytecode;
    int labels=0, says=0, choices=0;
    for (const auto& ins : bc) {
        if (ins.op == BitOp::LABEL) ++labels;
        else if (ins.op == BitOp::SAY) ++says;
        else if (ins.op == BitOp::CHOICE) ++choices;
    }
    auto sz = std::filesystem::file_size(dst);
    std::cout << "│  \n├─ Compilation Successful!\n│  Output     : " << dst << " (" << sz << " bytes)\n│  Time       : " << elapsed << "ms\n│  \n"
              << "├─ Project Summary:\n│  - Instructions : " << bc.size() << "\n│  - Scenes       : " << labels << "\n│  - Dialogue     : " << says << " lines\n│  - Choices      : " << choices << "\n│  - Variables    : " << proj.variables.size() << "\n│  - Entities     : " << proj.entities.size() << "\n│  - Assets       : " << (proj.backgrounds.size() + proj.music.size() + proj.sfx.size()) << " registered\n└──────────────────────────────────────────────────────────────────\n";
    return 0;
}

int BitApp::DoDryRun(const std::string& path) {
    auto startTime = std::chrono::steady_clock::now();
    std::cout << "┌─ " << ENGINE_NAME << " v" << ENGINE_VERSION << " | Validating: " << path << "\n";
    DialogEngine engine;
    if (!engine.LoadProject(path)) {
        std::cerr << "└─ [ERROR] Failed to parse script.\n";
        for (const auto& e : engine.GetErrors()) std::cerr << "   - " << e << "\n";
        return 1;
    }
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    const auto& proj = engine.GetProject();
    const auto& bc   = proj.bytecode;
    int labels=0, says=0, choices=0;
    for (const auto& ins : bc) {
        if (ins.op == BitOp::LABEL) ++labels;
        else if (ins.op == BitOp::SAY) ++says;
        else if (ins.op == BitOp::CHOICE) ++choices;
    }
    auto validation = engine.ValidateProject(engine.GetProject());
    auto errs = validation.errors;
    if (errs.empty()) {
        std::cout << "│  \n├─ Validation Successful! (No errors found)\n│  Time       : " << elapsed << "ms\n│  \n├─ Project Summary:\n│  - Instructions : " << bc.size() << "\n│  - Scenes       : " << labels << "\n│  - Dialogue     : " << says << " lines\n│  - Choices      : " << choices << "\n│  - Variables    : " << proj.variables.size() << "\n└──────────────────────────────────────────────────────────────────\n";
        return 0;
    } else {
        std::cerr << "│  \n├─ [WARN] " << errs.size() << " issue(s) found:\n";
        for (const auto& e : errs) std::cerr << "│  - " << e << "\n";
        std::cerr << "└──────────────────────────────────────────────────────────────────\n";
        return 1;
    }
}

int BitApp::DoListScenes(const std::string& path) {
    DialogEngine engine;
    if (!engine.LoadProject(path)) { std::cerr << "[ERROR] Failed to parse: " << path << "\n"; return 1; }
    const auto& bc = engine.GetProject().bytecode;
    std::cout << "[BitEngine] Scene labels in " << path << ":\n";
    int count = 0;
    for (const auto& ins : bc) if (ins.op == BitOp::LABEL) { std::cout << "  " << ins.args[0] << "\n"; ++count; }
    std::cout << "  (" << count << " total)\n";
    return 0;
}

int BitApp::DoStats(const std::string& path) {
    DialogEngine engine;
    if (!engine.LoadProject(path)) { std::cerr << "[ERROR] Failed to parse: " << path << "\n"; return 1; }
    const auto& proj = engine.GetProject();
    const auto& bc   = proj.bytecode;
    int labels=0, says=0, choices=0, gotos=0, events=0, sets=0, ifs=0;
    for (const auto& ins : bc) {
        switch (ins.op) {
            case BitOp::LABEL:  ++labels;  break;
            case BitOp::SAY:    ++says;    break;
            case BitOp::CHOICE: ++choices; break;
            case BitOp::GOTO:   ++gotos;   break;
            case BitOp::EVENT:  ++events;  break;
            case BitOp::IF:
            case BitOp::IF_REF: ++ifs;     break;
            case BitOp::SET: case BitOp::SET_REF:
            case BitOp::ADD: case BitOp::ADD_REF:
            case BitOp::SUB: case BitOp::SUB_REF:
            case BitOp::MUL: case BitOp::MUL_REF:
            case BitOp::DIV: case BitOp::DIV_REF: ++sets; break;
            default: break;
        }
    }
    std::cout << "[BitEngine] Stats for: " << path << "\n" << "  Instructions total : " << bc.size() << "\n" << "  Scenes (labels)    : " << labels << "\n" << "  Dialogue lines     : " << says << "\n" << "  Choices            : " << choices << "\n" << "  Jumps              : " << gotos << "\n" << "  Events             : " << events << "\n" << "  Variable ops       : " << sets << "\n" << "  Conditionals       : " << ifs << "\n" << "  Variables defined  : " << proj.variables.size() << "\n" << "  Entities defined   : " << proj.entities.size() << "\n" << "  Backgrounds        : " << proj.backgrounds.size() << "\n" << "  Music tracks       : " << proj.music.size() << "\n";
    return 0;
}

void BitApp::Run(const std::string& projectPath) {
    SetTraceLogLevel(LOG_NONE);
    if (m_resizable) SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(m_width, m_height, m_title.c_str());
    SetWindowMinSize(m_minWidth, m_minHeight);
    SetTargetFPS(m_fps);

    DialogEngine dialogSystem;
    BitRenderer uiBridge(dialogSystem);
    bool loaded = dialogSystem.LoadProject(projectPath);
    uiBridge.GetStyleManager().LoadStyle("res/style.json");
    uiBridge.PreloadAssets();
    if (loaded) dialogSystem.StartDialog();

    while (!WindowShouldClose()) {
        dialogSystem.Update(GetFrameTime());
        if (IsKeyPressed(KEY_TAB)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) uiBridge.GetStyleManager().PrevStyle();
            else uiBridge.GetStyleManager().NextStyle();
        }
        uiBridge.HandleInput();
        BeginDrawing();
        ClearBackground(Color{ 10, 10, 15, 255 });
        for (int i = 0; i < GetScreenWidth();  i += 80) DrawLine(i, 0, i, GetScreenHeight(), Fade(Color{ 80, 80, 120, 255 }, 0.05f));
        for (int i = 0; i < GetScreenHeight(); i += 80) DrawLine(0, i, GetScreenWidth(), i,  Fade(Color{ 80, 80, 120, 255 }, 0.05f));
        DrawText(TextFormat("BITENGINE v%s", ENGINE_VERSION), 30, 30, 22, Color{ 80, 80, 120, 255 });
        uiBridge.Draw();
        
        {
            std::string styleName = uiBridge.GetStyleManager().GetCurrentStyleName();
            auto names = uiBridge.GetStyleManager().GetStyleNames();
            int total = (int)names.size(), idx = 0;
            for (int i = 0; i < total; ++i) if (names[i] == styleName) { idx = i; break; }
            std::string label = "STYLE [" + std::to_string(idx + 1) + "/" + std::to_string(total) + "]: " + styleName;
            int tw = MeasureText(label.c_str(), 14);
            DrawRectangle(20, GetScreenHeight() - 46, tw + 24, 30, Fade(BLACK, 0.6f));
            DrawRectangleLines(20, GetScreenHeight() - 46, tw + 24, 30, Fade(WHITE, 0.15f));
            DrawText(label.c_str(), 32, GetScreenHeight() - 38, 14, Color{ 160, 160, 200, 255 });
            DrawText("Tab / Shift+Tab to switch", 32, GetScreenHeight() - 18, 11, Fade(Color{ 120, 120, 160, 255 }, 0.8f));
        }
        EndDrawing();
    }
}
