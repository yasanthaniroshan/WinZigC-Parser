#include <gtest/gtest.h>

#include <spdlog/spdlog.h>

#include "utils/logger.h"

class LoggerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { Logger::init("LoggerTest"); }
};

TEST_F(LoggerTest, InitProducesLoggerWithInfoLevelByDefault) {
    EXPECT_EQ(Logger::getLevel(), spdlog::level::info);
}

TEST_F(LoggerTest, SetLevelChangesLogLevel) {
    Logger::setLevel(spdlog::level::debug);
    EXPECT_EQ(Logger::getLevel(), spdlog::level::debug);

    Logger::setLevel(spdlog::level::warn);
    EXPECT_EQ(Logger::getLevel(), spdlog::level::warn);

    Logger::setLevel(spdlog::level::err);
    EXPECT_EQ(Logger::getLevel(), spdlog::level::err);

    Logger::setLevel(spdlog::level::info);
    EXPECT_EQ(Logger::getLevel(), spdlog::level::info);
}

TEST_F(LoggerTest, LogMacrosDoNotCrash) {
    Logger::setLevel(spdlog::level::debug);
    EXPECT_NO_THROW(LOG_DEBUG("debug message"));
    EXPECT_NO_THROW(LOG_INFO("info message"));
    EXPECT_NO_THROW(LOG_WARN("warn message"));
    EXPECT_NO_THROW(LOG_ERROR("error message"));
}

TEST_F(LoggerTest, AllSeverityHelpersAreCallable) {
    Logger::setLevel(spdlog::level::debug);
    EXPECT_NO_THROW(Logger::debug("d"));
    EXPECT_NO_THROW(Logger::info("i"));
    EXPECT_NO_THROW(Logger::warn("w"));
    EXPECT_NO_THROW(Logger::error("e"));
}
