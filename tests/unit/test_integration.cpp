#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "parser/parser.h"
#include "tokenizer/tokenizer.h"
#include "utils/filereader.h"
#include "utils/logger.h"
#include "utils/tree.h"

namespace {

std::string readFile(const std::string& path) {
    auto result = FileReader(path).read();
    if (!result.success) {
        throw std::runtime_error("failed to read " + path);
    }
    return result.value->content;
}

Result<TreeNode*> parseSource(const std::string& source) {
    auto tokens = Tokenizer(source).tokenize();
    if (!tokens.success) {
        return Result<TreeNode*>::Err(
            ParserError(tokens.error_message.value_or("tokenize failed")));
    }
    return Parser(tokens.value.value()).parseTree();
}

std::string integrationPath(const std::string& filename) {
    return std::string(TEST_INTEGRATION_DIR) + "/" + filename;
}

struct IntegrationFileCase {
    std::string name;
};

std::vector<IntegrationFileCase> discoverIntegrationCases() {
    namespace fs = std::filesystem;

    std::vector<IntegrationFileCase> cases;
    const fs::path dir(TEST_INTEGRATION_DIR);

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path path = entry.path();
        if (path.extension() == ".tree") {
            continue;
        }

        const fs::path golden = fs::path(path.string() + ".tree");
        if (!fs::exists(golden)) {
            continue;
        }

        cases.push_back({path.filename().string()});
    }

    std::sort(cases.begin(), cases.end(),
              [](const IntegrationFileCase& a, const IntegrationFileCase& b) {
                  return a.name < b.name;
              });
    return cases;
}

const std::vector<IntegrationFileCase>& integrationCases() {
    static const std::vector<IntegrationFileCase> cases = discoverIntegrationCases();
    return cases;
}

}  // namespace

class ParserIntegrationFileTest
    : public ::testing::TestWithParam<IntegrationFileCase> {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("ParserIntegrationTest");
            initialized = true;
        }
    }
};

TEST_P(ParserIntegrationFileTest, MatchesGoldenTree) {
    const auto& p = GetParam();
    const std::string source = readFile(integrationPath(p.name));
    const std::string expected = readFile(integrationPath(p.name + ".tree"));

    auto result = parseSource(source);
    ASSERT_TRUE(result.success) << p.name;
    ASSERT_TRUE(result.value.has_value()) << p.name;

    TreeNode* root = result.value.value();
    EXPECT_EQ(treeToString(root), expected) << p.name;
    delete root;
}

INSTANTIATE_TEST_SUITE_P(
    Integration, ParserIntegrationFileTest,
    ::testing::ValuesIn(integrationCases()),
    [](const ::testing::TestParamInfo<IntegrationFileCase>& info) {
        return info.param.name;
    });

class ParserIntegrationTest : public ::testing::Test {};

TEST_F(ParserIntegrationTest, DiscoversFixtures) {
    ASSERT_FALSE(integrationCases().empty())
        << "no integration fixtures found in " << TEST_INTEGRATION_DIR;
}

TEST_F(ParserIntegrationTest, DumpIntegrationStemFromEnv) {
    const char* stem = std::getenv("INTEGRATION_STEM");
    if (stem == nullptr) {
        GTEST_SKIP() << "Set INTEGRATION_STEM to dump a single integration golden tree";
    }
    Logger::init("DumpIntegrationStem");
    auto result = parseSource(readFile(integrationPath(std::string(stem))));
    ASSERT_TRUE(result.success) << stem;
    std::cout << treeToString(result.value.value());
    delete result.value.value();
}
