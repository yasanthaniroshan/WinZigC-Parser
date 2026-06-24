// include/common/error.h
#pragma once
#include <string>

struct Error {
    virtual ~Error() = default;
    virtual std::string message() const = 0;
};

struct BaseError : public Error {
    std::string msg;
    BaseError(std::string m) : msg(std::move(m)) {}
    std::string message() const override {
        return "Error: " + msg;
    }
};