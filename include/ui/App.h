// Application layer: interactive menu-driven lab and a scriptable CLI.
#pragma once

#include <string>
#include <vector>

#include "core/Trace.h"
#include "core/Types.h"
#include "io/TraceIO.h"

namespace prs {

class App {
public:
    App();
    int run(int argc, char** argv);

private:
    // CLI
    int runCli(const std::vector<std::string>& args);
    static void printUsage();

    // Interactive menu
    void interactive();
    void showMenu();
    void actionEnterTrace();
    void actionLoadFile();
    void actionGenerate();
    void actionScenarios();
    void actionConfigure();
    void actionSelectAlgorithms();
    void actionCompare();
    void actionWalkthrough();
    void actionSweep();
    void actionBelady();
    void actionWorkingSet();
    void actionProfile();
    void actionInclusion();
    void actionExport();
    void actionValidation();
    void actionEncyclopedia();
    void actionSaveTrace();

    // Input helpers (all tolerate EOF)
    bool readLine(const std::string& label, std::string& out);
    std::string ask(const std::string& label, const std::string& def = "");
    int askInt(const std::string& label, int def, int lo, int hi);
    double askDouble(const std::string& label, double def, double lo, double hi);
    void pause();

    int workingSetWindow() const;
    std::string exportAll(const std::string& dir, int sweepMax);

    Trace trace_;
    SimulationConfig cfg_;
    std::vector<std::string> keys_;
    ParseOptions parse_;
    int wsWindow_ = 0;  // 0 = automatic
    bool eof_ = false;
};

}  // namespace prs
