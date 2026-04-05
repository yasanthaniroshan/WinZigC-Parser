#include "utils/argparser.h"

ArgParser::ArgParser(int argc, char** argv)
    : _argc(argc), _argv(argv), _app(std::make_unique<CLI::App>()) {
    _app->add_flag("--version", _version, "Print version and exit");
    _logLevelOpt = _app->add_option("-l,--log-level", _logLevelStr, "Set log level");
}

Result<void> ArgParser::parse() {
    try {
        _app->parse(_argc, _argv);
    } catch (const CLI::ParseError& e) {
        return Result<void>::Err(ArgParserError(e.what()));
    }

    if (_version) {
        LOG_INFO("WinZigCParser version 0.1");
        return Result<void>::Ok();
    }

    if (_logLevelOpt && _logLevelOpt->count() > 0) {
        const std::string& s = _logLevelStr;
        spdlog::level::level_enum level = spdlog::level::info;
        if (s == "DEBUG")
            level = spdlog::level::debug;
        else if (s == "INFO")
            level = spdlog::level::info;
        else if (s == "WARN")
            level = spdlog::level::warn;
        else if (s == "ERROR")
            level = spdlog::level::err;
        else {
            LOG_ERROR("Invalid log level: " + s);
            return Result<void>::Err(ArgParserError("Invalid log level: " + s));
        }
        Logger::setLevel(level);
    }

    return Result<void>::Ok();
}

ArgParser::~ArgParser() = default;
