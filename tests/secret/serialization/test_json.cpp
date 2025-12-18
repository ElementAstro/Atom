/*
 * test_json.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/password/entry.hpp"
#include "atom/secret/serialization/json.hpp"

namespace atom::secret::test {

class JsonSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    PasswordEntry createTestEntry() {
        PasswordEntry entry;
        entry.id = "test-id-123";
        entry.title = "Test Entry";
        entry.username = "testuser";
        entry.password = "testpassword";
        entry.url = "https://example.com";
        entry.notes = "Test notes";
        entry.email = "test@example.com";
        entry.category = PasswordCategory::Work;
        entry.isFavorite = true;
        entry.created = std::chrono::system_clock::now();
        entry.modified = std::chrono::system_clock::now();
        entry.addTag("work");
        entry.addTag("important");
        return entry;
    }
};

TEST_F(JsonSerializerTest, SerializeEntry) {
    PasswordEntry entry = createTestEntry();

    auto result = JsonSerializer::serializeEntry(entry);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_FALSE(result.value().empty());
    EXPECT_NE(result.value().find("test-id-123"), std::string::npos);
    EXPECT_NE(result.value().find("Test Entry"), std::string::npos);
    EXPECT_NE(result.value().find("testuser"), std::string::npos);
}

TEST_F(JsonSerializerTest, SerializeEntryPretty) {
    PasswordEntry entry = createTestEntry();

    auto result = JsonSerializer::serializeEntry(entry, true);
    ASSERT_TRUE(result.isSuccess());

    // Pretty printed should have newlines
    EXPECT_NE(result.value().find('\n'), std::string::npos);
}

TEST_F(JsonSerializerTest, DeserializeEntry) {
    std::string json = R"({
        "id": "entry-456",
        "title": "Deserialized Entry",
        "username": "user123",
        "password": "pass456",
        "url": "https://test.com",
        "notes": "Some notes",
        "email": "user@test.com",
        "category": "Finance",
        "isFavorite": true,
        "isArchived": false,
        "tags": ["finance", "bank"],
        "accessCount": 5
    })";

    auto result = JsonSerializer::deserializeEntry(json);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().id, "entry-456");
    EXPECT_EQ(result.value().title, "Deserialized Entry");
    EXPECT_EQ(result.value().username, "user123");
    EXPECT_EQ(result.value().password, "pass456");
    EXPECT_EQ(result.value().url, "https://test.com");
    EXPECT_EQ(result.value().category, PasswordCategory::Finance);
    EXPECT_TRUE(result.value().isFavorite);
    EXPECT_EQ(result.value().tags.size(), 2);
    EXPECT_EQ(result.value().accessCount, 5);
}

TEST_F(JsonSerializerTest, RoundTrip) {
    PasswordEntry original = createTestEntry();

    auto serializeResult = JsonSerializer::serializeEntry(original);
    ASSERT_TRUE(serializeResult.isSuccess());

    auto deserializeResult =
        JsonSerializer::deserializeEntry(serializeResult.value());
    ASSERT_TRUE(deserializeResult.isSuccess());

    EXPECT_EQ(deserializeResult.value().id, original.id);
    EXPECT_EQ(deserializeResult.value().title, original.title);
    EXPECT_EQ(deserializeResult.value().username, original.username);
    EXPECT_EQ(deserializeResult.value().password, original.password);
    EXPECT_EQ(deserializeResult.value().url, original.url);
    EXPECT_EQ(deserializeResult.value().category, original.category);
    EXPECT_EQ(deserializeResult.value().isFavorite, original.isFavorite);
    EXPECT_EQ(deserializeResult.value().tags, original.tags);
}

TEST_F(JsonSerializerTest, SerializeEntries) {
    std::vector<PasswordEntry> entries;
    for (int i = 0; i < 3; ++i) {
        PasswordEntry entry;
        entry.id = "id-" + std::to_string(i);
        entry.title = "Entry " + std::to_string(i);
        entries.push_back(entry);
    }

    auto result = JsonSerializer::serializeEntries(entries);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_NE(result.value().find("id-0"), std::string::npos);
    EXPECT_NE(result.value().find("id-1"), std::string::npos);
    EXPECT_NE(result.value().find("id-2"), std::string::npos);
}

TEST_F(JsonSerializerTest, DeserializeEntries) {
    std::string json = R"([
        {"id": "1", "title": "Entry 1", "username": "user1", "password": "pass1"},
        {"id": "2", "title": "Entry 2", "username": "user2", "password": "pass2"},
        {"id": "3", "title": "Entry 3", "username": "user3", "password": "pass3"}
    ])";

    auto result = JsonSerializer::deserializeEntries(json);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 3);
    EXPECT_EQ(result.value()[0].id, "1");
    EXPECT_EQ(result.value()[1].id, "2");
    EXPECT_EQ(result.value()[2].id, "3");
}

TEST_F(JsonSerializerTest, DeserializeInvalidJson) {
    std::string invalidJson = "{ invalid json }";

    auto result = JsonSerializer::deserializeEntry(invalidJson);
    EXPECT_TRUE(result.isError());
}

TEST_F(JsonSerializerTest, DeserializeEmptyJson) {
    auto result = JsonSerializer::deserializeEntry("");
    EXPECT_TRUE(result.isError());
}

TEST_F(JsonSerializerTest, SerializeEmptyEntries) {
    std::vector<PasswordEntry> entries;

    auto result = JsonSerializer::serializeEntries(entries);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value(), "[]");
}

TEST_F(JsonSerializerTest, DeserializeEmptyArray) {
    auto result = JsonSerializer::deserializeEntries("[]");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_TRUE(result.value().empty());
}

TEST_F(JsonSerializerTest, SerializeWithCustomFields) {
    PasswordEntry entry;
    entry.id = "custom-fields-test";
    entry.title = "Custom Fields Entry";
    entry.addCustomField(CustomField("API Key", "abc123", true, false));
    entry.addCustomField(
        CustomField("Notes", "Multi\nline\ntext", false, true));

    auto result = JsonSerializer::serializeEntry(entry);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_NE(result.value().find("API Key"), std::string::npos);
    EXPECT_NE(result.value().find("abc123"), std::string::npos);
}

TEST_F(JsonSerializerTest, DeserializeWithCustomFields) {
    std::string json = R"({
        "id": "cf-test",
        "title": "Test",
        "customFields": [
            {"name": "Field1", "value": "Value1", "isProtected": true, "isMultiline": false},
            {"name": "Field2", "value": "Value2", "isProtected": false, "isMultiline": true}
        ]
    })";

    auto result = JsonSerializer::deserializeEntry(json);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().customFields.size(), 2);
    EXPECT_EQ(result.value().customFields[0].name, "Field1");
    EXPECT_TRUE(result.value().customFields[0].isProtected);
}

TEST_F(JsonSerializerTest, SerializeWithPasswordHistory) {
    PasswordEntry entry;
    entry.id = "history-test";
    entry.title = "History Test";
    entry.password = "current";
    entry.passwordHistory.push_back(PreviousPassword(
        "old1", std::chrono::system_clock::now() - std::chrono::hours(24)));
    entry.passwordHistory.push_back(PreviousPassword(
        "old2", std::chrono::system_clock::now() - std::chrono::hours(48)));

    auto result = JsonSerializer::serializeEntry(entry);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_NE(result.value().find("passwordHistory"), std::string::npos);
}

TEST_F(JsonSerializerTest, SerializeEntriesWithKeys) {
    std::vector<std::pair<std::string, PasswordEntry>> entries;

    PasswordEntry entry1;
    entry1.id = "1";
    entry1.title = "Entry 1";
    entries.push_back({"key1", entry1});

    PasswordEntry entry2;
    entry2.id = "2";
    entry2.title = "Entry 2";
    entries.push_back({"key2", entry2});

    auto result = JsonSerializer::serializeEntriesWithKeys(entries);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_NE(result.value().find("key1"), std::string::npos);
    EXPECT_NE(result.value().find("key2"), std::string::npos);
}

}  // namespace atom::secret::test
