#include <iostream>
#include "common/result.h"
#include "common/error.h"

#include "utils/logger.h"
#include "utils/argparser.h"
#include "utils/filereader.h"
#include "utils/tokenizer.h"

int main(int argc, char** argv) {
    Logger::init("WinZigCParser");
    auto argParserResult = ArgParser(argc, argv).parse();
    if (!argParserResult.success) {
        LOG_ERROR(argParserResult.error_message.value());
        return 1;
    }
    auto result = argParserResult.value.value();
    if (result.showVersion) {
        return 0;   
    }
    LOG_INFO("WinZigCParser started");
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
        LOG_INFO("Token: " + token.toString());
    }

    LOG_INFO("File read successfully: " + fileReaderResult.value.value().content);
    return 0;
}