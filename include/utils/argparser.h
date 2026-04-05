// include/utils/argparser.h
/**
 * @file argparser.h
 * @brief Argument parser class
 * @version 0.1
 * @date 2026-04-05
 *
 * @copyright Copyright (c) 2026
 */
#ifndef ARGPARSER_H
#define ARGPARSER_H

#include <memory>
#include <string>

#include "CLI/CLI.hpp"
#include "common/result.h"
#include "utils/logger.h"

struct ArgParserResult {
    int optimizationLevel = 0;
    std::string inputFile;
    std::string outputFile = "output.txt";
    std::string logLevel = "INFO";
    std::string logFile = "logs/argparser.log";

    bool operator==(const ArgParserResult& o) const {
        return optimizationLevel == o.optimizationLevel && inputFile == o.inputFile &&
               outputFile == o.outputFile && logLevel == o.logLevel && logFile == o.logFile;
    }
};

struct ArgParserError : public Error {
    std::string msg;

    explicit ArgParserError(std::string m) : msg(std::move(m)) {}

    std::string message() const override { return "ArgParserError: " + msg; }
};

class ArgParser {
public:
    ArgParser(int argc, char** argv);
    ~ArgParser();
    Result<void> parse();

private:
    int _argc;
    char** _argv;
    std::unique_ptr<CLI::App> _app;
    bool _version = false;
    std::string _logLevelStr;
    CLI::Option* _logLevelOpt = nullptr;
};

#endif
