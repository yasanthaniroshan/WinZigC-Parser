#include "utils/filereader.h"

FileReader::FileReader(const std::string& filename) : _filename(filename) {}

Result<FileReaderResult> FileReader::read() {
    try {
        std::ifstream file(_filename);
        if (!file.is_open()) {
            LOG_ERROR("File not found: " + _filename);
            return Result<FileReaderResult>::Err(FileNotFoundError(_filename));
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        LOG_DEBUG("File read successfully: " + _filename);
        return Result<FileReaderResult>::Ok(FileReaderResult(content));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read file: " + _filename + " " + e.what());
        return Result<FileReaderResult>::Err(FileReadError(_filename));
    }
}

FileReader::~FileReader() = default;