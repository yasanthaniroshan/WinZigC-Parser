#include <gtest/gtest.h>
#include "common/result.h"
#include "utils/argparser.h"
#include "utils/logger.h"

class ArgParserTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { Logger::init("ArgParserTest"); }
};

TEST_F(ArgParserTest, VersionFlag) {
    const char* argv[] = {"winzigc", "--version"};
    int argc = 2;
    auto r = ArgParser(argc, (char**)argv).parse();
    ASSERT_TRUE(r.success);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_TRUE(r.value->showVersion);
    EXPECT_EQ(r.value->inputFile, "");
    EXPECT_EQ(r.value->outputFile, "");
}

TEST_F(ArgParserTest, LogLevelOption) {
    const char* argv[] = {"winzigc", "-l", "DEBUG", "input.txt"};
    int argc = 4;
    auto expected = Result<ArgParserResult>::Ok(ArgParserResult("input.txt", "output.txt"));
    EXPECT_EQ(ArgParser(argc, (char**)argv).parse(), expected);
    EXPECT_EQ(Logger::getLevel(), spdlog::level::debug);
}

TEST_F(ArgParserTest, InvalidLogLevelOption) {
    const char* argv[] = {"winzigc", "-l", "INVALID"};
    int argc = 3;
    EXPECT_EQ(ArgParser(argc, (char**)argv).parse(),
              Result<ArgParserResult>::Err(ArgParserError("Invalid log level: INVALID")));
}

TEST_F(ArgParserTest, MissingInputFileIsAnError) {
    const char* argv[] = {"winzigc"};
    int argc = 1;
    auto r = ArgParser(argc, (char**)argv).parse();
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(r.error_message.has_value());
    EXPECT_NE(r.error_message->find("Input file is required"), std::string::npos);
}

TEST_F(ArgParserTest, PositionalInputIsParsed) {
    const char* argv[] = {"winzigc", "src.winzig"};
    int argc = 2;
    auto r = ArgParser(argc, (char**)argv).parse();
    ASSERT_TRUE(r.success);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.value->inputFile, "src.winzig");
    EXPECT_EQ(r.value->outputFile, "output.txt");
    EXPECT_FALSE(r.value->showVersion);
    EXPECT_FALSE(r.value->printAbstractSyntaxTree);
}

TEST_F(ArgParserTest, InputAndOutputFlags) {
    const char* argv[] = {"winzigc", "-i", "in.txt", "-o", "out.txt"};
    int argc = 5;
    auto r = ArgParser(argc, (char**)argv).parse();
    ASSERT_TRUE(r.success);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.value->inputFile, "in.txt");
    EXPECT_EQ(r.value->outputFile, "out.txt");
}

TEST_F(ArgParserTest, AstFlagSetsPrintFlag) {
    const char* argv[] = {"winzigc", "-a", "input.txt"};
    int argc = 3;
    auto r = ArgParser(argc, (char**)argv).parse();
    ASSERT_TRUE(r.success);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_TRUE(r.value->printAbstractSyntaxTree);
}

TEST_F(ArgParserTest, UnknownOptionIsParseError) {
    const char* argv[] = {"winzigc", "--definitely-not-a-flag"};
    int argc = 2;
    auto r = ArgParser(argc, (char**)argv).parse();
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(r.error_message.has_value());
}

TEST_F(ArgParserTest, AllValidLogLevelsAreAccepted) {
    const struct {
        const char* arg;
        spdlog::level::level_enum level;
    } cases[] = {
        {"DEBUG", spdlog::level::debug},
        {"INFO", spdlog::level::info},
        {"WARN", spdlog::level::warn},
        {"ERROR", spdlog::level::err},
    };
    for (const auto& c : cases) {
        const char* argv[] = {"winzigc", "-l", c.arg, "input.txt"};
        int argc = 4;
        auto r = ArgParser(argc, (char**)argv).parse();
        ASSERT_TRUE(r.success) << c.arg;
        EXPECT_EQ(Logger::getLevel(), c.level) << c.arg;
    }
}

TEST_F(ArgParserTest, ArgParserResultEqualityComparesKeyFields) {
    ArgParserResult a("in.txt", "out.txt");
    ArgParserResult b("in.txt", "out.txt");
    ArgParserResult c("other.txt", "out.txt");
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

TEST_F(ArgParserTest, ArgParserErrorMessageIsPrefixed) {
    ArgParserError e("boom");
    EXPECT_EQ(e.message(), "ArgParserError: boom");
}