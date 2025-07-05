#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <thread>
#include <vector>

#include "atom/memory/shared.hpp"

using namespace atom::connection;

// Sample trivially copyable struct for testing
struct alignas(16) TestData {
    int a;
    double b;
};

// Test fixture for SharedMemory
class SharedMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        shm_name_ = "TestSharedMemory";
        if (SharedMemory<TestData>::exists(shm_name_)) {
            // Cleanup before test
#ifdef _WIN32
            HANDLE h =
                OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name_.c_str());
            if (h) {
                CloseHandle(h);
            }
#else
            shm_unlink(shm_name_.c_str());
#endif
        }
    }

    void TearDown() override {
        if (SharedMemory<TestData>::exists(shm_name_)) {
#ifdef _WIN32
            HANDLE h =
                OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name_.c_str());
            if (h) {
                CloseHandle(h);
            }
#else
            shm_unlink(shm_name_.c_str());
#endif
        }
    }

    std::string shm_name_;
};

TEST_F(SharedMemoryTest, ConstructorCreatesSharedMemory) {
    EXPECT_NO_THROW({ SharedMemory<TestData> shm(shm_name_, true); });

    EXPECT_TRUE(SharedMemory<TestData>::exists(shm_name_));
}

TEST_F(SharedMemoryTest, WriteAndRead) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int K_MAGIC_NUMBER_A = 42;
    const double K_MAGIC_NUMBER_B = 3.14;
    TestData data = {K_MAGIC_NUMBER_A, K_MAGIC_NUMBER_B};
    shm.write(data);

    TestData readData = shm.read();
    EXPECT_EQ(readData.a, data.a);
    EXPECT_DOUBLE_EQ(readData.b, data.b);
}

TEST_F(SharedMemoryTest, ClearSharedMemory) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int K_MAGIC_NUMBER_A = 42;
    const double K_MAGIC_NUMBER_B = 3.14;
    TestData data = {K_MAGIC_NUMBER_A, K_MAGIC_NUMBER_B};
    shm.write(data);

    shm.clear();

    TestData readData = shm.read();
    EXPECT_EQ(readData.a, 0);
    EXPECT_DOUBLE_EQ(readData.b, 0.0);
}

TEST_F(SharedMemoryTest, ResizeSharedMemory) {
    SharedMemory<TestData> shm(shm_name_, true);
    EXPECT_EQ(shm.getSize(), sizeof(TestData));

    shm.resize(sizeof(TestData) * 2);
    EXPECT_EQ(shm.getSize(), sizeof(TestData) * 2);
}

TEST_F(SharedMemoryTest, ExistsMethod) {
    EXPECT_FALSE(SharedMemory<TestData>::exists(shm_name_));

    SharedMemory<TestData> shm(shm_name_, true);
    EXPECT_TRUE(SharedMemory<TestData>::exists(shm_name_));
}

TEST_F(SharedMemoryTest, PartialWriteAndRead) {
    SharedMemory<TestData> shm(shm_name_, true);

    const int K_PARTIAL_A = 100;
    shm.writePartial(K_PARTIAL_A, offsetof(TestData, a));

    const double K_PARTIAL_B = 6.28;
    shm.writePartial(K_PARTIAL_B, offsetof(TestData, b));

    auto readA = shm.readPartial<int>(offsetof(TestData, a));
    auto readB = shm.readPartial<double>(offsetof(TestData, b));

    EXPECT_EQ(readA, K_PARTIAL_A);
    EXPECT_DOUBLE_EQ(readB, K_PARTIAL_B);
}

TEST_F(SharedMemoryTest, WritePartialOutOfBounds) {
    SharedMemory<TestData> shm(shm_name_, true);
    const int K_DATA = 100;
    EXPECT_THROW(
        {
            shm.writePartial(K_DATA, sizeof(TestData));  // Offset out of bounds
        },
        SharedMemoryException);
}

TEST_F(SharedMemoryTest, ReadPartialOutOfBounds) {
    SharedMemory<TestData> shm(shm_name_, true);
    EXPECT_THROW(
        {
            (void)shm.readPartial<int>(
                sizeof(TestData));  // Offset out of bounds
        },
        SharedMemoryException);
}

TEST_F(SharedMemoryTest, TryReadSuccess) {
    SharedMemory<TestData> shm(shm_name_, true);
    const int K_MAGIC_NUMBER_A = 42;
    const double K_MAGIC_NUMBER_B = 3.14;
    TestData data = {K_MAGIC_NUMBER_A, K_MAGIC_NUMBER_B};
    shm.write(data);

    auto result = shm.tryRead();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->a, data.a);
    EXPECT_DOUBLE_EQ(result->b, data.b);
}

TEST_F(SharedMemoryTest, TryReadFailure) {
    SharedMemory<TestData> shm(shm_name_, true);
    shm.clear();

    // Simulate timeout by using a very short timeout and holding the lock
    std::atomic<bool> lockAcquired{false};
    std::thread lockThread([&shm, &lockAcquired]() {
        shm.withLock(
            [&]() {
                lockAcquired = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            },
            std::chrono::milliseconds(200));
    });

    while (!lockAcquired.load()) {
        std::this_thread::yield();
    }

    auto result = shm.tryRead(std::chrono::milliseconds(10));
    EXPECT_FALSE(result.has_value());

    lockThread.join();
}

TEST_F(SharedMemoryTest, WriteAndReadSpan) {
    SharedMemory<TestData> shm(shm_name_, true);
    std::array<std::byte, sizeof(TestData)> dataBytes = {
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    std::span<const std::byte> dataSpan(dataBytes);
    shm.writeSpan(dataSpan);

    std::array<std::byte, sizeof(TestData)> readBytes;
    std::span<std::byte> readSpan(readBytes);
    size_t bytesRead = shm.readSpan(readSpan);
    EXPECT_EQ(bytesRead, sizeof(TestData));
    EXPECT_EQ(std::memcmp(dataBytes.data(), readBytes.data(), sizeof(TestData)),
              0);
}

TEST_F(SharedMemoryTest, WriteSpanOutOfBounds) {
    SharedMemory<TestData> shm(shm_name_, true);
    std::vector<std::byte> data(sizeof(TestData) + 1, std::byte{0});
    std::span<const std::byte> dataSpan(data.data(), data.size());

    EXPECT_THROW({ shm.writeSpan(dataSpan); }, SharedMemoryException);
}

TEST_F(SharedMemoryTest, ReadSpanPartial) {
    SharedMemory<TestData> shm(shm_name_, true);
    const int K_MAGIC_NUMBER_A = 42;
    const double K_MAGIC_NUMBER_B = 3.14;
    TestData data = {K_MAGIC_NUMBER_A, K_MAGIC_NUMBER_B};
    shm.write(data);

    std::vector<std::byte> readBytes(sizeof(TestData) - 4, std::byte{0});
    std::span<std::byte> readSpan(readBytes.data(), readBytes.size());
    size_t bytesRead = shm.readSpan(readSpan);
    EXPECT_EQ(bytesRead, readBytes.size());
}

// Test async operations
TEST_F(SharedMemoryTest, AsyncReadWrite) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Write data and read it back asynchronously
    TestData writeData = {123, 456.789};
    auto writeFuture = shm.writeAsync(writeData);

    // Wait for the write to complete
    EXPECT_NO_THROW(writeFuture.get());

    // Read data asynchronously
    auto readFuture = shm.readAsync();

    // Wait for the read to complete and check the result
    TestData readData;
    EXPECT_NO_THROW(readData = readFuture.get());
    EXPECT_EQ(readData.a, writeData.a);
    EXPECT_DOUBLE_EQ(readData.b, writeData.b);
}

// Test change callbacks
TEST_F(SharedMemoryTest, ChangeCallbacks) {
    SharedMemory<TestData> shm(shm_name_, true);

    TestData callbackData = {0, 0.0};
    std::atomic<int> callbackCount{0};

    // Register a callback
    size_t callbackId = shm.registerChangeCallback(
        [&callbackData, &callbackCount](const TestData& data) {
            callbackData = data;
            callbackCount++;
        });

    // Write data to trigger the callback
    TestData writeData = {123, 456.789};
    shm.write(writeData);

    // Give the callback thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check that the callback was called
    EXPECT_EQ(callbackCount.load(), 1);
    EXPECT_EQ(callbackData.a, writeData.a);
    EXPECT_DOUBLE_EQ(callbackData.b, writeData.b);

    // Unregister the callback
    EXPECT_TRUE(shm.unregisterChangeCallback(callbackId));

    // Write again, callback should not be called
    TestData newData = {456, 789.123};
    shm.write(newData);

    // Give the callback thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check that the callback count didn't change
    EXPECT_EQ(callbackCount.load(), 1);

    // Try to unregister a non-existent callback
    EXPECT_FALSE(shm.unregisterChangeCallback(9999));
}

// Test waiting for changes
TEST_F(SharedMemoryTest, WaitForChange) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Initial data
    TestData initialData = {1, 1.1};
    shm.write(initialData);

    // Start a thread that will wait for a change
    std::atomic<bool> changeDetected{false};
    std::thread waitThread([&shm, &changeDetected]() {
        // Wait for a change with a timeout
        bool result = shm.waitForChange(std::chrono::milliseconds(500));
        changeDetected = result;
    });

    // Give the thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Write new data to trigger the change
    TestData newData = {2, 2.2};
    shm.write(newData);

    // Wait for the thread to finish
    waitThread.join();

    // Check that the change was detected
    EXPECT_TRUE(changeDetected);

    // Test timeout behavior
    std::atomic<bool> timeoutResult{false};
    std::thread timeoutThread([&shm, &timeoutResult]() {
        // No change should occur within this timeout
        bool result = shm.waitForChange(std::chrono::milliseconds(100));
        timeoutResult = result;
    });

    // Wait for the thread to timeout
    timeoutThread.join();

    // No change occurred, so timeoutResult should be false
    EXPECT_FALSE(timeoutResult);
}

// Test error conditions
TEST_F(SharedMemoryTest, ErrorConditions) {
    // Test opening non-existent shared memory
    EXPECT_THROW(
        { SharedMemory<TestData> shm(shm_name_ + "_nonexistent", false); },
        SharedMemoryException);

    // Test creating shared memory that already exists
    {
        SharedMemory<TestData> shm1(shm_name_, true);

        EXPECT_THROW(
            { SharedMemory<TestData> shm2(shm_name_, true); },
            SharedMemoryException);
    }

    // Test trying to resize from non-creator process
    {
        SharedMemory<TestData> creator(shm_name_, true);
        SharedMemory<TestData> accessor(shm_name_, false);

        EXPECT_THROW(
            { accessor.resize(sizeof(TestData) * 2); }, SharedMemoryException);
    }

    // Test reading uninitialized memory
    {
        SharedMemory<TestData> shm(shm_name_, true);
        // Don't write any data, just try to read

        EXPECT_THROW(
            { [[maybe_unused]] TestData data = shm.read(); },
            SharedMemoryException);
    }
}

// Test multiple processes accessing shared memory
// Note: This is a simplified simulation of multiple processes using threads
TEST_F(SharedMemoryTest, MultipleProcessesSimulation) {
    const int numProcesses = 5;
    const int updatesPerProcess = 10;

    // Create the shared memory in the main "process"
    SharedMemory<TestData> mainShm(shm_name_, true);
    TestData initialData = {0, 0.0};
    mainShm.write(initialData);

    // Function to simulate a process that reads and updates shared memory
    auto processFunc = [this](int processId) {
        try {
            // Open existing shared memory
            SharedMemory<TestData> shm(shm_name_, false);

            for (int i = 0; i < updatesPerProcess; i++) {
                // Read current value
                auto currentData = shm.read();

                // Update value
                TestData newData = {
                    currentData.a + processId,
                    currentData.b + static_cast<double>(processId) / 10.0};

                // Write back the updated value
                shm.write(newData);

                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const SharedMemoryException& e) {
            ADD_FAILURE() << "Exception in process " << processId << ": "
                          << e.what();
        }
    };

    // Start "processes"
    std::vector<std::thread> threads;
    for (int i = 1; i <= numProcesses; i++) {
        threads.emplace_back(processFunc, i);
    }

    // Wait for all "processes" to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Check final value
    TestData finalData = mainShm.read();

    // Calculate expected values based on the updates
    // Each process adds its ID to a and its ID/10.0 to b, updatesPerProcess
    // times
    int expectedA = initialData.a;
    double expectedB = initialData.b;
    for (int i = 1; i <= numProcesses; i++) {
        expectedA += i * updatesPerProcess;
        expectedB += (static_cast<double>(i) / 10.0) * updatesPerProcess;
    }

    EXPECT_EQ(finalData.a, expectedA);
    EXPECT_DOUBLE_EQ(finalData.b, expectedB);
}

// Test shared memory version tracking
TEST_F(SharedMemoryTest, VersionTracking) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Initial version should be 0
    EXPECT_EQ(shm.getVersion(), 0);

    // Write data, version should increase
    TestData data = {1, 1.1};
    shm.write(data);
    EXPECT_EQ(shm.getVersion(), 1);

    // Write again, version should increase
    data = {2, 2.2};
    shm.write(data);
    EXPECT_EQ(shm.getVersion(), 2);

    // Clear should also increase version
    shm.clear();
    EXPECT_EQ(shm.getVersion(), 3);

    // Partial write should increase version
    int partialData = 42;
    shm.writePartial(partialData, offsetof(TestData, a));
    EXPECT_EQ(shm.getVersion(), 4);
}

// Test shared memory initialization checks
TEST_F(SharedMemoryTest, InitializationStatus) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Initially not initialized
    EXPECT_FALSE(shm.isInitialized());

    // After write, should be initialized
    TestData data = {1, 1.1};
    shm.write(data);
    EXPECT_TRUE(shm.isInitialized());

    // After clear, should be not initialized again
    shm.clear();
    EXPECT_FALSE(shm.isInitialized());
}

// Test native handle access
TEST_F(SharedMemoryTest, NativeHandle) {
    SharedMemory<TestData> shm(shm_name_, true);

    void* handle = shm.getNativeHandle();
#ifdef _WIN32
    // On Windows, the handle should be a valid HANDLE
    EXPECT_NE(handle, nullptr);
    EXPECT_NE(handle, INVALID_HANDLE_VALUE);
#else
    // On POSIX, the handle is actually an int file descriptor cast to void*
    // It should not be -1 (invalid fd)
    EXPECT_NE(reinterpret_cast<intptr_t>(handle), -1);
#endif
}

// Test creator status
TEST_F(SharedMemoryTest, CreatorStatus) {
    // Create shared memory
    SharedMemory<TestData> creator(shm_name_, true);
    EXPECT_TRUE(creator.isCreator());

    // Access existing shared memory
    SharedMemory<TestData> accessor(shm_name_, false);
    EXPECT_FALSE(accessor.isCreator());
}

// Test edge cases for span operations
TEST_F(SharedMemoryTest, SpanEdgeCases) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Edge case: Empty span
    std::array<std::byte, 0> emptyArray;
    std::span<const std::byte> emptySpan(emptyArray);
    EXPECT_NO_THROW(shm.writeSpan(emptySpan));

    // Edge case: Partially filled span (reading less than available)
    std::array<std::byte, sizeof(TestData)> fullArray;
    std::fill(fullArray.begin(), fullArray.end(), std::byte{42});
    shm.writeSpan(std::span<const std::byte>(fullArray));

    std::array<std::byte, sizeof(TestData) / 2> halfArray;
    std::span<std::byte> halfSpan(halfArray);
    size_t bytesRead = shm.readSpan(halfSpan);
    EXPECT_EQ(bytesRead, sizeof(TestData) / 2);

    // Verify the read data
    for (size_t i = 0; i < halfArray.size(); i++) {
        EXPECT_EQ(halfArray[i], std::byte{42});
    }
}

// Test initializing shared memory with initial data
TEST_F(SharedMemoryTest, InitialData) {
    TestData initialData = {42, 3.14159};

    // Create shared memory with initial data
    SharedMemory<TestData> shm(shm_name_, true, initialData);

    // Read data and verify it matches initial data
    TestData readData = shm.read();
    EXPECT_EQ(readData.a, initialData.a);
    EXPECT_DOUBLE_EQ(readData.b, initialData.b);

    // Verify it's marked as initialized
    EXPECT_TRUE(shm.isInitialized());
}

// Test exception error codes
TEST_F(SharedMemoryTest, ExceptionErrorCodes) {
    try {
        // Attempt to open non-existent shared memory
        SharedMemory<TestData> shm(shm_name_ + "_nonexistent", false);
        FAIL() << "Expected SharedMemoryException";
    } catch (const SharedMemoryException& e) {
        EXPECT_EQ(e.getErrorCode(),
                  SharedMemoryException::ErrorCode::NOT_FOUND);
        EXPECT_EQ(e.getErrorCodeString(), "NOT_FOUND");
    }

    // Create shared memory
    SharedMemory<TestData> shm1(shm_name_, true);

    try {
        // Attempt to create already existing shared memory
        SharedMemory<TestData> shm2(shm_name_, true);
        FAIL() << "Expected SharedMemoryException";
    } catch (const SharedMemoryException& e) {
        EXPECT_EQ(e.getErrorCode(),
                  SharedMemoryException::ErrorCode::ALREADY_EXISTS);
        EXPECT_EQ(e.getErrorCodeString(), "ALREADY_EXISTS");
    }

    try {
        // Attempt to write span that's too large
        std::vector<std::byte> largeData(sizeof(TestData) + 1, std::byte{0});
        shm1.writeSpan(std::span<const std::byte>(largeData));
        FAIL() << "Expected SharedMemoryException";
    } catch (const SharedMemoryException& e) {
        EXPECT_EQ(e.getErrorCode(),
                  SharedMemoryException::ErrorCode::SIZE_ERROR);
        EXPECT_EQ(e.getErrorCodeString(), "SIZE_ERROR");
    }
}

// Test concurrent read/write operations
TEST_F(SharedMemoryTest, ConcurrentReadWrite) {
    SharedMemory<TestData> shm(shm_name_, true);

    // Initial data
    TestData initialData = {0, 0.0};
    shm.write(initialData);

    // Number of concurrent readers and writers
    const int numReaders = 5;
    const int numWriters = 3;
    const int operationsPerThread = 50;

    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    std::atomic<int> errorCount{0};

    // Reader function
    auto readerFunc = [&]() {
        for (int i = 0; i < operationsPerThread; i++) {
            try {
                [[maybe_unused]] TestData data = shm.read();
                readCount++;
            } catch (const SharedMemoryException& e) {
                errorCount++;
            }
        }
    };

    // Writer function
    auto writerFunc = [&]() {
        for (int i = 0; i < operationsPerThread; i++) {
            try {
                // Create incrementing values
                TestData data = {i, static_cast<double>(i)};
                shm.write(data);
                writeCount++;
            } catch (const SharedMemoryException& e) {
                errorCount++;
            }
        }
    };

    // Start reader and writer threads
    std::vector<std::thread> threads;

    for (int i = 0; i < numReaders; i++) {
        threads.emplace_back(readerFunc);
    }

    for (int i = 0; i < numWriters; i++) {
        threads.emplace_back(writerFunc);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Check operations were completed successfully
    EXPECT_EQ(readCount, numReaders * operationsPerThread)
        << "Not all read operations completed";
    EXPECT_EQ(writeCount, numWriters * operationsPerThread)
        << "Not all write operations completed";
    EXPECT_EQ(errorCount, 0) << "Some operations resulted in errors";

    // Final data should be from one of the last write operations
    TestData finalData = shm.read();
    EXPECT_LE(finalData.a, operationsPerThread - 1);
    EXPECT_GE(finalData.a, 0);
}

// Test handling of very large TestData structures
TEST_F(SharedMemoryTest, LargeStructure) {
    // Define a larger test structure
    struct LargeTestData {
        int values[1024];
        char string[4096];
        double doubles[512];
    };

    static_assert(std::is_trivially_copyable<LargeTestData>::value,
                  "LargeTestData must be trivially copyable");

    std::string largeShmName = shm_name_ + "_large";

    // Create shared memory for large structure
    SharedMemory<LargeTestData> largeShm(largeShmName, true);

    // Initialize and write large structure
    LargeTestData writeData;
    for (int i = 0; i < 1024; i++) {
        writeData.values[i] = i;
    }
    for (int i = 0; i < 4096; i++) {
        writeData.string[i] = 'A' + (i % 26);
    }
    for (int i = 0; i < 512; i++) {
        writeData.doubles[i] = static_cast<double>(i) / 3.14159;
    }

    largeShm.write(writeData);

    // Read back and verify
    LargeTestData readData = largeShm.read();

    bool valuesMatch = true;
    bool stringMatches = true;
    bool doublesMatch = true;

    for (int i = 0; i < 1024; i++) {
        if (readData.values[i] != writeData.values[i]) {
            valuesMatch = false;
            break;
        }
    }

    for (int i = 0; i < 4096; i++) {
        if (readData.string[i] != writeData.string[i]) {
            stringMatches = false;
            break;
        }
    }

    for (int i = 0; i < 512; i++) {
        if (readData.doubles[i] != writeData.doubles[i]) {
            doublesMatch = false;
            break;
        }
    }

    EXPECT_TRUE(valuesMatch) << "Integer values don't match";
    EXPECT_TRUE(stringMatches) << "String data doesn't match";
    EXPECT_TRUE(doublesMatch) << "Double values don't match";

    // Clean up
#ifdef _WIN32
    HANDLE h =
        OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, largeShmName.c_str());
    if (h) {
        CloseHandle(h);
    }
#else
    shm_unlink(largeShmName.c_str());
#endif
}

// Test initialization failures
TEST_F(SharedMemoryTest, InitializationFailures) {
    // Test with invalid name (on some platforms)
    const char* invalidNames[] = {"", "/invalid/name", "name/with/slashes",
                                  "name\\with\\backslashes"};

    for (const auto& invalidName : invalidNames) {
        try {
            SharedMemory<TestData> shm(invalidName, true);
            // If we get here, the name was actually valid on this platform
            SUCCEED() << "SharedMemory creation with name '" << invalidName
                      << "' succeeded on this platform";
        } catch (const SharedMemoryException& e) {
            // This is expected on platforms where the name is invalid
            EXPECT_EQ(e.getErrorCode(),
                      SharedMemoryException::ErrorCode::CREATION_FAILED)
                << "Unexpected error code for invalid name '" << invalidName
                << "'";
        }
    }
}
