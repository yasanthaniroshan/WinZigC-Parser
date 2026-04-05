#include <gtest/gtest.h>
#include "common/result.h"
#include "utils/argparser.h"
#include "utils/logger.h"

class ArgParserTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { Logger::init("ArgParserTest"); }
};

TEST_F(ArgParserTest, VersionFlag) {
    const char* argv[] = {"parser", "--version"};
    int argc = 2;
    EXPECT_EQ(ArgParser(argc, (char**)argv).parse(), Result<void>::Ok());
}

TEST_F(ArgParserTest, LogLevelOption) {
    const char* argv[] = {"parser", "-l", "DEBUG"};
    int argc = 3;
    EXPECT_EQ(ArgParser(argc, (char**)argv).parse(), Result<void>::Ok());
    EXPECT_EQ(Logger::getLevel(), spdlog::level::debug);
}

TEST_F(ArgParserTest, InvalidLogLevelOption) {
    const char* argv[] = {"parser", "-l", "INVALID"};
    int argc = 3;
    EXPECT_EQ(ArgParser(argc, (char**)argv).parse(),
              Result<void>::Err(ArgParserError("Invalid log level: INVALID")));
}
