#include "utils/argparser.h"

ArgParser::ArgParser(int argc, char** argv)
    : _argc(argc), _argv(argv), _app(std::make_unique<CLI::App>()) {
    _app->add_flag("--version", _version, "Print version and exit");
    _logLevelOpt = _app->add_option("-l,--log-level", _logLevelStr, "Set log level");
    _app->add_option("-i,--input-file", _inputFileStr, "Set input file");
    _app->add_option("-o,--output-file", _outputFileStr, "Set output file");
    auto* inputOpt = _app->add_option("input", _inputFileStr, "Input file");
    inputOpt->required(false);
}

Result<ArgParserResult> ArgParser::parse() {
    try {
        _app->parse(_argc, _argv);
    } catch (const CLI::ParseError& e) {
        return Result<ArgParserResult>::Err(ArgParserError(e.what()));
    }

    if (_version) {
        LOG_INFO("WinZigCParser version 0.1");
        ArgParserResult r("", "");
        r.showVersion = true;
        return Result<ArgParserResult>::Ok(std::move(r));
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
            return Result<ArgParserResult>::Err(ArgParserError("Invalid log level: " + s));
        }
        Logger::setLevel(level);
    }

    if (_inputFileStr.empty()) {
        LOG_ERROR("Input file is required");
        return Result<ArgParserResult>::Err(ArgParserError("Input file is required"));
    }

    std::string output = _outputFileStr.empty() ? "output.txt" : _outputFileStr;
    return Result<ArgParserResult>::Ok(ArgParserResult(_inputFileStr, output));
}

ArgParser::~ArgParser() = default;
