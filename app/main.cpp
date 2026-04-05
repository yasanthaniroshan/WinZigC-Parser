#include <iostream>
#include "utils/logger.h"
#include "utils/argparser.h"

int main(int argc, char** argv) {
    Logger::init("WinZigCParser");
    ArgParser argParser(argc, argv);
    LOG_INFO("WinZigCParser started");
    return 0;
}