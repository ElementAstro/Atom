#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "atom/image/formats/fits_data.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

// Mock concrete implementation of FITSData for testing
class MockFITSData : public FITSData {
public:
    MockFITSData() = default;

    void readData(std::ifstream& file, int64_t dataSize) override {
        // Mock implementation - just read the specified amount
        data_.resize(dataSize);
        file.read(reinterpret_cast<char*>(data_.data()), dataSize);
        if (file.fail() && !file.eof()) {
            throw FITSDataException(FITSDataErrorCode::DataReadError,
                                    "Failed to read data");
        }
    }

    void writeData(std::ofstream& file) const override {
        file.write(reinterpret_cast<const char*>(data_.data()), data_.size());
        if (file.fail()) {
            throw FITSDataException(FITSDataErrorCode::DataWriteError,
                                    "Failed to write data");
        }
    }

    size_t getDataSize() const override { return data_.size(); }

    DataType getDataType() const override { return DataType::BYTE; }

    void setData(const std::vector<uint8_t>& data) { data_ = data; }

    const std::vector<uint8_t>& getData() const { return data_; }

private:
    std::vector<uint8_t> data_;
};

class FitsDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        fileManager = std::make_unique<TestFileManager>();

        // Create test data files
        createTestDataFiles();
    }

    void TearDown() override { fileManager->cleanup(); }

    void createTestDataFiles() {
        // Create a binary data file
        test_data_file = "test_data.bin";
        std::ofstream file(test_data_file, std::ios::binary);

        // Write test data (256 bytes)
        for (int i = 0; i < 256; ++i) {
            uint8_t value = static_cast<uint8_t>(i);
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        file.close();
        fileManager->registerTempFile(test_data_file);

        // Create an empty data file
        empty_data_file = "empty_data.bin";
        std::ofstream(empty_data_file).close();
        fileManager->registerTempFile(empty_data_file);

        // Create a large data file
        large_data_file = "large_data.bin";
        std::ofstream largeFile(large_data_file, std::ios::binary);
        std::vector<uint8_t> largeData(10000, 0x42);
        largeFile.write(reinterpret_cast<const char*>(largeData.data()),
                        largeData.size());
        largeFile.close();
        fileManager->registerTempFile(large_data_file);
    }

    std::unique_ptr<TestFileManager> fileManager;
    std::string test_data_file, empty_data_file, large_data_file;
};

// Test basic data reading
TEST_F(FitsDataTest, BasicDataReading) {
    MockFITSData fitsData;

    std::ifstream file(test_data_file, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    EXPECT_NO_THROW(fitsData.readData(file, 256));
    EXPECT_EQ(fitsData.getDataSize(), 256);

    const auto& data = fitsData.getData();
    EXPECT_EQ(data.size(), 256);

    // Verify data content
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(data[i], static_cast<uint8_t>(i));
    }
}

// Test data writing
TEST_F(FitsDataTest, DataWriting) {
    MockFITSData fitsData;

    // Set test data
    std::vector<uint8_t> testData = {0x01, 0x02, 0x03, 0x04, 0x05};
    fitsData.setData(testData);

    std::string outputFile = "test_output.bin";
    fileManager->registerTempFile(outputFile);

    std::ofstream file(outputFile, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    EXPECT_NO_THROW(fitsData.writeData(file));
    file.close();

    // Verify written data
    std::ifstream readFile(outputFile, std::ios::binary);
    std::vector<uint8_t> readData(testData.size());
    readFile.read(reinterpret_cast<char*>(readData.data()), readData.size());

    EXPECT_EQ(readData, testData);
}

// Test data type enumeration
TEST_F(FitsDataTest, DataTypeEnumeration) {
    // Test all data types
    std::vector<DataType> dataTypes = {DataType::BYTE,  DataType::SHORT,
                                       DataType::INT,   DataType::LONG,
                                       DataType::FLOAT, DataType::DOUBLE};

    for (const auto& dataType : dataTypes) {
        // Test that data types can be assigned and compared
        DataType testType = dataType;
        EXPECT_EQ(testType, dataType);
    }
}

// Test error handling with invalid file
TEST_F(FitsDataTest, ErrorHandlingInvalidFile) {
    MockFITSData fitsData;

    std::ifstream invalidFile("non_existent_file.bin", std::ios::binary);
    EXPECT_FALSE(invalidFile.is_open());

    // Should handle invalid file gracefully
    EXPECT_THROW(fitsData.readData(invalidFile, 100), FITSDataException);
}

// Test error handling with empty file
TEST_F(FitsDataTest, ErrorHandlingEmptyFile) {
    MockFITSData fitsData;

    std::ifstream file(empty_data_file, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    // Reading from empty file should handle gracefully
    EXPECT_NO_THROW(fitsData.readData(file, 0));
    EXPECT_EQ(fitsData.getDataSize(), 0);
}

// Test large data handling
TEST_F(FitsDataTest, LargeDataHandling) {
    MockFITSData fitsData;

    std::ifstream file(large_data_file, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    EXPECT_NO_THROW(fitsData.readData(file, 10000));
    EXPECT_EQ(fitsData.getDataSize(), 10000);

    const auto& data = fitsData.getData();
    EXPECT_EQ(data.size(), 10000);

    // Verify all data is correct
    for (const auto& byte : data) {
        EXPECT_EQ(byte, 0x42);
    }
}

// Test FITS data exception functionality
TEST_F(FitsDataTest, FitsDataExceptionFunctionality) {
    // Test exception with error code
    FITSDataException ex1(FITSDataErrorCode::InvalidDataType, "Test message");
    EXPECT_EQ(ex1.errorCode(), FITSDataErrorCode::InvalidDataType);
    EXPECT_TRUE(std::string(ex1.what()).find("Test message") !=
                std::string::npos);

    // Test exception with message only
    FITSDataException ex2("Another test message");
    EXPECT_EQ(ex2.errorCode(), FITSDataErrorCode::InternalError);
    EXPECT_TRUE(std::string(ex2.what()).find("Another test message") !=
                std::string::npos);
}

// Test error code functionality
TEST_F(FitsDataTest, ErrorCodeFunctionality) {
    // Test error code creation
    auto errorCode = make_error_code(FITSDataErrorCode::InvalidDataSize);
    EXPECT_EQ(errorCode.value(),
              static_cast<int>(FITSDataErrorCode::InvalidDataSize));

    // Test different error codes
    std::vector<FITSDataErrorCode> errorCodes = {
        FITSDataErrorCode::Success,
        FITSDataErrorCode::InvalidDataType,
        FITSDataErrorCode::InvalidDataSize,
        FITSDataErrorCode::StreamError,
        FITSDataErrorCode::DataReadError,
        FITSDataErrorCode::DataWriteError,
        FITSDataErrorCode::InvalidOperation,
        FITSDataErrorCode::CompressionError,
        FITSDataErrorCode::DataValidationError,
        FITSDataErrorCode::MemoryAllocationError,
        FITSDataErrorCode::InternalError};

    for (const auto& code : errorCodes) {
        auto ec = make_error_code(code);
        EXPECT_EQ(ec.value(), static_cast<int>(code));
    }
}

// Test FitsNumericType concept
TEST_F(FitsDataTest, FitsNumericTypeConcept) {
    // Test that the concept works with valid types
    static_assert(FitsNumericType<uint8_t>);
    static_assert(FitsNumericType<int16_t>);
    static_assert(FitsNumericType<int32_t>);
    static_assert(FitsNumericType<int64_t>);
    static_assert(FitsNumericType<float>);
    static_assert(FitsNumericType<double>);

    // Test that the concept rejects invalid types
    static_assert(!FitsNumericType<std::string>);
    static_assert(!FitsNumericType<char>);
    static_assert(!FitsNumericType<bool>);

    EXPECT_TRUE(true);  // If we get here, static_assert tests passed
}

// Test data size validation
TEST_F(FitsDataTest, DataSizeValidation) {
    MockFITSData fitsData;

    std::ifstream file(test_data_file, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    // Test reading exact size
    EXPECT_NO_THROW(fitsData.readData(file, 256));
    EXPECT_EQ(fitsData.getDataSize(), 256);

    // Reset file position
    file.clear();
    file.seekg(0);

    // Test reading partial data
    MockFITSData partialData;
    EXPECT_NO_THROW(partialData.readData(file, 100));
    EXPECT_EQ(partialData.getDataSize(), 100);
}

// Test concurrent data access
TEST_F(FitsDataTest, ConcurrentDataAccess) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    MockFITSData fitsData;
                    std::ifstream file(test_data_file, std::ios::binary);

                    if (file.is_open()) {
                        fitsData.readData(file, 256);
                        if (fitsData.getDataSize() == 256) {
                            successCount.fetch_add(1);
                        } else {
                            errorCount.fetch_add(1);
                        }
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

}  // namespace atom::image::test
