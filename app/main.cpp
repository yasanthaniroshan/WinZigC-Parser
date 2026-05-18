#include <iostream>
#include <string>
#include <vector>

#include "common/result.h"
#include "common/error.h"

#include "utils/logger.h"
#include "utils/argparser.h"
#include "utils/filereader.h"
#include "tokenizer/tokenizer.h"
#include "parser/parser.h"

namespace {

// CLI11 only allows "--name" for long flags; course hand-in uses "-ast".
std::vector<char*> normalizeArgv(int argc, char** argv) {
    std::vector<char*> out;
    out.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-ast") {
            static char astFlag[] = "--ast";
            out.push_back(astFlag);
        } else {
            out.push_back(argv[i]);
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    Logger::init("WinZigCParser");
    std::vector<char*> normalizedArgv = normalizeArgv(argc, argv);
    int normalizedArgc = static_cast<int>(normalizedArgv.size());
    auto argParserResult =
        ArgParser(normalizedArgc, normalizedArgv.data()).parse();
    if (!argParserResult.success) {
        LOG_ERROR(argParserResult.error_message.value());
        return 1;
    }
    auto result = argParserResult.value.value();
    if (result.showVersion) {
        return 0;   
    }
    LOG_DEBUG("WinZigCParser started");
    auto fileReader = FileReader(result.inputFile);
    auto fileReaderResult = fileReader.read();
    if (!fileReaderResult.success) {
        LOG_ERROR(fileReaderResult.error_message.value());
        return 1;
    }

    auto tokenizer = Tokenizer(std::string(fileReaderResult.value.value().content));
    auto toks =  tokenizer.tokenize();

    // iterate through `toks` and printing the tokens
    // DEBUGGING
    if (!toks.success) {
        LOG_ERROR(toks.error_message.value());
        return 1;
    }
    for (const auto& token : toks.value.value()) {
        LOG_DEBUG("Token: " + token.toString());
    }

    LOG_DEBUG("Tokenizing successful");
    auto parser = Parser(toks.value.value());
    auto parserResult = parser.parse(result.printAbstractSyntaxTree);
    if (!parserResult.success) {
        LOG_ERROR(parserResult.error_message.value());
        return 1;
    }
    LOG_DEBUG("Parser parsed successfully");
    return 0;
}