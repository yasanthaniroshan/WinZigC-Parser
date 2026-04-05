#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "common/error.h"

template <typename T>
struct Result {
    bool success{false};
    std::optional<T> value;
    std::optional<std::string> error_message;

    static Result Ok(T val) {
        Result r;
        r.success = true;
        r.value = std::move(val);
        return r;
    }

    template <typename E, std::enable_if_t<std::is_base_of_v<Error, std::decay_t<E>>, int> = 0>
    static Result Err(E&& e) {
        Result r;
        r.success = false;
        r.error_message = std::forward<E>(e).message();
        return r;
    }

    friend bool operator==(const Result& a, const Result& b) {
        return a.success == b.success && a.value == b.value && a.error_message == b.error_message;
    }
};

template <>
struct Result<void> {
    bool success{false};
    std::optional<std::string> error_message;

    static Result Ok() {
        Result r;
        r.success = true;
        return r;
    }

    template <typename E, std::enable_if_t<std::is_base_of_v<Error, std::decay_t<E>>, int> = 0>
    static Result Err(E&& e) {
        Result r;
        r.success = false;
        r.error_message = std::forward<E>(e).message();
        return r;
    }

    friend bool operator==(const Result& a, const Result& b) {
        return a.success == b.success && a.error_message == b.error_message;
    }
};
