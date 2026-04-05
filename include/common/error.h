// include/common/error.h
#pragma once
#include <string>

struct Error {
    virtual ~Error() = default;
    virtual std::string message() const = 0;
};