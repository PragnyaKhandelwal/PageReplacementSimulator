// ANSI colour support for the terminal (enables VT processing on Windows 10+).
#pragma once

#include <string>

namespace prs {
namespace console {

enum class Tone { Plain, Title, Accent, Muted, Hit, Fault, Victim, Good, Warn, Bad, Bold };

void init(bool useColor);
bool colorEnabled();

std::string paint(const std::string& text, Tone tone);

// Section heading:  "== Title ==========="
std::string heading(const std::string& title);

}  // namespace console
}  // namespace prs
