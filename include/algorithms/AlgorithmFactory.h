// Creates policies by name and parses algorithm lists such as "fifo,lru,opt".
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "algorithms/ReplacementAlgorithm.h"

namespace prs {

// Every registered key, in presentation order.
const std::vector<std::string>& allAlgorithmKeys();

// The three algorithms required by the synopsis.
const std::vector<std::string>& classicAlgorithmKeys();

// Case-insensitive, accepts aliases (OPTIMAL, MIN, SC, SECOND, ...).
// Returns nullptr for an unknown name.
std::unique_ptr<ReplacementAlgorithm> createAlgorithm(const std::string& name);

// Canonical key for a name/alias, or "" when unknown.
std::string canonicalKey(const std::string& name);

// "all", "classic" or a comma separated list. Throws std::invalid_argument.
std::vector<std::string> parseAlgorithmList(const std::string& spec);

}  // namespace prs
