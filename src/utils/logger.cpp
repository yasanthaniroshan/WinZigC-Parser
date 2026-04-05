// src/utils/logger.cpp
#include <chrono>
#include "utils/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>


std::shared_ptr<spdlog::logger> Logger::_logger = nullptr;

void Logger::init(const std::string& name) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/WinZigCParser.log");
    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
    _logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    spdlog::register_logger(_logger);
    _logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    _logger->set_level(spdlog::level::info);
}

void Logger::debug(const std::string& msg) {
    _logger->debug(msg);
}

void Logger::info(const std::string& msg) {
    _logger->info(msg);
}

void Logger::warn(const std::string& msg) {
    _logger->warn(msg);
}

void Logger::error(const std::string& msg) {
    _logger->error(msg);
}

void Logger::setLevel(spdlog::level::level_enum level) {
    _logger->set_level(level);
}

spdlog::level::level_enum Logger::getLevel() {
    return _logger ? _logger->level() : spdlog::level::off;
}
