/**
 * BitEngine — Narrative Scripting Engine
 * Entry point with full CLI support.
 *
 * Usage:
 *   BitEngine [options] [file]
 *
 *
 * If no option is given and a file is provided it is run directly.
 * The default project file is "res_bitscript/main.bitscript".
 */

#include "headers/BitApp.hpp"
#include "headers/BitEngine.hpp"
#include "headers/BitScriptInterpreter.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <filesystem>

// ── version ──────────────────────────────────────────────────────────────────
static const char* ENGINE_VERSION = "2.1.0";
static const char* ENGINE_NAME    = "BitEngine";

// ── helpers ───────────────────────────────────────────────────────────────────
static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string defaultOutput(const std::string& input) {
    // Replace extension with .bitc
    auto dot = input.rfind('.');
    if (dot != std::string::npos) return input.substr(0, dot) + ".bitc";
    return input + ".bitc";
}

// ── help text ─────────────────────────────────────────────────────────────────
static void printHelp(const char* argv0) {
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
        "  " << argv0 << " [options] [file]\n"
        "\n"
        "  If no option is given, the file is run directly.\n"
        "  Default file: res_bitscript/main.bitscript\n"
        "\n"
        "OPTIONS:\n"
        "  -h, --help                 Show this help message and exit\n"
        "  -v, --version              Print version information and exit\n"
        "\n"
        "  -r, --run <file>           Run a .bitscript or .bitc file\n"
        "  -c, --compile <src> [dst]  Compile a .bitscript to bytecode (.bitc)\n"
        "                             If [dst] is omitted, writes <src>.bitc\n"
        "  -d, --dry-run <file>       Parse/validate a .bitscript without launching\n"
        "                             a window. Prints any errors found.\n"
        "  -l, --list-scenes <file>   List all scene labels in a .bitscript file\n"
        "  -s, --stats <file>         Print instruction count and variable stats\n"
        "                             for a .bitscript or .bitc file\n"
        "\n"
        "EXAMPLES:\n"
        "  " << argv0 << "                              # run default (res_bitscript/main.bitscript)\n"
        "  " << argv0 << " custom.bitscript             # run a .bitscript directly\n"
        "  " << argv0 << " -r my_story.bitscript        # run with explicit flag\n"
        "  " << argv0 << " -c my_story.bitscript        # compile -> my_story.bitc\n"
        "  " << argv0 << " -c my_story.bitscript out.bitc\n"
        "  " << argv0 << " out.bitc                     # run compiled bytecode\n"
        "  " << argv0 << " -d my_story.bitscript        # validate without window\n"
        "  " << argv0 << " -l my_story.bitscript        # list all scene names\n"
        "  " << argv0 << " -s my_story.bitscript        # show stats\n"
        "\n"
        "FILE TYPES:\n"
        "  .bitscript    Human-readable BitScript source file\n"
        "  .bitc         Compiled bytecode (portable, no source needed)\n"
        "\n";
}

// ── dry-run / validate ───────────────────────────────────────────────────────
static int doDryRun(const std::string& path) {
    if (!endsWith(path, ".bitscript")) {
        std::cerr << "[ERROR] --dry-run only supports .bitscript files.\n";
        return 1;
    }
    std::cout << "[BitEngine] Parsing: " << path << "\n";
    DialogEngine engine;
    if (!engine.LoadProject(path)) {
        std::cerr << "[ERROR] Failed to parse script.\n";
        for (const auto& e : engine.GetErrors())
            std::cerr << "  " << e << "\n";
        return 1;
    }
    auto errs = engine.GetErrors();
    if (errs.empty()) {
        std::cout << "[OK] No errors found. "
                  << engine.GetProject().bytecode.size() << " instructions compiled.\n";
        return 0;
    } else {
        std::cerr << "[WARN] " << errs.size() << " issue(s) found:\n";
        for (const auto& e : errs) std::cerr << "  " << e << "\n";
        return 1;
    }
}

// ── compile ──────────────────────────────────────────────────────────────────
static int doCompile(const std::string& src, const std::string& dst) {
    if (!endsWith(src, ".bitscript")) {
        std::cerr << "[ERROR] --compile only supports .bitscript source files.\n";
        return 1;
    }
    std::cout << "[BitEngine] Compiling: " << src << "\n";
    DialogEngine engine;
    if (!engine.LoadProject(src)) {
        std::cerr << "[ERROR] Failed to parse script.\n";
        for (const auto& e : engine.GetErrors())
            std::cerr << "  " << e << "\n";
        return 1;
    }
    if (!engine.SaveBytecode(dst)) {
        std::cerr << "[ERROR] Failed to write: " << dst << "\n";
        return 1;
    }
    auto sz = std::filesystem::file_size(dst);
    std::cout << "[OK] Written: " << dst
              << "  (" << engine.GetProject().bytecode.size() << " instructions, "
              << sz << " bytes)\n";
    return 0;
}

// ── list scenes ───────────────────────────────────────────────────────────────
static int doListScenes(const std::string& path) {
    DialogEngine engine;
    if (!engine.LoadProject(path)) {
        std::cerr << "[ERROR] Failed to parse: " << path << "\n";
        return 1;
    }
    const auto& bc = engine.GetProject().bytecode;
    std::cout << "[BitEngine] Scene labels in " << path << ":\n";
    int count = 0;
    for (const auto& ins : bc) {
        if (ins.op == BitOp::LABEL) {
            std::cout << "  " << ins.args[0] << "\n";
            ++count;
        }
    }
    std::cout << "  (" << count << " total)\n";
    return 0;
}

// ── stats ────────────────────────────────────────────────────────────────────
static int doStats(const std::string& path) {
    DialogEngine engine;
    if (!engine.LoadProject(path)) {
        std::cerr << "[ERROR] Failed to parse: " << path << "\n";
        return 1;
    }
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

    std::cout << "[BitEngine] Stats for: " << path << "\n"
              << "  Instructions total : " << bc.size()           << "\n"
              << "  Scenes (labels)    : " << labels              << "\n"
              << "  Dialogue lines     : " << says                << "\n"
              << "  Choices            : " << choices             << "\n"
              << "  Jumps              : " << gotos               << "\n"
              << "  Events             : " << events              << "\n"
              << "  Variable ops       : " << sets                << "\n"
              << "  Conditionals       : " << ifs                 << "\n"
              << "  Variables defined  : " << proj.variables.size()<< "\n"
              << "  Entities defined   : " << proj.entities.size() << "\n"
              << "  Backgrounds        : " << proj.backgrounds.size()<< "\n"
              << "  Music tracks       : " << proj.music.size()   << "\n";
    return 0;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {

    // No arguments: run default project
    if (argc == 1) {
        BitApp app("res_bitscript/main.bitscript");
        app.Run();
        return 0;
    }

    const char* arg1 = argv[1];

    // ── help ─────────────────────────────────────────────────────────────────
    if (strcmp(arg1, "-h") == 0 || strcmp(arg1, "--help") == 0) {
        printHelp(argv[0]);
        return 0;
    }

    // ── version ───────────────────────────────────────────────────────────────
    if (strcmp(arg1, "-v") == 0 || strcmp(arg1, "--version") == 0) {
        std::cout << ENGINE_NAME << " v" << ENGINE_VERSION << "\n";
        return 0;
    }

    // ── compile ───────────────────────────────────────────────────────────────
    if (strcmp(arg1, "-c") == 0 || strcmp(arg1, "--compile") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --compile requires a source file.\n"; return 1; }
        std::string src = argv[2];
        std::string dst = (argc >= 4) ? argv[3] : defaultOutput(src);
        return doCompile(src, dst);
    }

    // ── dry-run / validate ────────────────────────────────────────────────────
    if (strcmp(arg1, "-d") == 0 || strcmp(arg1, "--dry-run") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --dry-run requires a file.\n"; return 1; }
        return doDryRun(argv[2]);
    }

    // ── list scenes ───────────────────────────────────────────────────────────
    if (strcmp(arg1, "-l") == 0 || strcmp(arg1, "--list-scenes") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --list-scenes requires a file.\n"; return 1; }
        return doListScenes(argv[2]);
    }

    // ── stats ─────────────────────────────────────────────────────────────────
    if (strcmp(arg1, "-s") == 0 || strcmp(arg1, "--stats") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --stats requires a file.\n"; return 1; }
        return doStats(argv[2]);
    }

    // ── explicit run ──────────────────────────────────────────────────────────
    if (strcmp(arg1, "-r") == 0 || strcmp(arg1, "--run") == 0) {
        if (argc < 3) { std::cerr << "[ERROR] --run requires a file.\n"; return 1; }
        BitApp app(argv[2]);
        app.Run();
        return 0;
    }

    // ── positional: run file directly ─────────────────────────────────────────
    if (arg1[0] != '-') {
        BitApp app(argv[1]);
        app.Run();
        return 0;
    }

    std::cerr << "[ERROR] Unknown option: " << arg1
              << "\nRun '" << argv[0] << " --help' for usage.\n";
    return 1;
}