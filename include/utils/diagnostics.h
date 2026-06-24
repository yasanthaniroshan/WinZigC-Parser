// include/utils/diagnostics.h
/**
 * @file diagnostics.h
 * @brief Compiler-style, colorized diagnostic output for the terminal.
 *
 * Header-only helpers for printing pretty diagnostics to stderr. Any compiler
 * phase (tokenizer, parser, semantic analyzer, code generator, ...) can use
 * these to report errors, warnings, and notes in a consistent format:
 *
 *   error: <message>
 *     --> line <line>, column <column>
 *
 * ANSI styling is emitted only when stderr is an interactive terminal, so
 * piped or redirected output (and log files) stay free of escape codes.
 */
#ifndef UTILS_DIAGNOSTICS_H
#define UTILS_DIAGNOSTICS_H

#include <cstdio>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace diagnostics {

inline bool stderrIsTty()
{
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

// Returns the given ANSI escape code when stderr is a terminal, otherwise "".
inline const char *style(const char *code)
{
    static const bool tty = stderrIsTty();
    return tty ? code : "";
}

// Common ANSI escape codes, gated through style().
namespace color {
inline const char *red() { return style("\033[1;31m"); }
inline const char *green() { return style("\033[1;32m"); }
inline const char *yellow() { return style("\033[1;33m"); }
inline const char *blue() { return style("\033[1;34m"); }
inline const char *cyan() { return style("\033[1;36m"); }
inline const char *bold() { return style("\033[1m"); }
inline const char *dim() { return style("\033[2m"); }
inline const char *reset() { return style("\033[0m"); }
}  // namespace color

// Severity of a diagnostic. Each level has a label and a color.
enum class Severity { Error, Warning, Note };

inline const char *severityLabel(Severity severity)
{
    switch (severity)
    {
        case Severity::Error:   return "error";
        case Severity::Warning: return "warning";
        case Severity::Note:    return "note";
    }
    return "error";
}

inline const char *severityColor(Severity severity)
{
    switch (severity)
    {
        case Severity::Error:   return color::red();
        case Severity::Warning: return color::yellow();
        case Severity::Note:    return color::cyan();
    }
    return color::red();
}

// Source position. Negative line/column means "no location available".
struct Location {
    int line = -1;
    int column = -1;
    bool valid() const { return line >= 0 && column >= 0; }
};

// Core renderer: "<severity>: <message>" followed by an optional location line.
inline void print(Severity severity, const std::string &message, Location loc = {})
{
    std::cerr << severityColor(severity) << severityLabel(severity) << color::reset()
              << color::bold() << ": " << message << color::reset() << "\n";
    if (loc.valid())
    {
        std::cerr << "  " << color::blue() << "-->" << color::reset()
                  << " line " << loc.line << ", column " << loc.column << "\n";
    }
    std::cerr << "\n";
}

// Convenience wrappers for the common severities.
inline void error(const std::string &message, int line = -1, int column = -1)
{
    print(Severity::Error, message, {line, column});
}

inline void warning(const std::string &message, int line = -1, int column = -1)
{
    print(Severity::Warning, message, {line, column});
}

inline void note(const std::string &message, int line = -1, int column = -1)
{
    print(Severity::Note, message, {line, column});
}

// Prints a colored one-line summary, e.g. "Semantic analysis failed with 2 error(s)."
inline void summary(const std::string &message, Severity severity = Severity::Error)
{
    std::cerr << severityColor(severity) << message << color::reset() << "\n";
}

}  // namespace diagnostics

#endif  // UTILS_DIAGNOSTICS_H
