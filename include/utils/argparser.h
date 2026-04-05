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

#include "CLI/CLI.hpp"

class ArgParser {
    public:
        ArgParser(int argc, char** argv);
        ~ArgParser();
};

#endif