// Correctness validation against manually calculated textbook cases
// (methodology step 7 of the synopsis).
#pragma once

#include <string>
#include <vector>

namespace prs {

struct ValidationCase {
    std::string name;
    std::string source;     // where the expected value comes from
    std::string trace;      // reference string (parsed with parseTrace)
    int frames = 3;
    std::string algorithm;
    long long expectedFaults = 0;
    long long expectedWriteBacks = -1;  // -1 = not checked
};

struct ValidationOutcome {
    ValidationCase testCase;
    long long actualFaults = 0;
    long long actualWriteBacks = 0;
    bool passed = false;
};

const std::vector<ValidationCase>& textbookCases();
std::vector<ValidationOutcome> runValidation();

}  // namespace prs
