#include "utils/argparser.h"

ArgParser::ArgParser(int argc, char** argv)
    : _argc(argc), _argv(argv), _app(std::make_unique<CLI::App>()) {
    _app->add_flag("-v,--version", _version, "Print version and exit");
    _logLevelOpt = _app->add_option("-l,--log-level", _logLevelStr, "Set log level");
    _app->add_option("-i,--input-file", _inputFileStr, "Set input file");
    _app->add_option("-o,--output-file", _outputFileStr, "Set output file");
    // Long names must use "--" in CLI11; course scripts use "-ast" — normalize in main() before parse
    _app->add_flag("-a,--ast", _abstractSyntaxTree, "Print Abstract Syntax Tree");
    auto* inputOpt = _app->add_option("input", _inputFileStr, "Input file");
    inputOpt->required(false);
}

Result<ArgParserResult> ArgParser::parse() {
    try {
        _app->parse(_argc, _argv);
    } catch (const CLI::ParseError& e) {
        return Result<ArgParserResult>::Err(ArgParserError(e.what()));
    }

    bool printAbstractSyntaxTree = false;

    if (_version) {
        std::cout << "WinZigC version " << WINZIG_VERSION << std::endl;
        std::cout << "Compiler: " << WINZIG_COMPILER << std::endl;
        std::cout << "Git hash: " << WINZIG_GIT_HASH << std::endl;
        std::cout << "Platform: " << WINZIG_PLATFORM << std::endl;
        std::cout << "Build on: " << WINZIG_BUILD_TIME << std::endl;
        ArgParserResult r("", "");
        r.showVersion = true;
        return Result<ArgParserResult>::Ok(std::move(r));
    }

    if (_abstractSyntaxTree) {
        LOG_DEBUG("Abstract Syntax Tree will be printed");
        printAbstractSyntaxTree = true;
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
    std::string output_file = _inputFileStr.substr(0, _inputFileStr.find_last_of('.')) + ".asm";
    std::string output = _outputFileStr.empty() ? output_file : _outputFileStr;
    return Result<ArgParserResult>::Ok(ArgParserResult(_inputFileStr, output, printAbstractSyntaxTree));
}

ArgParser::~ArgParser() = default;
