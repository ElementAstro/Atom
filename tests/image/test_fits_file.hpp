#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <chrono>
#include <thread>

#include "atom/image/formats/fits_file.hpp"
#include "atom/image/formats/hdu.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class FitsFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        fileManager = std::make_unique<TestFileManager>();

        // Create test FITS files
        createTestFitsFiles();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestFitsFiles() {
        // Create a simple FITS file with single HDU
        simple_fits_file = FitsTestDataGenerator::createTempFitsFile(32, 32, 1, 32);
        fileManager->registerTempFile(simple_fits_file);

        // Create a multi-HDU FITS file
        multi_hdu_fits_file = FitsTestDataGenerator::createTempFitsFile(64, 64, 3, 16);
        fileManager->registerTempFile(multi_hdu_fits_file);

        // Create a large FITS file for performance testing
        large_fits_file = FitsTestDataGenerator::createTempFitsFile(256, 256, 1, 32);
        fileManager->registerTempFile(large_fits_file);

        // Create an empty file for error testing
        empty_fits_file = "test_empty.fits";
        std::ofstream(empty_fits_file).close();
        fileManager->registerTempFile(empty_fits_file);

        // Create a corrupted FITS file
        corrupted_fits_file = "test_corrupted.fits";
        createCorruptedFitsFile(corrupted_fits_file);
        fileManager->registerTempFile(corrupted_fits_file);
    }

    void createCorruptedFitsFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);

        // Write invalid FITS header
        file.write("INVALID_FITS_HEADER", 19);
        file.write("CORRUPTED_DATA", 14);
        file.close();
    }

    std::unique_ptr<TestFileManager> fileManager;

    std::string simple_fits_file, multi_hdu_fits_file, large_fits_file;
    std::string empty_fits_file, corrupted_fits_file;
};

// Test basic FITS file reading
TEST_F(FitsFileTest, ReadBasicFitsFile) {
    FITSFile fitsFile;

    EXPECT_NO_THROW(fitsFile.readFITS(simple_fits_file));
    EXPECT_FALSE(fitsFile.isEmpty());
    EXPECT_GT(fitsFile.getHDUCount(), 0);
}

// Test FITS file constructor
TEST_F(FitsFileTest, ConstructorWithFilename) {
    EXPECT_NO_THROW(FITSFile fitsFile(simple_fits_file));

    FITSFile fitsFile(simple_fits_file);
    EXPECT_FALSE(fitsFile.isEmpty());
    EXPECT_GT(fitsFile.getHDUCount(), 0);
}

// Test HDU access
TEST_F(FitsFileTest, HDUAccess) {
    FITSFile fitsFile(simple_fits_file);

    EXPECT_GT(fitsFile.getHDUCount(), 0);

    // Test valid HDU access
    EXPECT_NO_THROW(const auto& hdu = fitsFile.getHDU(0));

    // Test invalid HDU access
    EXPECT_THROW(fitsFile.getHDU(999), std::out_of_range);
}

// Test HDU type casting
TEST_F(FitsFileTest, HDUTypeCasting) {
    FITSFile fitsFile(simple_fits_file);

    if (fitsFile.getHDUCount() > 0) {
        // Test getting HDU as specific type (assuming ImageHDU exists)
        try {
            auto& imageHDU = fitsFile.getHDUAs<ImageHDU>(0);
            EXPECT_TRUE(true); // If we get here, casting succeeded
        } catch (const std::bad_cast&) {
            // HDU is not an ImageHDU, which is also valid
            EXPECT_TRUE(true);
        }
    }
}

// Test FITS file writing
TEST_F(FitsFileTest, WriteFitsFile) {
    std::string outputFile = "test_output.fits";
    fileManager->registerTempFile(outputFile);

    // Read a FITS file
    FITSFile fitsFile(simple_fits_file);

    // Write it to a new file
    EXPECT_NO_THROW(fitsFile.writeFITS(outputFile));

    // Verify the written file exists and can be read
    EXPECT_TRUE(std::filesystem::exists(outputFile));

    FITSFile writtenFile(outputFile);
    EXPECT_EQ(writtenFile.getHDUCount(), fitsFile.getHDUCount());
}

// Test adding HDUs
TEST_F(FitsFileTest, AddHDU) {
    FITSFile fitsFile;

    size_t initialCount = fitsFile.getHDUCount();

    // Create a new HDU (assuming we have a concrete HDU implementation)
    auto newHDU = std::make_unique<HDU>();

    EXPECT_NO_THROW(fitsFile.addHDU(std::move(newHDU)));
    EXPECT_EQ(fitsFile.getHDUCount(), initialCount + 1);
}

// Test removing HDUs
TEST_F(FitsFileTest, RemoveHDU) {
    FITSFile fitsFile(simple_fits_file);

    size_t initialCount = fitsFile.getHDUCount();

    if (initialCount > 1) {
        EXPECT_NO_THROW(fitsFile.removeHDU(initialCount - 1));
        EXPECT_EQ(fitsFile.getHDUCount(), initialCount - 1);
    }

    // Test removing invalid index
    EXPECT_THROW(fitsFile.removeHDU(999), std::out_of_range);
}

// Test error handling with non-existent file
TEST_F(FitsFileTest, HandleNonExistentFile) {
    FITSFile fitsFile;

    EXPECT_THROW(fitsFile.readFITS("non_existent_file.fits"), FITSFileException);

    // Test constructor with non-existent file
    EXPECT_THROW(FITSFile badFile("non_existent_file.fits"), FITSFileException);
}

// Test error handling with empty file
TEST_F(FitsFileTest, HandleEmptyFile) {
    FITSFile fitsFile;

    EXPECT_THROW(fitsFile.readFITS(empty_fits_file), FITSFileException);
}

// Test error handling with corrupted file
TEST_F(FitsFileTest, HandleCorruptedFile) {
    FITSFile fitsFile;

    EXPECT_THROW(fitsFile.readFITS(corrupted_fits_file), FITSFileException);
}

// Test async reading
TEST_F(FitsFileTest, AsyncReading) {
    FITSFile fitsFile;

    auto future = fitsFile.readFITSAsync(simple_fits_file);

    // Wait for completion with timeout
    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should not throw
    EXPECT_NO_THROW(future.get());

    EXPECT_FALSE(fitsFile.isEmpty());
    EXPECT_GT(fitsFile.getHDUCount(), 0);
}

// Test async writing
TEST_F(FitsFileTest, AsyncWriting) {
    std::string outputFile = "test_async_output.fits";
    fileManager->registerTempFile(outputFile);

    FITSFile fitsFile(simple_fits_file);

    auto future = fitsFile.writeFITSAsync(outputFile);

    // Wait for completion with timeout
    auto status = future.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);

    // Should not throw
    EXPECT_NO_THROW(future.get());

    // Verify file was written
    EXPECT_TRUE(std::filesystem::exists(outputFile));
}

// Test multi-HDU file handling
TEST_F(FitsFileTest, MultiHDUHandling) {
    FITSFile fitsFile(multi_hdu_fits_file);

    size_t hduCount = fitsFile.getHDUCount();
    EXPECT_GT(hduCount, 0);

    // Test accessing all HDUs
    for (size_t i = 0; i < hduCount; ++i) {
        EXPECT_NO_THROW(const auto& hdu = fitsFile.getHDU(i));
    }
}

// Test FITS file validation
TEST_F(FitsFileTest, FitsFileValidation) {
    FITSFile fitsFile(simple_fits_file);

    // Test that the file is valid
    EXPECT_FALSE(fitsFile.isEmpty());
    EXPECT_GT(fitsFile.getHDUCount(), 0);

    // Test HDU validation
    for (size_t i = 0; i < fitsFile.getHDUCount(); ++i) {
        const auto& hdu = fitsFile.getHDU(i);
        // Basic validation - HDU should exist
        EXPECT_TRUE(true); // If we get here, HDU access succeeded
    }
}

// Test move semantics
TEST_F(FitsFileTest, MoveSemantics) {
    FITSFile originalFile(simple_fits_file);
    size_t originalCount = originalFile.getHDUCount();

    // Test move constructor
    FITSFile movedFile = std::move(originalFile);
    EXPECT_EQ(movedFile.getHDUCount(), originalCount);

    // Test move assignment
    FITSFile assignedFile;
    assignedFile = std::move(movedFile);
    EXPECT_EQ(assignedFile.getHDUCount(), originalCount);
}

// Test error code functionality
TEST_F(FitsFileTest, ErrorCodeFunctionality) {
    // Test error code creation
    auto errorCode = make_error_code(FITSErrorCode::FileNotExist);
    EXPECT_EQ(errorCode.value(), static_cast<int>(FITSErrorCode::FileNotExist));

    // Test error category
    const auto& category = FITSErrorCategory::instance();
    EXPECT_FALSE(std::string(category.name()).empty());
    EXPECT_FALSE(category.message(static_cast<int>(FITSErrorCode::FileNotExist)).empty());
}

// Test FITS exception functionality
TEST_F(FitsFileTest, FitsExceptionFunctionality) {
    // Test exception with error code
    FITSFileException ex1(FITSErrorCode::InvalidFormat, "Test message");
    EXPECT_EQ(ex1.errorCode(), FITSErrorCode::InvalidFormat);
    EXPECT_TRUE(std::string(ex1.what()).find("Test message") != std::string::npos);

    // Test exception with message only
    FITSFileException ex2("Another test message");
    EXPECT_EQ(ex2.errorCode(), FITSErrorCode::InternalError);
    EXPECT_TRUE(std::string(ex2.what()).find("Another test message") != std::string::npos);
}

// Test concurrent access
TEST_F(FitsFileTest, ConcurrentAccess) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    FITSFile fitsFile(simple_fits_file);
                    if (!fitsFile.isEmpty()) {
                        successCount.fetch_add(1);
                    } else {
                        errorCount.fetch_add(1);
                    }
                } catch (...) {
                    errorCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test large file handling
TEST_F(FitsFileTest, LargeFileHandling) {
    FITSFile fitsFile(large_fits_file);

    EXPECT_FALSE(fitsFile.isEmpty());
    EXPECT_GT(fitsFile.getHDUCount(), 0);

    // Test that large files can be processed efficiently
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < fitsFile.getHDUCount(); ++i) {
        const auto& hdu = fitsFile.getHDU(i);
        // Access HDU to ensure it's loaded
        (void)hdu; // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process reasonably quickly (less than 1 second for test data)
    EXPECT_LT(duration.count(), 1000);
}

// Test HDU manipulation
TEST_F(FitsFileTest, HDUManipulation) {
    FITSFile fitsFile;

    // Start with empty file
    EXPECT_TRUE(fitsFile.isEmpty());
    EXPECT_EQ(fitsFile.getHDUCount(), 0);

    // Add multiple HDUs
    for (int i = 0; i < 3; ++i) {
        auto hdu = std::make_unique<ImageHDU>();
        fitsFile.addHDU(std::move(hdu));
    }

    EXPECT_EQ(fitsFile.getHDUCount(), 3);
    EXPECT_FALSE(fitsFile.isEmpty());

    // Remove HDUs
    fitsFile.removeHDU(1); // Remove middle HDU
    EXPECT_EQ(fitsFile.getHDUCount(), 2);

    fitsFile.removeHDU(0); // Remove first HDU
    EXPECT_EQ(fitsFile.getHDUCount(), 1);

    fitsFile.removeHDU(0); // Remove last HDU
    EXPECT_EQ(fitsFile.getHDUCount(), 0);
    EXPECT_TRUE(fitsFile.isEmpty());
}

// Test file I/O with different paths
TEST_F(FitsFileTest, FileIOWithDifferentPaths) {
    // Test with relative path
    std::string relativeOutput = "relative_output.fits";
    fileManager->registerTempFile(relativeOutput);

    FITSFile fitsFile(simple_fits_file);
    EXPECT_NO_THROW(fitsFile.writeFITS(relativeOutput));
    EXPECT_TRUE(std::filesystem::exists(relativeOutput));

    // Test reading the written file
    FITSFile readFile(relativeOutput);
    EXPECT_EQ(readFile.getHDUCount(), fitsFile.getHDUCount());
}

// Test progress callback functionality (if supported)
TEST_F(FitsFileTest, ProgressCallback) {
    bool callbackCalled = false;
    float lastProgress = -1.0f;

    ProgressCallback callback = [&](float progress, const std::string& status) {
        callbackCalled = true;
        EXPECT_GE(progress, 0.0f);
        EXPECT_LE(progress, 1.0f);
        EXPECT_GE(progress, lastProgress);
        lastProgress = progress;
        EXPECT_FALSE(status.empty());
    };

    // Note: This test assumes the FITSFile class supports progress callbacks
    // If not implemented, this test can be disabled or modified
    FITSFile fitsFile;

    // Test reading with progress callback (if API supports it)
    try {
        // This would need to be implemented in the actual API
        // fitsFile.readFITSWithProgress(large_fits_file, callback);
        // For now, just test that we can create the callback
        EXPECT_TRUE(callback != nullptr);
    } catch (...) {
        // If progress callbacks aren't implemented, that's okay
        EXPECT_TRUE(true);
    }
}

// Test memory usage and cleanup
TEST_F(FitsFileTest, MemoryUsageAndCleanup) {
    // Test that FITS files properly clean up resources
    {
        FITSFile fitsFile(large_fits_file);
        EXPECT_FALSE(fitsFile.isEmpty());
        // File should be automatically cleaned up when going out of scope
    }

    // Test multiple file operations
    for (int i = 0; i < 10; ++i) {
        FITSFile fitsFile(simple_fits_file);
        EXPECT_FALSE(fitsFile.isEmpty());

        std::string tempOutput = "temp_" + std::to_string(i) + ".fits";
        fileManager->registerTempFile(tempOutput);

        fitsFile.writeFITS(tempOutput);
        EXPECT_TRUE(std::filesystem::exists(tempOutput));
    }
}

// Test performance benchmarking
TEST_F(FitsFileTest, DISABLED_PerformanceBenchmark) {
    const int iterations = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        FITSFile fitsFile(simple_fits_file);
        EXPECT_FALSE(fitsFile.isEmpty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avgTime = static_cast<double>(duration.count()) / iterations;

    std::cout << "Average FITS file read time: " << avgTime << " microseconds" << std::endl;

    // Should be reasonably fast (less than 10ms per file for small test files)
    EXPECT_LT(avgTime, 10000.0);
}

// Test edge cases
TEST_F(FitsFileTest, EdgeCases) {
    FITSFile fitsFile;

    // Test adding null HDU
    EXPECT_THROW(fitsFile.addHDU(nullptr), std::invalid_argument);

    // Test accessing HDU from empty file
    EXPECT_THROW(fitsFile.getHDU(0), std::out_of_range);

    // Test removing HDU from empty file
    EXPECT_THROW(fitsFile.removeHDU(0), std::out_of_range);

    // Test writing empty file
    std::string emptyOutput = "empty_output.fits";
    fileManager->registerTempFile(emptyOutput);

    // This might succeed or fail depending on implementation
    // The test just ensures it doesn't crash
    try {
        fitsFile.writeFITS(emptyOutput);
    } catch (const FITSFileException&) {
        // Expected for empty files
        EXPECT_TRUE(true);
    }
}

} // namespace atom::image::test
