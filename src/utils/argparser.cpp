// src/utils/argparser.cpp
#include "utils/argparser.h"
#include "utils/logger.h"
#include <string>

ArgParser::ArgParser(int argc, char** argv) {

    CLI::App app("WinZigCParser");

    bool version = false;
    std::string logLevelStr = "INFO";

    app.add_flag("--version", version, "Print version and exit");

    app.add_option("-l,--log-level", logLevelStr, "Set log level")
        ->check(CLI::IsMember({"DEBUG", "INFO", "WARN", "ERROR"}));

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    // Handle version
    if (version) {
        std::cout << "WinZigCParser version 0.1\n";
        return 0;
    }

    // Convert string → enum
    spdlog::level::level_enum level = spdlog::level::info;
    if (logLevelStr == "DEBUG") level = spdlog::level::debug;
    else if (logLevelStr == "INFO") level = spdlog::level::info;
    else if (logLevelStr == "WARN") level = spdlog::level::warn;
    else if (logLevelStr == "ERROR") level = spdlog::level::err;
    else {
        LOG_ERROR("Invalid log level: " + logLevelStr);
        return 1;
    }
    Logger::setLevel(level);
    return 0;
}

ArgParser::~ArgParser() {}