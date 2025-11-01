#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/storage/storage.hpp"

namespace atom::system::test {

using atom::system::StorageMonitor;

// Mock filesystem operations for testing
class MockFileSystem {
public:
    MOCK_METHOD(bool, exists, (const std::string& path), (const));
    MOCK_METHOD(std::uintmax_t, space_available, (const std::string& path),
                (const));
    MOCK_METHOD(std::uintmax_t, space_capacity, (const std::string& path),
                (const));
    MOCK_METHOD(std::vector<std::string>, list_directories,
                (const std::string& path), (const));
    MOCK_METHOD(std::vector<std::string>, list_files, (const std::string& path),
                (const));
};

class StorageMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a storage monitor instance
        storageMonitor = std::make_unique<StorageMonitor>();

        // Create mock filesystem
        mockFileSystem =
            std::make_unique<::testing::NiceMock<MockFileSystem>>();

        // Set up default behavior for mock filesystem
        ON_CALL(*mockFileSystem, exists(::testing::_))
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mockFileSystem, space_available(::testing::_))
            .WillByDefault(
                ::testing::Return(1024 * 1024 * 1024));  // 1GB available
        ON_CALL(*mockFileSystem, space_capacity(::testing::_))
            .WillByDefault(::testing::Return(2048 * 1024 * 1024));  // 2GB total
        ON_CALL(*mockFileSystem, list_directories(::testing::_))
            .WillByDefault(
                ::testing::Return(std::vector<std::string>{"dir1", "dir2"}));
        ON_CALL(*mockFileSystem, list_files(::testing::_))
            .WillByDefault(::testing::Return(
                std::vector<std::string>{"file1.txt", "file2.txt"}));

        // Test paths
        testPath1 = "/test/path1";
        testPath2 = "/test/path2";
    }

    void TearDown() override {
        if (storageMonitor && storageMonitor->isRunning()) {
            storageMonitor->stopMonitoring();
        }
        storageMonitor.reset();
        mockFileSystem.reset();
    }

    std::unique_ptr<StorageMonitor> storageMonitor;
    std::unique_ptr<MockFileSystem> mockFileSystem;
    std::string testPath1;
    std::string testPath2;
};

// Test basic storage monitor creation
TEST_F(StorageMonitorTest, CreateStorageMonitor) {
    EXPECT_NE(storageMonitor, nullptr);
    EXPECT_FALSE(storageMonitor->isRunning());
}

// Test callback registration
TEST_F(StorageMonitorTest, RegisterCallback) {
    bool callbackCalled = false;
    std::string receivedPath;

    auto callback = [&callbackCalled, &receivedPath](const std::string& path) {
        callbackCalled = true;
        receivedPath = path;
    };

    EXPECT_NO_THROW(storageMonitor->registerCallback(callback));

    // Trigger callback manually
    storageMonitor->triggerCallbacks(testPath1);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(receivedPath, testPath1);
}

// Test multiple callback registration
TEST_F(StorageMonitorTest, RegisterMultipleCallbacks) {
    int callback1Called = 0;
    int callback2Called = 0;

    auto callback1 = [&callback1Called](const std::string&) {
        callback1Called++;
    };

    auto callback2 = [&callback2Called](const std::string&) {
        callback2Called++;
    };

    storageMonitor->registerCallback(callback1);
    storageMonitor->registerCallback(callback2);

    // Trigger callbacks
    storageMonitor->triggerCallbacks(testPath1);

    EXPECT_EQ(callback1Called, 1);
    EXPECT_EQ(callback2Called, 1);
}

// Test storage path management
TEST_F(StorageMonitorTest, AddStoragePath) {
    EXPECT_NO_THROW(storageMonitor->addStoragePath(testPath1));
    EXPECT_NO_THROW(storageMonitor->addStoragePath(testPath2));
}

TEST_F(StorageMonitorTest, RemoveStoragePath) {
    storageMonitor->addStoragePath(testPath1);
    storageMonitor->addStoragePath(testPath2);

    EXPECT_NO_THROW(storageMonitor->removeStoragePath(testPath1));
}

// Test monitoring start/stop
TEST_F(StorageMonitorTest, StartStopMonitoring) {
    storageMonitor->addStoragePath(testPath1);

    EXPECT_TRUE(storageMonitor->startMonitoring());
    EXPECT_TRUE(storageMonitor->isRunning());

    storageMonitor->stopMonitoring();
    EXPECT_FALSE(storageMonitor->isRunning());
}

TEST_F(StorageMonitorTest, StartMonitoringWithoutPaths) {
    // Should fail to start monitoring without any paths
    EXPECT_FALSE(storageMonitor->startMonitoring());
    EXPECT_FALSE(storageMonitor->isRunning());
}

// Test new media detection
TEST_F(StorageMonitorTest, NewMediaDetection) {
    EXPECT_CALL(*mockFileSystem, exists(testPath1))
        .WillOnce(::testing::Return(false))  // First check: not exists
        .WillOnce(::testing::Return(true));  // Second check: exists

    // First check should return false (no media)
    bool hasMedia1 = storageMonitor->isNewMediaInserted(testPath1);
    EXPECT_FALSE(hasMedia1);

    // Second check should return true (new media detected)
    bool hasMedia2 = storageMonitor->isNewMediaInserted(testPath1);
    EXPECT_TRUE(hasMedia2);
}

// Test storage listing
TEST_F(StorageMonitorTest, ListAllStorage) {
    // This test verifies that listAllStorage doesn't crash
    EXPECT_NO_THROW(storageMonitor->listAllStorage());
}

TEST_F(StorageMonitorTest, ListFiles) {
    EXPECT_CALL(*mockFileSystem, list_files(testPath1))
        .WillOnce(::testing::Return(
            std::vector<std::string>{"test1.txt", "test2.txt"}));

    EXPECT_NO_THROW(storageMonitor->listFiles(testPath1));
}

// Test storage statistics
TEST_F(StorageMonitorTest, GetStorageInfo) {
    storageMonitor->addStoragePath(testPath1);

    std::string info = storageMonitor->getStorageInfo(testPath1);
    EXPECT_FALSE(info.empty());
}

TEST_F(StorageMonitorTest, GetStorageInfoNonexistentPath) {
    std::string info = storageMonitor->getStorageInfo("/nonexistent/path");
    // Should return some info even for nonexistent paths (may be empty or error
    // message)
    EXPECT_NO_THROW(storageMonitor->getStorageInfo("/nonexistent/path"));
}

// Test storage status
TEST_F(StorageMonitorTest, GetStorageStatus) {
    storageMonitor->addStoragePath(testPath1);

    std::string status = storageMonitor->getStorageStatus();
    EXPECT_FALSE(status.empty());
}

// Test callback management
TEST_F(StorageMonitorTest, GetCallbackCount) {
    EXPECT_EQ(storageMonitor->getCallbackCount(), 0);

    auto callback = [](const std::string&) {};
    storageMonitor->registerCallback(callback);

    EXPECT_EQ(storageMonitor->getCallbackCount(), 1);
}

TEST_F(StorageMonitorTest, ClearCallbacks) {
    auto callback1 = [](const std::string&) {};
    auto callback2 = [](const std::string&) {};

    storageMonitor->registerCallback(callback1);
    storageMonitor->registerCallback(callback2);

    EXPECT_EQ(storageMonitor->getCallbackCount(), 2);

    storageMonitor->clearCallbacks();
    EXPECT_EQ(storageMonitor->getCallbackCount(), 0);
}

// Test error handling
class StorageMonitorErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        storageMonitor = std::make_unique<StorageMonitor>();
        mockFileSystem =
            std::make_unique<::testing::NiceMock<MockFileSystem>>();
    }

    void TearDown() override {
        if (storageMonitor && storageMonitor->isRunning()) {
            storageMonitor->stopMonitoring();
        }
        storageMonitor.reset();
        mockFileSystem.reset();
    }

    std::unique_ptr<StorageMonitor> storageMonitor;
    std::unique_ptr<MockFileSystem> mockFileSystem;
};

// Test invalid path handling
TEST_F(StorageMonitorErrorTest, InvalidPathHandling) {
    EXPECT_CALL(*mockFileSystem, exists(""))
        .WillRepeatedly(::testing::Return(false));

    // Empty path should be handled gracefully
    EXPECT_NO_THROW(storageMonitor->addStoragePath(""));
    EXPECT_FALSE(storageMonitor->isNewMediaInserted(""));
}

// Test filesystem error handling
TEST_F(StorageMonitorErrorTest, FilesystemErrorHandling) {
    // Should handle filesystem errors gracefully
    std::string info = storageMonitor->getStorageInfo("/error/path");
    EXPECT_NO_THROW(storageMonitor->getStorageInfo("/error/path"));
}

// Test concurrent access
TEST_F(StorageMonitorErrorTest, ConcurrentAccess) {
    std::vector<std::thread> threads;
    std::atomic<int> callbackCount{0};

    auto callback = [&callbackCount](const std::string&) { callbackCount++; };

    storageMonitor->registerCallback(callback);

    // Start multiple threads that trigger callbacks
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() {
            storageMonitor->triggerCallbacks("/test/path" + std::to_string(i));
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callbackCount.load(), 5);
}

// Performance tests
class StorageMonitorPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        storageMonitor = std::make_unique<StorageMonitor>();
    }

    void TearDown() override {
        if (storageMonitor && storageMonitor->isRunning()) {
            storageMonitor->stopMonitoring();
        }
        storageMonitor.reset();
    }

    std::unique_ptr<StorageMonitor> storageMonitor;
};

// Test callback performance
TEST_F(StorageMonitorPerformanceTest, CallbackPerformance) {
    std::atomic<int> callbackCount{0};

    auto callback = [&callbackCount](const std::string&) { callbackCount++; };

    storageMonitor->registerCallback(callback);

    auto start = std::chrono::high_resolution_clock::now();

    // Trigger many callbacks
    for (int i = 0; i < 1000; ++i) {
        storageMonitor->triggerCallbacks("/test/path");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(callbackCount.load(), 1000);
    // Should complete within reasonable time (1 second for 1000 callbacks)
    EXPECT_LT(duration.count(), 1000);
}

}  // namespace atom::system::test
