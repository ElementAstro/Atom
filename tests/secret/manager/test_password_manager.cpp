/*
 * test_password_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "atom/secret/manager/password_manager.hpp"
#include "atom/secret/storage/storage.hpp"

namespace atom::secret::test {

class PasswordManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "atom_pm_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::unique_ptr<PasswordManager> createManager() {
        auto storage = SecureStorage::create(StorageBackend::Memory);
        return std::make_unique<PasswordManager>(std::move(storage));
    }

    std::filesystem::path testDir_;
};

TEST_F(PasswordManagerTest, Initialize) {
    auto manager = createManager();

    auto result = manager->initialize("MasterPassword123!");
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_TRUE(manager->isUnlocked());
}

TEST_F(PasswordManagerTest, InitializeShortPassword) {
    auto manager = createManager();

    auto result = manager->initialize("short");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::PasswordTooShort);
}

TEST_F(PasswordManagerTest, InitializeEmptyPassword) {
    auto manager = createManager();

    auto result = manager->initialize("");
    EXPECT_TRUE(result.isError());
}

TEST_F(PasswordManagerTest, LockUnlock) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    EXPECT_TRUE(manager->isUnlocked());

    manager->lock();
    EXPECT_FALSE(manager->isUnlocked());

    auto unlockResult = manager->unlock("MasterPassword123!");
    ASSERT_TRUE(unlockResult.isSuccess());
    EXPECT_TRUE(manager->isUnlocked());
}

TEST_F(PasswordManagerTest, AddEntry) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Test Entry";
    entry.username = "testuser";
    entry.password = "testpassword";
    entry.url = "https://example.com";

    auto result = manager->addEntry(entry);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_FALSE(result.value().empty());
    EXPECT_EQ(manager->getEntryCount(), 1);
}

TEST_F(PasswordManagerTest, AddEntryWhenLocked) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");
    manager->lock();

    PasswordEntry entry;
    entry.title = "Test";

    auto result = manager->addEntry(entry);
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::ManagerLocked);
}

TEST_F(PasswordManagerTest, GetEntry) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Test Entry";
    entry.username = "testuser";
    entry.password = "testpassword";

    auto addResult = manager->addEntry(entry);
    ASSERT_TRUE(addResult.isSuccess());

    auto getResult = manager->getEntry(addResult.value());
    ASSERT_TRUE(getResult.isSuccess());

    EXPECT_EQ(getResult.value().title, "Test Entry");
    EXPECT_EQ(getResult.value().username, "testuser");
}

TEST_F(PasswordManagerTest, GetEntryNotFound) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    auto result = manager->getEntry("nonexistent-id");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::EntryNotFound);
}

TEST_F(PasswordManagerTest, UpdateEntry) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Original Title";

    auto addResult = manager->addEntry(entry);
    ASSERT_TRUE(addResult.isSuccess());

    auto getResult = manager->getEntry(addResult.value());
    ASSERT_TRUE(getResult.isSuccess());

    PasswordEntry updated = getResult.value();
    updated.title = "Updated Title";

    auto updateResult = manager->updateEntry(updated);
    ASSERT_TRUE(updateResult.isSuccess());

    auto verifyResult = manager->getEntry(addResult.value());
    ASSERT_TRUE(verifyResult.isSuccess());
    EXPECT_EQ(verifyResult.value().title, "Updated Title");
}

TEST_F(PasswordManagerTest, DeleteEntry) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "To Delete";

    auto addResult = manager->addEntry(entry);
    ASSERT_TRUE(addResult.isSuccess());

    EXPECT_EQ(manager->getEntryCount(), 1);

    auto deleteResult = manager->deleteEntry(addResult.value());
    ASSERT_TRUE(deleteResult.isSuccess());

    EXPECT_EQ(manager->getEntryCount(), 0);
}

TEST_F(PasswordManagerTest, GetAllEntries) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    for (int i = 0; i < 5; ++i) {
        PasswordEntry entry;
        entry.title = "Entry " + std::to_string(i);
        manager->addEntry(entry);
    }

    auto result = manager->getAllEntries();
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 5);
}

TEST_F(PasswordManagerTest, SearchByQuery) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry1;
    entry1.title = "GitHub Account";
    entry1.username = "developer";
    manager->addEntry(entry1);

    PasswordEntry entry2;
    entry2.title = "Gmail Account";
    entry2.username = "user@gmail.com";
    manager->addEntry(entry2);

    PasswordEntry entry3;
    entry3.title = "Bank Account";
    entry3.username = "customer";
    manager->addEntry(entry3);

    SearchFilter filter;
    filter.query = "Account";

    auto result = manager->search(filter);
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 3);

    filter.query = "Git";
    result = manager->search(filter);
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 1);
}

TEST_F(PasswordManagerTest, SearchByCategory) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry1;
    entry1.title = "Work Email";
    entry1.category = PasswordCategory::Work;
    manager->addEntry(entry1);

    PasswordEntry entry2;
    entry2.title = "Personal Email";
    entry2.category = PasswordCategory::Personal;
    manager->addEntry(entry2);

    SearchFilter filter;
    filter.categories = {PasswordCategory::Work};

    auto result = manager->search(filter);
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0].title, "Work Email");
}

TEST_F(PasswordManagerTest, SearchFavoritesOnly) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry1;
    entry1.title = "Favorite";
    entry1.isFavorite = true;
    manager->addEntry(entry1);

    PasswordEntry entry2;
    entry2.title = "Not Favorite";
    entry2.isFavorite = false;
    manager->addEntry(entry2);

    SearchFilter filter;
    filter.favoritesOnly = true;

    auto result = manager->search(filter);
    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 1);
    EXPECT_EQ(result.value()[0].title, "Favorite");
}

TEST_F(PasswordManagerTest, GetPassword) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Test";
    entry.password = "SecretPassword";

    auto addResult = manager->addEntry(entry);
    ASSERT_TRUE(addResult.isSuccess());

    auto passwordResult = manager->getPassword(addResult.value());
    ASSERT_TRUE(passwordResult.isSuccess());
    EXPECT_EQ(passwordResult.value(), "SecretPassword");
}

TEST_F(PasswordManagerTest, UpdatePassword) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Test";
    entry.password = "OldPassword";

    auto addResult = manager->addEntry(entry);
    ASSERT_TRUE(addResult.isSuccess());

    auto updateResult =
        manager->updatePassword(addResult.value(), "NewPassword");
    ASSERT_TRUE(updateResult.isSuccess());

    auto passwordResult = manager->getPassword(addResult.value());
    ASSERT_TRUE(passwordResult.isSuccess());
    EXPECT_EQ(passwordResult.value(), "NewPassword");
}

TEST_F(PasswordManagerTest, ExportToJson) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    PasswordEntry entry;
    entry.title = "Export Test";
    entry.username = "user";
    entry.password = "pass";
    manager->addEntry(entry);

    auto result = manager->exportToJson();
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().empty());
    EXPECT_NE(result.value().find("Export Test"), std::string::npos);
}

TEST_F(PasswordManagerTest, ImportFromJson) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");

    std::string json = R"([
        {
            "id": "test-id-1",
            "title": "Imported Entry",
            "username": "imported_user",
            "password": "imported_pass"
        }
    ])";

    auto result = manager->importFromJson(json);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value(), 1);
    EXPECT_EQ(manager->getEntryCount(), 1);
}

TEST_F(PasswordManagerTest, ChangeMasterPassword) {
    auto manager = createManager();
    manager->initialize("OldMasterPassword!");

    auto result = manager->changeMasterPassword("OldMasterPassword!",
                                                "NewMasterPassword!");
    ASSERT_TRUE(result.isSuccess());
}

TEST_F(PasswordManagerTest, ChangeMasterPasswordWhenLocked) {
    auto manager = createManager();
    manager->initialize("MasterPassword123!");
    manager->lock();

    auto result =
        manager->changeMasterPassword("MasterPassword123!", "NewPassword!");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::ManagerLocked);
}

}  // namespace atom::secret::test
