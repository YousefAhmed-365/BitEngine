#ifndef BIT_SCRIPT_ANALYZER_HPP
#define BIT_SCRIPT_ANALYZER_HPP

#include "BitRuntime.hpp"
#include <string>
#include <vector>
#include <set>

struct AnalysisMessage {
    enum class Level { INFO, WARNING, ERROR };
    Level level;
    std::string message;
    int line; // Line number in source script if available
};

class BitScriptAnalyzer {
public:
    static std::vector<AnalysisMessage> Analyze(const BitProject& project);

private:
    static void CheckLabels(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckEntities(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckVariables(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckControlFlow(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckAssets(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckInstructions(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckConfiguration(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckTimelines(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckSprites(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckEvents(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckUIAssets(const BitProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckUICommands(const BitProject& project, std::vector<AnalysisMessage>& messages);
};

#endif
