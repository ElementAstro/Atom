#include <gtest/gtest.h>
#include <optional>
#include <string>
#include "context.h"


using modern_log::LogContext;

TEST(LogContextTest, DefaultIsEmpty) {
    LogContext ctx;
    EXPECT_TRUE(ctx.empty());
    EXPECT_EQ(ctx.user_id(), "");
    EXPECT_EQ(ctx.session_id(), "");
    EXPECT_EQ(ctx.trace_id(), "");
    EXPECT_EQ(ctx.request_id(), "");
}

TEST(LogContextTest, WithUserSetsUserId) {
    LogContext ctx;
    ctx.with_user("alice");
    EXPECT_EQ(ctx.user_id(), "alice");
    EXPECT_FALSE(ctx.empty());
}

TEST(LogContextTest, WithSessionSetsSessionId) {
    LogContext ctx;
    ctx.with_session("sess-123");
    EXPECT_EQ(ctx.session_id(), "sess-123");
    EXPECT_FALSE(ctx.empty());
}

TEST(LogContextTest, WithTraceSetsTraceId) {
    LogContext ctx;
    ctx.with_trace("trace-xyz");
    EXPECT_EQ(ctx.trace_id(), "trace-xyz");
    EXPECT_FALSE(ctx.empty());
}

TEST(LogContextTest, WithRequestSetsRequestId) {
    LogContext ctx;
    ctx.with_request("req-456");
    EXPECT_EQ(ctx.request_id(), "req-456");
    EXPECT_FALSE(ctx.empty());
}

TEST(LogContextTest, WithFieldAddsCustomField) {
    LogContext ctx;
    ctx.with_field("ip", std::string("127.0.0.1"));
    auto ip = ctx.get_field<std::string>("ip");
    ASSERT_TRUE(ip.has_value());
    EXPECT_EQ(ip.value(), "127.0.0.1");
    EXPECT_FALSE(ctx.empty());
}

TEST(LogContextTest, WithFieldSupportsMultipleTypes) {
    LogContext ctx;
    ctx.with_field("int_field", 42)
        .with_field("double_field", 3.14)
        .with_field("bool_field", true);
    EXPECT_EQ(ctx.get_field<int>("int_field"), 42);
    EXPECT_EQ(ctx.get_field<double>("double_field"), 3.14);
    EXPECT_EQ(ctx.get_field<bool>("bool_field"), true);
}

TEST(LogContextTest, GetFieldReturnsNulloptIfNotFound) {
    LogContext ctx;
    EXPECT_EQ(ctx.get_field<std::string>("missing"), std::nullopt);
}

TEST(LogContextTest, GetFieldReturnsNulloptIfTypeMismatch) {
    LogContext ctx;
    ctx.with_field("num", 123);
    EXPECT_EQ(ctx.get_field<std::string>("num"), std::nullopt);
}

TEST(LogContextTest, ToJsonIncludesAllFields) {
    LogContext ctx;
    ctx.with_user("bob")
        .with_session("sess-1")
        .with_trace("trace-2")
        .with_request("req-3")
        .with_field("custom", std::string("val"))
        .with_field("num", 7)
        .with_field("flag", true);
    std::string json = ctx.to_json();
    EXPECT_NE(json.find("\"user_id\":\"bob\""), std::string::npos);
    EXPECT_NE(json.find("\"session_id\":\"sess-1\""), std::string::npos);
    EXPECT_NE(json.find("\"trace_id\":\"trace-2\""), std::string::npos);
    EXPECT_NE(json.find("\"request_id\":\"req-3\""), std::string::npos);
    EXPECT_NE(json.find("\"custom\":\"val\""), std::string::npos);
    EXPECT_NE(json.find("\"num\":7"), std::string::npos);
    EXPECT_NE(json.find("\"flag\":true"), std::string::npos);
}

TEST(LogContextTest, MergePrefersOtherContextFields) {
    LogContext a, b;
    a.with_user("alice").with_field("x", 1);
    b.with_user("bob").with_session("sess2").with_field("x", 2).with_field("y",
                                                                           3);
    LogContext merged = a.merge(b);
    EXPECT_EQ(merged.user_id(), "bob");
    EXPECT_EQ(merged.session_id(), "sess2");
    EXPECT_EQ(merged.get_field<int>("x"), 2);
    EXPECT_EQ(merged.get_field<int>("y"), 3);
}

TEST(LogContextTest, ClearResetsAllFields) {
    LogContext ctx;
    ctx.with_user("alice").with_session("sess").with_field("foo", 1);
    ctx.clear();
    EXPECT_TRUE(ctx.empty());
    EXPECT_EQ(ctx.user_id(), "");
    EXPECT_EQ(ctx.session_id(), "");
    EXPECT_EQ(ctx.get_field<int>("foo"), std::nullopt);
}

TEST(LogContextTest, EmptyReturnsTrueOnlyIfAllFieldsAreEmpty) {
    LogContext ctx;
    EXPECT_TRUE(ctx.empty());
    ctx.with_user("alice");
    EXPECT_FALSE(ctx.empty());
    ctx.clear();
    EXPECT_TRUE(ctx.empty());
    ctx.with_field("foo", 1);
    EXPECT_FALSE(ctx.empty());
    ctx.clear();
    EXPECT_TRUE(ctx.empty());
}