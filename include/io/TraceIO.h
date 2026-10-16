// Parsing, loading and saving of reference traces.
//
// Accepted token forms (separated by spaces, commas, semicolons or newlines;
// '#' starts a comment):
//   7        read of page 7
//   7W, W7   write to page 7 (also 7w, w:7, 7:w, R7, 7R for explicit reads)
//   0x1A3F   virtual address -> page = address / pageSize
// With addressMode = true, plain decimal numbers are treated as addresses too.
#pragma once

#include <string>

#include "core/Trace.h"

namespace prs {

struct ParseOptions {
    long long pageSize = 4096;  // bytes per page for address translation
    bool addressMode = false;   // treat decimal numbers as byte addresses
};

// Throws std::invalid_argument with the offending token on malformed input.
Trace parseTrace(const std::string& text, const std::string& name = "custom",
                 const ParseOptions& options = ParseOptions());

// Throws std::runtime_error if the file cannot be read.
Trace loadTraceFile(const std::string& path, const ParseOptions& options = ParseOptions());

void saveTraceFile(const Trace& trace, const std::string& path);

}  // namespace prs
