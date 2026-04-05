#include <iostream>
#include "common/result.h"
#include "common/error.h"

#include "utils/logger.h"
#include "utils/argparser.h"
#include "utils/filereader.h"

int main(int argc, char** argv) {
    Logger::init("WinZigCParser");
    auto argParserResult = ArgParser(argc, argv).parse();
    if (!argParserResult.success) {
        LOG_ERROR(argParserResult.error_message.value());
        return 1;
    }
    auto result = argParserResult.value.value();
    LOG_INFO("WinZigCParser started");
    auto fileReader = FileReader(result.inputFile);
    auto fileReaderResult = fileReader.read();
    if (!fileReaderResult.success) {
        LOG_ERROR(fileReaderResult.error_message.value());
        return 1;
    }
    LOG_INFO("File read successfully: " + fileReaderResult.value.value().content);
    return 0;
}