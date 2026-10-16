#include "algorithms/AlgorithmFactory.h"

#include <map>
#include <stdexcept>

#include "algorithms/Algorithms.h"
#include "core/Util.h"

namespace prs {

const std::vector<std::string>& allAlgorithmKeys() {
    static const std::vector<std::string> keys = {
        "FIFO", "LRU", "OPT", "CLOCK", "ESC", "NRU", "AGING", "LFU", "MFU", "RANDOM", "ARC"};
    return keys;
}

const std::vector<std::string>& classicAlgorithmKeys() {
    static const std::vector<std::string> keys = {"FIFO", "LRU", "OPT"};
    return keys;
}

std::string canonicalKey(const std::string& name) {
    static const std::map<std::string, std::string> aliases = {
        {"FIFO", "FIFO"},        {"FCFS", "FIFO"},
        {"LRU", "LRU"},
        {"OPT", "OPT"},          {"OPTIMAL", "OPT"},   {"MIN", "OPT"}, {"BELADY", "OPT"},
        {"CLOCK", "CLOCK"},      {"SC", "CLOCK"},      {"SECOND", "CLOCK"},
        {"SECONDCHANCE", "CLOCK"},
        {"ESC", "ESC"},          {"ENHANCED", "ESC"},  {"ECLOCK", "ESC"},
        {"NRU", "NRU"},
        {"AGING", "AGING"},      {"AGE", "AGING"},
        {"LFU", "LFU"},
        {"MFU", "MFU"},
        {"RANDOM", "RANDOM"},    {"RAND", "RANDOM"},   {"RR", "RANDOM"},
        {"ARC", "ARC"},
    };
    std::string k = toUpper(trim(name));
    k.erase(std::remove(k.begin(), k.end(), '-'), k.end());
    k.erase(std::remove(k.begin(), k.end(), '_'), k.end());
    auto it = aliases.find(k);
    return it == aliases.end() ? std::string() : it->second;
}

std::unique_ptr<ReplacementAlgorithm> createAlgorithm(const std::string& name) {
    const std::string key = canonicalKey(name);
    if (key == "FIFO") return std::unique_ptr<ReplacementAlgorithm>(new FIFO());
    if (key == "LRU") return std::unique_ptr<ReplacementAlgorithm>(new LRU());
    if (key == "OPT") return std::unique_ptr<ReplacementAlgorithm>(new Optimal());
    if (key == "CLOCK") return std::unique_ptr<ReplacementAlgorithm>(new Clock());
    if (key == "ESC") return std::unique_ptr<ReplacementAlgorithm>(new EnhancedClock());
    if (key == "NRU") return std::unique_ptr<ReplacementAlgorithm>(new NRU());
    if (key == "AGING") return std::unique_ptr<ReplacementAlgorithm>(new Aging());
    if (key == "LFU") return std::unique_ptr<ReplacementAlgorithm>(new LFU());
    if (key == "MFU") return std::unique_ptr<ReplacementAlgorithm>(new MFU());
    if (key == "RANDOM") return std::unique_ptr<ReplacementAlgorithm>(new RandomReplacement());
    if (key == "ARC") return std::unique_ptr<ReplacementAlgorithm>(new ARC());
    return nullptr;
}

std::vector<std::string> parseAlgorithmList(const std::string& spec) {
    const std::string s = toLower(trim(spec));
    if (s.empty() || s == "classic") return classicAlgorithmKeys();
    if (s == "all") return allAlgorithmKeys();

    std::vector<std::string> keys;
    for (const auto& token : split(s, ", ;")) {
        std::string key = canonicalKey(token);
        if (key.empty()) throw std::invalid_argument("unknown algorithm '" + token + "'");
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
    }
    if (keys.empty()) throw std::invalid_argument("no algorithm selected");
    return keys;
}

}  // namespace prs
