#ifndef BIT_SCRIPT_ANALYZER_HPP
#define BIT_SCRIPT_ANALYZER_HPP

#include "BitEngine.hpp"
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
    static std::vector<AnalysisMessage> Analyze(const DialogProject& project);

private:
    static void CheckLabels(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckEntities(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckVariables(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckControlFlow(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckAssets(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckInstructions(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckConfiguration(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckTimelines(const DialogProject& project, std::vector<AnalysisMessage>& messages);
    static void CheckSprites(const DialogProject& project, std::vector<AnalysisMessage>& messages);
};

#endif
