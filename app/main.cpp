#include <iostream>
#include "common/result.h"
#include "utils/logger.h"
#include "utils/argparser.h"

int main(int argc, char** argv) {
    Logger::init("WinZigCParser");
    auto result = ArgParser(argc, argv).parse();
    if (!result.success) {
        LOG_ERROR(result.error_message.value());
        return 1;
    }
    LOG_INFO("WinZigCParser started");
    return 0;
}