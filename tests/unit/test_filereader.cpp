#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "common/result.h"
#include "utils/filereader.h"
#include "utils/logger.h"

namespace {

class TempFile {
public:
    explicit TempFile(const std::string& contents) {
        path_ = std::filesystem::temp_directory_path() /
                ("winzigc_filereader_" + std::to_string(::getpid()) + "_" +
                 std::to_string(counter()) + ".txt");
        std::ofstream out(path_);
        out << contents;
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    static unsigned& counter() {
        static unsigned c = 0;
        return ++c;
    }
    std::filesystem::path path_;
};

}  // namespace

class FileReaderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        static bool initialized = false;
        if (!initialized) {
            Logger::init("FileReaderTest");
            initialized = true;
        }
    }
};

TEST_F(FileReaderTest, ReadsExistingFileContent) {
    const std::string body = "hello\nworld\n";
    TempFile tmp(body);

    auto result = FileReader(tmp.path().string()).read();
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value->content, body);
}

TEST_F(FileReaderTest, ReadsEmptyFile) {
    TempFile tmp("");

    auto result = FileReader(tmp.path().string()).read();
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value->content, "");
}

TEST_F(FileReaderTest, MissingFileReturnsFileNotFoundError) {
    const std::string path = "/definitely/does/not/exist/winzigc_no_such_file_42";

    auto result = FileReader(path).read();
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error_message.has_value());
    EXPECT_NE(result.error_message->find("File not found"), std::string::npos);
    EXPECT_NE(result.error_message->find(path), std::string::npos);
}

TEST_F(FileReaderTest, FileNotFoundErrorMessageMatchesType) {
    FileNotFoundError e("missing.txt");
    EXPECT_EQ(e.message(), "File not found: missing.txt");
}

TEST_F(FileReaderTest, FileReadErrorMessageMatchesType) {
    FileReadError e("bad.txt");
    EXPECT_EQ(e.message(), "File read error: bad.txt");
}

TEST_F(FileReaderTest, FileReaderResultCarriesContent) {
    FileReaderResult r("payload");
    EXPECT_EQ(r.content, "payload");
}
