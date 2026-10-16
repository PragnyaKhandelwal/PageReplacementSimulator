// Page Replacement Algorithm Simulator
// Operating Systems PBL - Jaypee Institute of Information Technology
//
// Entry point: with no arguments an interactive lab starts; otherwise the
// command line options are executed (see --help).
#include "ui/App.h"

int main(int argc, char** argv) {
    prs::App app;
    return app.run(argc, argv);
}
