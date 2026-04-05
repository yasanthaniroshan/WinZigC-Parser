// include/utils/logger.h

/**
 * @file logger.h
 * @brief Logger class and macros for logging messages
 * @version 0.1
 * @date 2026-04-05
 * 
 * @copyright Copyright (c) 2026
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

class Logger {
private:
    static std::shared_ptr<spdlog::logger> _logger;

public:
    static void init(const std::string& name);

    static void setLevel(spdlog::level::level_enum level);

    static spdlog::level::level_enum getLevel();

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
};

#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARN(msg) Logger::warn(msg)
#define LOG_ERROR(msg) Logger::error(msg)
#define LOG_DEBUG(msg) Logger::debug(msg)

#endif