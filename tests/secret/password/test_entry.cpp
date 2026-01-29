/*
 * test_entry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/password/entry.hpp"

namespace atom::secret::test {

class PasswordEntryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PasswordEntryTest, DefaultConstruction) {
    PasswordEntry entry;

    EXPECT_TRUE(entry.id.empty());
    EXPECT_TRUE(entry.title.empty());
    EXPECT_TRUE(entry.username.empty());
    EXPECT_TRUE(entry.password.empty());
    EXPECT_EQ(entry.category, PasswordCategory::General);
    EXPECT_FALSE(entry.isFavorite);
    EXPECT_FALSE(entry.isArchived);
}

TEST_F(PasswordEntryTest, CreateWithGeneratedId) {
    PasswordEntry entry = PasswordEntry::create();

    EXPECT_FALSE(entry.id.empty());
    EXPECT_NE(entry.created, std::chrono::system_clock::time_point{});
    EXPECT_NE(entry.modified, std::chrono::system_clock::time_point{});
}

TEST_F(PasswordEntryTest, GenerateId) {
    PasswordEntry entry;
    EXPECT_TRUE(entry.id.empty());

    entry.generateId();
    EXPECT_FALSE(entry.id.empty());

    // UUID format check (basic)
    EXPECT_GE(entry.id.length(), 32);
}

TEST_F(PasswordEntryTest, IsEmpty) {
    PasswordEntry entry;
    EXPECT_TRUE(entry.isEmpty());

    entry.password = "secret";
    EXPECT_FALSE(entry.isEmpty());
}

TEST_F(PasswordEntryTest, IsExpired) {
    PasswordEntry entry;

    // No expiration set
    EXPECT_FALSE(entry.isExpired());

    // Set expiration in the past
    entry.expires = std::chrono::system_clock::now() - std::chrono::hours(1);
    EXPECT_TRUE(entry.isExpired());

    // Set expiration in the future
    entry.expires = std::chrono::system_clock::now() + std::chrono::hours(1);
    EXPECT_FALSE(entry.isExpired());
}

TEST_F(PasswordEntryTest, IsExpiringSoon) {
    PasswordEntry entry;

    // No expiration set
    EXPECT_FALSE(entry.isExpiringSoon(7));

    // Expires in 3 days
    entry.expires = std::chrono::system_clock::now() + std::chrono::hours(72);
    EXPECT_TRUE(entry.isExpiringSoon(7));
    EXPECT_FALSE(entry.isExpiringSoon(1));

    // Already expired
    entry.expires = std::chrono::system_clock::now() - std::chrono::hours(1);
    EXPECT_FALSE(entry.isExpiringSoon(7));
}

TEST_F(PasswordEntryTest, AddTag) {
    PasswordEntry entry;

    entry.addTag("work");
    EXPECT_EQ(entry.tags.size(), 1);
    EXPECT_EQ(entry.tags[0], "work");

    // Adding same tag again should not duplicate
    entry.addTag("work");
    EXPECT_EQ(entry.tags.size(), 1);

    entry.addTag("personal");
    EXPECT_EQ(entry.tags.size(), 2);
}

TEST_F(PasswordEntryTest, RemoveTag) {
    PasswordEntry entry;
    entry.addTag("work");
    entry.addTag("personal");

    EXPECT_TRUE(entry.removeTag("work"));
    EXPECT_EQ(entry.tags.size(), 1);

    EXPECT_FALSE(entry.removeTag("nonexistent"));
    EXPECT_EQ(entry.tags.size(), 1);
}

TEST_F(PasswordEntryTest, HasTag) {
    PasswordEntry entry;
    entry.addTag("work");

    EXPECT_TRUE(entry.hasTag("work"));
    EXPECT_FALSE(entry.hasTag("personal"));
}

TEST_F(PasswordEntryTest, AddCustomField) {
    PasswordEntry entry;

    CustomField field("API Key", "abc123", true, false);
    entry.addCustomField(field);

    EXPECT_EQ(entry.customFields.size(), 1);
    EXPECT_EQ(entry.customFields[0].name, "API Key");
    EXPECT_TRUE(entry.customFields[0].isProtected);
}

TEST_F(PasswordEntryTest, GetCustomField) {
    PasswordEntry entry;
    entry.addCustomField(CustomField("field1", "value1"));
    entry.addCustomField(CustomField("field2", "value2"));

    auto* field = entry.getCustomField("field1");
    ASSERT_NE(field, nullptr);
    EXPECT_EQ(field->value, "value1");

    auto* notFound = entry.getCustomField("nonexistent");
    EXPECT_EQ(notFound, nullptr);
}

TEST_F(PasswordEntryTest, UpdatePassword) {
    PasswordEntry entry;
    entry.password = "oldPassword";

    entry.updatePassword("newPassword");

    EXPECT_EQ(entry.password, "newPassword");
    EXPECT_EQ(entry.passwordHistory.size(), 1);
    EXPECT_EQ(entry.passwordHistory[0].password, "oldPassword");
}

TEST_F(PasswordEntryTest, UpdatePasswordHistory) {
    PasswordEntry entry;
    entry.password = "password0";

    // Update password multiple times
    for (int i = 1; i <= 15; ++i) {
        entry.updatePassword("password" + std::to_string(i), 10);
    }

    // History should be limited to 10
    EXPECT_EQ(entry.passwordHistory.size(), 10);
    EXPECT_EQ(entry.password, "password15");
}

TEST_F(PasswordEntryTest, MarkAccessed) {
    PasswordEntry entry;
    EXPECT_EQ(entry.accessCount, 0);

    entry.markAccessed();
    EXPECT_EQ(entry.accessCount, 1);
    EXPECT_NE(entry.lastAccessed, std::chrono::system_clock::time_point{});

    entry.markAccessed();
    EXPECT_EQ(entry.accessCount, 2);
}

TEST_F(PasswordEntryTest, CategoryToString) {
    EXPECT_EQ(categoryToString(PasswordCategory::General), "General");
    EXPECT_EQ(categoryToString(PasswordCategory::Finance), "Finance");
    EXPECT_EQ(categoryToString(PasswordCategory::Work), "Work");
    EXPECT_EQ(categoryToString(PasswordCategory::Email), "Email");
    EXPECT_EQ(categoryToString(PasswordCategory::Development), "Development");
}

TEST_F(PasswordEntryTest, StringToCategory) {
    EXPECT_EQ(stringToCategory("General"), PasswordCategory::General);
    EXPECT_EQ(stringToCategory("Finance"), PasswordCategory::Finance);
    EXPECT_EQ(stringToCategory("Work"), PasswordCategory::Work);
    EXPECT_EQ(stringToCategory("Unknown"), PasswordCategory::General);
}

TEST_F(PasswordEntryTest, StrengthToString) {
    EXPECT_EQ(strengthToString(PasswordStrength::VeryWeak), "Very Weak");
    EXPECT_EQ(strengthToString(PasswordStrength::Weak), "Weak");
    EXPECT_EQ(strengthToString(PasswordStrength::Medium), "Medium");
    EXPECT_EQ(strengthToString(PasswordStrength::Strong), "Strong");
    EXPECT_EQ(strengthToString(PasswordStrength::VeryStrong), "Very Strong");
}

}  // namespace atom::secret::test
