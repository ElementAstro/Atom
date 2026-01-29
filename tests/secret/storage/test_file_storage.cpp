/*
 * test_file_storage.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <filesystem>

#include "atom/secret/storage/file_storage.hpp"

namespace atom::secret::test {

class FileStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ =
            std::filesystem::temp_directory_path() / "atom_file_storage_test";
        std::filesystem::create_directories(testDir_);
        storagePath_ = testDir_ / "test_storage.dat";
    }

    void TearDown() override { std::filesystem::remove_all(testDir_); }

    std::filesystem::path testDir_;
    std::filesystem::path storagePath_;
};

TEST_F(FileStorageTest, CreateFileStorage) {
    FileStorage storage(storagePath_.string());
    EXPECT_EQ(storage.getLocation(), storagePath_.string());
}

TEST_F(FileStorageTest, StoreAndRetrieve) {
    FileStorage storage(storagePath_.string());

    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto storeResult = storage.store("test_key", data);
    ASSERT_TRUE(storeResult.isSuccess()) << storeResult.errorMessage();

    auto retrieveResult = storage.retrieve("test_key");
    ASSERT_TRUE(retrieveResult.isSuccess()) << retrieveResult.errorMessage();

    EXPECT_EQ(retrieveResult.value(), data);
}

TEST_F(FileStorageTest, Persistence) {
    // Store data
    {
        FileStorage storage(storagePath_.string());
        storage.storeString("persistent_key", "persistent_value");
    }

    // Retrieve in new instance
    {
        FileStorage storage(storagePath_.string());
        auto result = storage.retrieveString("persistent_key");
        ASSERT_TRUE(result.isSuccess());
        EXPECT_EQ(result.value(), "persistent_value");
    }
}

TEST_F(FileStorageTest, MultipleKeys) {
    FileStorage storage(storagePath_.string());

    storage.storeString("key1", "value1");
    storage.storeString("key2", "value2");
    storage.storeString("key3", "value3");

    EXPECT_EQ(storage.retrieveString("key1").value(), "value1");
    EXPECT_EQ(storage.retrieveString("key2").value(), "value2");
    EXPECT_EQ(storage.retrieveString("key3").value(), "value3");
}

TEST_F(FileStorageTest, LargeData) {
    FileStorage storage(storagePath_.string());

    std::vector<uint8_t> largeData(1024 * 1024);  // 1 MB
    for (size_t i = 0; i < largeData.size(); ++i) {
        largeData[i] = static_cast<uint8_t>(i % 256);
    }

    auto storeResult = storage.store("large_key", largeData);
    ASSERT_TRUE(storeResult.isSuccess());

    auto retrieveResult = storage.retrieve("large_key");
    ASSERT_TRUE(retrieveResult.isSuccess());

    EXPECT_EQ(retrieveResult.value(), largeData);
}

TEST_F(FileStorageTest, BinaryData) {
    FileStorage storage(storagePath_.string());

    std::vector<uint8_t> binaryData = {0x00, 0xFF, 0x00, 0xFF, 0x00};

    auto storeResult = storage.store("binary_key", binaryData);
    ASSERT_TRUE(storeResult.isSuccess());

    auto retrieveResult = storage.retrieve("binary_key");
    ASSERT_TRUE(retrieveResult.isSuccess());

    EXPECT_EQ(retrieveResult.value(), binaryData);
}

TEST_F(FileStorageTest, Remove) {
    FileStorage storage(storagePath_.string());

    storage.storeString("test_key", "value");
    EXPECT_TRUE(storage.exists("test_key"));

    auto removeResult = storage.remove("test_key");
    ASSERT_TRUE(removeResult.isSuccess());

    EXPECT_FALSE(storage.exists("test_key"));
}

TEST_F(FileStorageTest, Clear) {
    FileStorage storage(storagePath_.string());

    storage.storeString("key1", "value1");
    storage.storeString("key2", "value2");

    auto clearResult = storage.clear();
    ASSERT_TRUE(clearResult.isSuccess());

    EXPECT_FALSE(storage.exists("key1"));
    EXPECT_FALSE(storage.exists("key2"));
}

TEST_F(FileStorageTest, ListKeys) {
    FileStorage storage(storagePath_.string());

    storage.storeString("alpha", "a");
    storage.storeString("beta", "b");
    storage.storeString("gamma", "c");

    auto keysResult = storage.listKeys();
    ASSERT_TRUE(keysResult.isSuccess());

    EXPECT_EQ(keysResult.value().size(), 3);
}

}  // namespace atom::secret::test
