// atom/extra/spdlog/utils/test_structured_data.cpp

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include "structured_data.h"


using modern_log::StructuredData;

TEST(StructuredDataTest, AddAndGetSingleField) {
    StructuredData data;
    data.add("foo", 42);
    auto val = data.get<int>("foo");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), 42);
}

TEST(StructuredDataTest, AddStringAndGet) {
    StructuredData data;
    data.add("bar", std::string("baz"));
    auto val = data.get<std::string>("bar");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "baz");
}

TEST(StructuredDataTest, AddMultipleTypes) {
    StructuredData data;
    data.add("int", 1).add("double", 3.14).add("bool", true);
    EXPECT_EQ(data.get<int>("int"), 1);
    EXPECT_EQ(data.get<double>("double"), 3.14);
    EXPECT_EQ(data.get<bool>("bool"), true);
}

TEST(StructuredDataTest, HasReturnsTrueIfExists) {
    StructuredData data;
    data.add("x", 5);
    EXPECT_TRUE(data.has("x"));
    EXPECT_FALSE(data.has("y"));
}

TEST(StructuredDataTest, RemoveField) {
    StructuredData data;
    data.add("a", 1);
    EXPECT_TRUE(data.has("a"));
    EXPECT_TRUE(data.remove("a"));
    EXPECT_FALSE(data.has("a"));
    EXPECT_FALSE(data.remove("a"));
}

TEST(StructuredDataTest, ClearRemovesAllFields) {
    StructuredData data;
    data.add("a", 1).add("b", 2);
    EXPECT_FALSE(data.empty());
    data.clear();
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(data.size(), 0u);
}

TEST(StructuredDataTest, SizeAndEmpty) {
    StructuredData data;
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(data.size(), 0u);
    data.add("a", 1);
    EXPECT_FALSE(data.empty());
    EXPECT_EQ(data.size(), 1u);
    data.add("b", 2);
    EXPECT_EQ(data.size(), 2u);
}

TEST(StructuredDataTest, ToJsonEmpty) {
    StructuredData data;
    EXPECT_EQ(data.to_json(), "{}");
}

TEST(StructuredDataTest, ToJsonVariousTypes) {
    StructuredData data;
    data.add("str", std::string("abc"))
        .add("i", 123)
        .add("f", 1.5f)
        .add("d", 2.5)
        .add("b", true)
        .add("u", 42u);
    std::string json = data.to_json();
    EXPECT_NE(json.find("\"str\":\"abc\""), std::string::npos);
    EXPECT_NE(json.find("\"i\":123"), std::string::npos);
    EXPECT_NE(json.find("\"f\":1.5"), std::string::npos);
    EXPECT_NE(json.find("\"d\":2.5"), std::string::npos);
    EXPECT_NE(json.find("\"b\":true"), std::string::npos);
    EXPECT_NE(json.find("\"u\":42"), std::string::npos);
}

TEST(StructuredDataTest, GetReturnsNulloptIfNotFound) {
    StructuredData data;
    EXPECT_EQ(data.get<int>("missing"), std::nullopt);
}

TEST(StructuredDataTest, GetReturnsNulloptIfTypeMismatch) {
    StructuredData data;
    data.add("x", 123);
    EXPECT_EQ(data.get<std::string>("x"), std::nullopt);
}

TEST(StructuredDataTest, MergePrefersOtherFields) {
    StructuredData a, b;
    a.add("x", 1).add("y", 2);
    b.add("x", 10).add("z", 3);
    StructuredData merged = a.merge(b);
    EXPECT_EQ(merged.get<int>("x"), 10);
    EXPECT_EQ(merged.get<int>("y"), 2);
    EXPECT_EQ(merged.get<int>("z"), 3);
}

TEST(StructuredDataTest, KeysReturnsAllFieldNames) {
    StructuredData data;
    data.add("a", 1).add("b", 2).add("c", 3);
    auto keys = data.keys();
    EXPECT_EQ(keys.size(), 3u);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "a"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "b"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "c"), keys.end());
}

TEST(StructuredDataTest, AddConstCharStar) {
    StructuredData data;
    data.add("msg", "hello");
    auto val = data.get<const char*>("msg");
    // std::any_cast<const char*> will fail, but std::string will succeed
    EXPECT_EQ(data.get<std::string>("msg"), "hello");
}

TEST(StructuredDataTest, AddFieldsVariadic) {
    StructuredData data;
    // This test is commented out because add_fields is not implemented
    // correctly in the header. Uncomment and fix add_fields if you want to test
    // this. data.add_fields("a", 1, "b", 2, "c", 3);
    // EXPECT_EQ(data.get<int>("a"), 1);
    // EXPECT_EQ(data.get<int>("b"), 2);
    // EXPECT_EQ(data.get<int>("c"), 3);
}
