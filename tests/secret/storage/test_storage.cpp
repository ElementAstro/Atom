/*
 * test_storage.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "atom/secret/storage/storage.hpp"

namespace atom::secret::test {

class SecureStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "atom_secret_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::filesystem::path testDir_;
};

TEST_F(SecureStorageTest, CreateStorage) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);
}

TEST_F(SecureStorageTest, CreateFileStorage) {
    auto storagePath = testDir_ / "storage.dat";
    auto storage =
        SecureStorage::create(StorageBackend::File, storagePath.string());
    ASSERT_NE(storage, nullptr);
}

TEST_F(SecureStorageTest, StoreAndRetrieve) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto storeResult = storage->store("test_key", data);
    ASSERT_TRUE(storeResult.isSuccess()) << storeResult.errorMessage();

    auto retrieveResult = storage->retrieve("test_key");
    ASSERT_TRUE(retrieveResult.isSuccess()) << retrieveResult.errorMessage();

    EXPECT_EQ(retrieveResult.value(), data);
}

TEST_F(SecureStorageTest, StoreString) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    auto storeResult = storage->storeString("test_key", "Hello, World!");
    ASSERT_TRUE(storeResult.isSuccess());

    auto retrieveResult = storage->retrieveString("test_key");
    ASSERT_TRUE(retrieveResult.isSuccess());

    EXPECT_EQ(retrieveResult.value(), "Hello, World!");
}

TEST_F(SecureStorageTest, RetrieveNonExistent) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    auto result = storage->retrieve("nonexistent_key");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::StorageKeyNotFound);
}

TEST_F(SecureStorageTest, Remove) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    storage->storeString("test_key", "value");

    auto removeResult = storage->remove("test_key");
    ASSERT_TRUE(removeResult.isSuccess());

    auto retrieveResult = storage->retrieve("test_key");
    EXPECT_TRUE(retrieveResult.isError());
}

TEST_F(SecureStorageTest, RemoveNonExistent) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    auto result = storage->remove("nonexistent_key");
    EXPECT_TRUE(result.isError());
}

TEST_F(SecureStorageTest, Exists) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    EXPECT_FALSE(storage->exists("test_key"));

    storage->storeString("test_key", "value");
    EXPECT_TRUE(storage->exists("test_key"));

    storage->remove("test_key");
    EXPECT_FALSE(storage->exists("test_key"));
}

TEST_F(SecureStorageTest, ListKeys) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    storage->storeString("key1", "value1");
    storage->storeString("key2", "value2");
    storage->storeString("key3", "value3");

    auto keysResult = storage->listKeys();
    ASSERT_TRUE(keysResult.isSuccess());

    EXPECT_EQ(keysResult.value().size(), 3);
}

TEST_F(SecureStorageTest, Clear) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    storage->storeString("key1", "value1");
    storage->storeString("key2", "value2");

    auto clearResult = storage->clear();
    ASSERT_TRUE(clearResult.isSuccess());

    auto keysResult = storage->listKeys();
    ASSERT_TRUE(keysResult.isSuccess());
    EXPECT_TRUE(keysResult.value().empty());
}

TEST_F(SecureStorageTest, UpdateExisting) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    storage->storeString("test_key", "original");

    auto updateResult = storage->storeString("test_key", "updated");
    ASSERT_TRUE(updateResult.isSuccess());

    auto retrieveResult = storage->retrieveString("test_key");
    ASSERT_TRUE(retrieveResult.isSuccess());
    EXPECT_EQ(retrieveResult.value(), "updated");
}

TEST_F(SecureStorageTest, GetLocation) {
    auto storage = SecureStorage::create(StorageBackend::Memory);
    ASSERT_NE(storage, nullptr);

    std::string location = storage->getLocation();
    // Memory storage may return empty or "memory"
    (void)location;
}

TEST_F(SecureStorageTest, IsBackendAvailable) {
    // Memory backend should always be available
    EXPECT_TRUE(SecureStorage::isBackendAvailable(StorageBackend::Memory));

    // File backend should be available
    EXPECT_TRUE(SecureStorage::isBackendAvailable(StorageBackend::File));
}

}  // namespace atom::secret::test
