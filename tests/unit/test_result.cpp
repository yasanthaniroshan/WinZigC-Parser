#include <gtest/gtest.h>

#include <string>

#include "common/error.h"
#include "common/result.h"

namespace {

struct DummyError : public Error {
    std::string text;
    explicit DummyError(std::string t) : text(std::move(t)) {}
    std::string message() const override { return "DummyError: " + text; }
};

}  // namespace

TEST(ResultTest, OkResultCarriesValueAndSuccess) {
    auto r = Result<int>::Ok(42);
    EXPECT_TRUE(r.success);
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(*r.value, 42);
    EXPECT_FALSE(r.error_message.has_value());
}

TEST(ResultTest, ErrResultCarriesErrorMessage) {
    auto r = Result<int>::Err(DummyError("nope"));
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(r.value.has_value());
    ASSERT_TRUE(r.error_message.has_value());
    EXPECT_EQ(*r.error_message, "DummyError: nope");
}

TEST(ResultTest, EqualityForOkValues) {
    auto a = Result<int>::Ok(7);
    auto b = Result<int>::Ok(7);
    auto c = Result<int>::Ok(8);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

TEST(ResultTest, EqualityForErrValues) {
    auto a = Result<int>::Err(DummyError("x"));
    auto b = Result<int>::Err(DummyError("x"));
    auto c = Result<int>::Err(DummyError("y"));
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

TEST(ResultTest, OkAndErrNotEqual) {
    auto ok = Result<int>::Ok(1);
    auto err = Result<int>::Err(DummyError("e"));
    EXPECT_FALSE(ok == err);
}

TEST(ResultTest, VoidResultOkAndErr) {
    auto ok = Result<void>::Ok();
    auto err = Result<void>::Err(DummyError("oops"));

    EXPECT_TRUE(ok.success);
    EXPECT_FALSE(ok.error_message.has_value());

    EXPECT_FALSE(err.success);
    ASSERT_TRUE(err.error_message.has_value());
    EXPECT_EQ(*err.error_message, "DummyError: oops");
}

TEST(ResultTest, VoidResultEquality) {
    auto a = Result<void>::Ok();
    auto b = Result<void>::Ok();
    auto c = Result<void>::Err(DummyError("c"));
    auto d = Result<void>::Err(DummyError("c"));
    auto e = Result<void>::Err(DummyError("e"));

    EXPECT_EQ(a, b);
    EXPECT_EQ(c, d);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(c == e);
}

TEST(ResultTest, StringValueIsMoved) {
    std::string payload(64, 'x');
    auto r = Result<std::string>::Ok(std::move(payload));
    ASSERT_TRUE(r.value.has_value());
    EXPECT_EQ(r.value->size(), 64u);
}
