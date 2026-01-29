/*
 * test_backup.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "atom/secret/storage/backup.hpp"
#include "atom/secret/storage/file_storage.hpp"

namespace atom::secret::test {

class BackupTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "atom_backup_test";
        std::filesystem::create_directories(testDir_);
        storagePath_ = testDir_ / "storage.dat";
        backupDir_ = testDir_ / "backups";
        std::filesystem::create_directories(backupDir_);
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::filesystem::path testDir_;
    std::filesystem::path storagePath_;
    std::filesystem::path backupDir_;
};

TEST_F(BackupTest, CreateBackup) {
    // Create storage with some data
    FileStorage storage(storagePath_.string());
    storage.storeString("key1", "value1");
    storage.storeString("key2", "value2");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.compress = false;
    config.encrypt = false;

    auto result = BackupManager::createBackup(storage, config);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    // Backup file should exist
    EXPECT_TRUE(std::filesystem::exists(result.value()));
}

TEST_F(BackupTest, CreateCompressedBackup) {
    FileStorage storage(storagePath_.string());
    storage.storeString("key1", "value1");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.compress = true;
    config.encrypt = false;

    auto result = BackupManager::createBackup(storage, config);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_TRUE(std::filesystem::exists(result.value()));
}

TEST_F(BackupTest, CreateEncryptedBackup) {
    FileStorage storage(storagePath_.string());
    storage.storeString("key1", "value1");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.compress = false;
    config.encrypt = true;
    config.encryptionPassword = "BackupPassword123!";

    auto result = BackupManager::createBackup(storage, config);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_TRUE(std::filesystem::exists(result.value()));
}

TEST_F(BackupTest, RestoreBackup) {
    // Create storage with data
    FileStorage originalStorage(storagePath_.string());
    originalStorage.storeString("key1", "original_value1");
    originalStorage.storeString("key2", "original_value2");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();

    auto backupResult = BackupManager::createBackup(originalStorage, config);
    ASSERT_TRUE(backupResult.isSuccess());

    // Clear original storage
    originalStorage.clear();

    // Restore from backup
    auto restoreResult = BackupManager::restoreBackup(
        originalStorage, backupResult.value(), config);
    ASSERT_TRUE(restoreResult.isSuccess()) << restoreResult.errorMessage();

    // Verify data restored
    EXPECT_EQ(originalStorage.retrieveString("key1").value(),
              "original_value1");
    EXPECT_EQ(originalStorage.retrieveString("key2").value(),
              "original_value2");
}

TEST_F(BackupTest, RestoreEncryptedBackup) {
    FileStorage storage(storagePath_.string());
    storage.storeString("secret_key", "secret_value");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.encrypt = true;
    config.encryptionPassword = "BackupPassword123!";

    auto backupResult = BackupManager::createBackup(storage, config);
    ASSERT_TRUE(backupResult.isSuccess());

    storage.clear();

    auto restoreResult =
        BackupManager::restoreBackup(storage, backupResult.value(), config);
    ASSERT_TRUE(restoreResult.isSuccess());

    EXPECT_EQ(storage.retrieveString("secret_key").value(), "secret_value");
}

TEST_F(BackupTest, RestoreWithWrongPassword) {
    FileStorage storage(storagePath_.string());
    storage.storeString("key", "value");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.encrypt = true;
    config.encryptionPassword = "CorrectPassword";

    auto backupResult = BackupManager::createBackup(storage, config);
    ASSERT_TRUE(backupResult.isSuccess());

    storage.clear();

    config.encryptionPassword = "WrongPassword";
    auto restoreResult =
        BackupManager::restoreBackup(storage, backupResult.value(), config);
    EXPECT_TRUE(restoreResult.isError());
}

TEST_F(BackupTest, ListBackups) {
    FileStorage storage(storagePath_.string());
    storage.storeString("key", "value");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();

    // Create multiple backups
    BackupManager::createBackup(storage, config);
    BackupManager::createBackup(storage, config);
    BackupManager::createBackup(storage, config);

    auto listResult = BackupManager::listBackups(backupDir_.string());
    ASSERT_TRUE(listResult.isSuccess());

    EXPECT_GE(listResult.value().size(), 3);
}

TEST_F(BackupTest, DeleteOldBackups) {
    FileStorage storage(storagePath_.string());
    storage.storeString("key", "value");

    BackupConfig config;
    config.backupDirectory = backupDir_.string();
    config.maxBackups = 2;

    // Create more backups than max
    for (int i = 0; i < 5; ++i) {
        BackupManager::createBackup(storage, config);
    }

    auto deleteResult =
        BackupManager::deleteOldBackups(backupDir_.string(), config.maxBackups);
    ASSERT_TRUE(deleteResult.isSuccess());

    auto listResult = BackupManager::listBackups(backupDir_.string());
    ASSERT_TRUE(listResult.isSuccess());

    EXPECT_LE(listResult.value().size(),
              static_cast<size_t>(config.maxBackups));
}

}  // namespace atom::secret::test
