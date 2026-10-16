#include "ui/Console.h"

#include "core/Util.h"

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace prs {
namespace console {

namespace {
bool g_color = false;
}

void init(bool useColor) {
    g_color = useColor;
#ifdef _WIN32
    if (g_color) {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode) ||
            !SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            g_color = false;  // redirected output or legacy console
        }
    }
#endif
}

bool colorEnabled() { return g_color; }

std::string paint(const std::string& text, Tone tone) {
    if (!g_color || tone == Tone::Plain) return text;
    const char* code = "0";
    switch (tone) {
        case Tone::Title: code = "1;36"; break;
        case Tone::Accent: code = "36"; break;
        case Tone::Muted: code = "90"; break;
        case Tone::Hit: code = "32"; break;
        case Tone::Fault: code = "1;31"; break;
        case Tone::Victim: code = "33"; break;
        case Tone::Good: code = "1;32"; break;
        case Tone::Warn: code = "1;33"; break;
        case Tone::Bad: code = "1;31"; break;
        case Tone::Bold: code = "1"; break;
        case Tone::Plain: break;
    }
    return std::string("\x1b[") + code + "m" + text + "\x1b[0m";
}

std::string heading(const std::string& title) {
    std::string line = "== " + title + " ";
    if (line.size() < 78) line += std::string(78 - line.size(), '=');
    return "\n" + paint(line, Tone::Title) + "\n";
}

}  // namespace console
}  // namespace prs
