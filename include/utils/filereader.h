#ifndef FILEREADER_H
#define FILEREADER_H

#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include "common/result.h"
#include "common/error.h"
#include "utils/logger.h"


struct FileNotFoundError : public Error {
    std::string filename;
    FileNotFoundError(const std::string& filename) : filename(filename) {}
    std::string message() const override { return "File not found: " + filename; }
};

struct FileReadError : public Error {
    std::string filename;
    FileReadError(const std::string& filename) : filename(filename) {}
    std::string message() const override { return "File read error: " + filename; }
};


struct FileReaderResult {
    std::string content;
    FileReaderResult(const std::string& content) : content(content) {}
};

class FileReader {
public:
    FileReader(const std::string& filename);
    Result<FileReaderResult> read();
    ~FileReader();
private:
    std::string _filename;
};

#endif