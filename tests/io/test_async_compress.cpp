// filepath: d:\msys64\home\qwdma\Atom\atom\io\test_async_compress.cpp
#include "async_compress.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/software.hpp"

using namespace atom::async::io;
using namespace testing;

class AsyncCompressTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory structure
        test_dir_ = fs::temp_directory_path() / "atom_compress_test";
        input_dir_ = test_dir_ / "input";
        output_dir_ = test_dir_ / "output";

        // Clean up any existing test directories
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }

        // Create fresh directories
        fs::create_directories(input_dir_);
        fs::create_directories(output_dir_);

        // Create test files with content
        createTestFile(input_dir_ / "test1.txt", "This is test file 1 content.");
        createTestFile(input_dir_ / "test2.txt", "This is test file 2 with different content.");
        createTestFile(input_dir_ / "test3.txt", std::string(50000, 'x')); // Larger file

        // Create a subdirectory with files
        fs::create_directories(input_dir_ / "subdir");
        createTestFile(input_dir_ / "subdir" / "subfile1.txt", "Subdirectory file content.");

        // Set up io_context and work guard to keep io_context running
        work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            io_context_.get_executor());

        // Start io_context in a separate thread
        io_thread_ = std::thread([this]() {
            io_context_.run();
        });
    }

    void TearDown() override {
        // Allow io_context to finish and join thread
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }

        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
        ASSERT_TRUE(fs::exists(path)) << "Failed to create test file: " << path;
    }

    bool fileContentsEqual(const fs::path& file1, const fs::path& file2) {
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);

        if (!f1.is_open() || !f2.is_open()) {
            return false;
        }

        constexpr size_t BUFFER_SIZE = 4096;
        std::array<char, BUFFER_SIZE> buffer1, buffer2;

        while (f1 && f2) {
            f1.read(buffer1.data(), buffer1.size());
            f2.read(buffer2.data(), buffer2.size());

            if (f1.gcount() != f2.gcount()) {
                return false;
            }

            if (std::memcmp(buffer1.data(), buffer2.data(), f1.gcount()) != 0) {
                return false;
            }
        }

        return f1.eof() && f2.eof();
    }

    // Wait for an operation to complete
    void waitForCompletion(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        std::unique_lock<std::mutex> lock(completion_mutex_);
        ASSERT_TRUE(completion_cv_.wait_for(lock, timeout, [this] { return operation_completed_; }))
            << "Operation timed out";
        operation_completed_ = false;
    }

    void signalCompletion() {
        {
            std::lock_guard<std::mutex> lock(completion_mutex_);
            operation_completed_ = true;
        }
        completion_cv_.notify_one();
    }

    asio::io_context io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;

    fs::path test_dir_;
    fs::path input_dir_;
    fs::path output_dir_;

    std::mutex completion_mutex_;
    std::condition_variable completion_cv_;
    bool operation_completed_ = false;
};

// Test SingleFileCompressor functionality
TEST_F(AsyncCompressTest, SingleFileCompressorBasicOperation) {
    fs::path input_file = input_dir_ / "test1.txt";
    fs::path output_file = output_dir_ / "test1.txt.gz";

    // Create a completion handler
    auto handler = [this](const asio::error_code& ec, std::size_t bytes_transferred) {
        EXPECT_FALSE(ec) << "Error in async operation: " << ec.message();
        signalCompletion();
    };

    // Create and start the compressor
    auto compressor = std::make_shared<SingleFileCompressor>(
        io_context_, input_file, output_file);

    // Hook into the completion using a lambda that captures our handler
    compressor->start();

    // Wait for operation to complete
    waitForCompletion();

    // Verify output file exists and is not empty
    ASSERT_TRUE(fs::exists(output_file)) << "Output file was not created";
    EXPECT_GT(fs::file_size(output_file), 0) << "Output file is empty";
}

// Test DirectoryCompressor functionality
TEST_F(AsyncCompressTest, DirectoryCompressorBasicOperation) {
    fs::path output_file = output_dir_ / "all_files.gz";

    // Create a completion handler
    auto handler = [this](const asio::error_code& ec, std::size_t bytes_transferred) {
        EXPECT_FALSE(ec) << "Error in async operation: " << ec.message();
        signalCompletion();
    };

    // Create and start the compressor
    auto compressor = std::make_shared<DirectoryCompressor>(
        io_context_, input_dir_, output_file);

    // Start compression
    compressor->start();

    // Wait for operation to complete
    waitForCompletion();

    // Verify output file exists and is not empty
    ASSERT_TRUE(fs::exists(output_file)) << "Output file was not created";
    EXPECT_GT(fs::file_size(output_file), 0) << "Output file is empty";
}

// Test SingleFileDecompressor functionality
TEST_F(AsyncCompressTest, SingleFileDecompressorBasicOperation) {
    // First compress a file
    fs::path input_file = input_dir_ / "test1.txt";
    fs::path compressed_file = output_dir_ / "test1.txt.gz";

    {
        auto compressor = std::make_shared<SingleFileCompressor>(
            io_context_, input_file, compressed_file);
        compressor->start();
        waitForCompletion();
    }

    // Now decompress it
    fs::path decompressed_file = output_dir_ / "decompressed_test1.txt";

    auto decompressor = std::make_shared<SingleFileDecompressor>(
        io_context_, compressed_file, output_dir_);

    // Start decompression
    decompressor->start();

    // Wait for operation to complete
    waitForCompletion();

    // Verify decompressed file exists and content matches original
    ASSERT_TRUE(fs::exists(output_dir_ / "test1.txt"))
        << "Decompressed file was not created";

    EXPECT_TRUE(fileContentsEqual(input_file, output_dir_ / "test1.txt"))
        << "Decompressed content does not match original";
}

// Test DirectoryDecompressor functionality
TEST_F(AsyncCompressTest, DirectoryDecompressorBasicOperation) {
    // First compress the directory
    fs::path compressed_file = output_dir_ / "all_files.gz";

    {
        auto compressor = std::make_shared<DirectoryCompressor>(
            io_context_, input_dir_, compressed_file);
        compressor->start();
        waitForCompletion();
    }

    // Create a new output directory for decompressed files
    fs::path decompressed_dir = output_dir_ / "decompressed";
    fs::create_directories(decompressed_dir);

    // Now decompress it
    auto decompressor = std::make_shared<DirectoryDecompressor>(
        io_context_, output_dir_, decompressed_dir);

    // Start decompression
    decompressor->start();

    // Wait for operation to complete
    waitForCompletion();

    // Verify at least one decompressed file exists
    bool found_decompressed_file = false;
    for (const auto& entry : fs::directory_iterator(decompressed_dir)) {
        if (fs::is_regular_file(entry)) {
            found_decompressed_file = true;
            break;
        }
    }

    EXPECT_TRUE(found_decompressed_file) << "No decompressed files were created";
}

// Test error handling for input files that don't exist
TEST_F(AsyncCompressTest, CompressorErrorHandlingNonExistentFile) {
    fs::path non_existent_file = input_dir_ / "does_not_exist.txt";
    fs::path output_file = output_dir_ / "error_output.gz";

    // Expect an exception when trying to compress a non-existent file
    EXPECT_THROW({
        auto compressor = std::make_shared<SingleFileCompressor>(
            io_context_, non_existent_file, output_file);
        compressor->start();
    }, std::runtime_error);
}

// Test error handling for invalid output paths
TEST_F(AsyncCompressTest, CompressorErrorHandlingInvalidOutputPath) {
    fs::path input_file = input_dir_ / "test1.txt";
    fs::path invalid_output_file = fs::path("/non_existent_dir") / "output.gz";

    // Expect an exception when trying to write to an invalid path
    EXPECT_THROW({
        auto compressor = std::make_shared<SingleFileCompressor>(
            io_context_, input_file, invalid_output_file);
        compressor->start();
    }, std::runtime_error);
}

// Test ZIP operations
TEST_F(AsyncCompressTest, ZipOperations) {
    // Create a test ZIP file
    fs::path zip_file = output_dir_ / "test.zip";

    // We need to check if zip is available
    bool zip_available = atom::system::checkSoftwareInstalled("zip");
    if (!zip_available) {
        GTEST_SKIP() << "Skipping test as 'zip' command is not available";
    }

    // Create a ZIP file for testing using system commands
    std::string cmd = "zip -j " + zip_file.string() + " " +
                     (input_dir_ / "test1.txt").string() + " " +
                     (input_dir_ / "test2.txt").string();
    int result = std::system(cmd.c_str());
    ASSERT_EQ(result, 0) << "Failed to create test ZIP file";

    // Test ListFilesInZip
    {
        auto list_files = std::make_shared<ListFilesInZip>(io_context_, zip_file.string());
        list_files->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto file_list = list_files->getFileList();
        EXPECT_EQ(file_list.size(), 2);
        EXPECT_THAT(file_list, Contains(HasSubstr("test1.txt")));
        EXPECT_THAT(file_list, Contains(HasSubstr("test2.txt")));
    }

    // Test FileExistsInZip
    {
        auto file_exists = std::make_shared<FileExistsInZip>(
            io_context_, zip_file.string(), "test1.txt");
        file_exists->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        EXPECT_TRUE(file_exists->found());

        auto file_not_exists = std::make_shared<FileExistsInZip>(
            io_context_, zip_file.string(), "non_existent.txt");
        file_not_exists->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        EXPECT_FALSE(file_not_exists->found());
    }

    // Test GetZipFileSize
    {
        auto get_size = std::make_shared<GetZipFileSize>(io_context_, zip_file.string());
        get_size->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        EXPECT_GT(get_size->getSizeValue(), 0);
    }

    // Test RemoveFileFromZip
    {
        auto remove_file = std::make_shared<RemoveFileFromZip>(
            io_context_, zip_file.string(), "test1.txt");
        remove_file->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Check if removal was successful
        EXPECT_TRUE(remove_file->isSuccessful());

        // Verify the file is no longer in the ZIP
        auto file_exists = std::make_shared<FileExistsInZip>(
            io_context_, zip_file.string(), "test1.txt");
        file_exists->start();

        // Wait for io operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        EXPECT_FALSE(file_exists->found());
    }
}

// Test concurrent operations
TEST_F(AsyncCompressTest, ConcurrentCompression) {
    // Compress multiple files concurrently
    fs::path input_file1 = input_dir_ / "test1.txt";
    fs::path input_file2 = input_dir_ / "test2.txt";
    fs::path input_file3 = input_dir_ / "test3.txt";

    fs::path output_file1 = output_dir_ / "test1.txt.gz";
    fs::path output_file2 = output_dir_ / "test2.txt.gz";
    fs::path output_file3 = output_dir_ / "test3.txt.gz";

    // Create compressors
    auto compressor1 = std::make_shared<SingleFileCompressor>(
        io_context_, input_file1, output_file1);
    auto compressor2 = std::make_shared<SingleFileCompressor>(
        io_context_, input_file2, output_file2);
    auto compressor3 = std::make_shared<SingleFileCompressor>(
        io_context_, input_file3, output_file3);

    // Start compressions concurrently
    compressor1->start();
    compressor2->start();
    compressor3->start();

    // Wait for a reasonable amount of time for all operations to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Verify all output files exist
    EXPECT_TRUE(fs::exists(output_file1)) << "Output file 1 was not created";
    EXPECT_TRUE(fs::exists(output_file2)) << "Output file 2 was not created";
    EXPECT_TRUE(fs::exists(output_file3)) << "Output file 3 was not created";

    // Verify all output files are not empty
    EXPECT_GT(fs::file_size(output_file1), 0) << "Output file 1 is empty";
    EXPECT_GT(fs::file_size(output_file2), 0) << "Output file 2 is empty";
    EXPECT_GT(fs::file_size(output_file3), 0) << "Output file 3 is empty";
}

// Test compressing and then decompressing to verify data integrity
TEST_F(AsyncCompressTest, CompressDecompressRoundTrip) {
    // Original files
    std::vector<fs::path> input_files = {
        input_dir_ / "test1.txt",
        input_dir_ / "test2.txt",
        input_dir_ / "test3.txt"
    };

    // Create separate output directories for each file
    for (size_t i = 0; i < input_files.size(); ++i) {
        fs::path compressed_file = output_dir_ / (std::to_string(i) + ".gz");
        fs::path decomp_dir = output_dir_ / ("decomp_" + std::to_string(i));
        fs::create_directories(decomp_dir);

        // Compress
        {
            auto compressor = std::make_shared<SingleFileCompressor>(
                io_context_, input_files[i], compressed_file);
            compressor->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Decompress
        {
            auto decompressor = std::make_shared<SingleFileDecompressor>(
                io_context_, compressed_file, decomp_dir);
            decompressor->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Get the original filename
        fs::path original_name = input_files[i].filename();

        // Verify content matches
        EXPECT_TRUE(fileContentsEqual(
            input_files[i],
            decomp_dir / original_name
        )) << "Round-trip content does not match for file " << i;
    }
}

// Test compression levels and performance
TEST_F(AsyncCompressTest, CompressionPerformance) {
    // This test would typically measure and compare compression times and ratios
    // For different files or compression settings

    // Create a large test file
    fs::path large_file = input_dir_ / "large_file.txt";
    {
        std::ofstream file(large_file);
        // Create a 1MB file with semi-random content
        for (int i = 0; i < 1024; ++i) {
            file << std::string(1024, 'a' + (i % 26));
        }
    }

    fs::path output_file = output_dir_ / "large_file.gz";

    // Record start time
    auto start_time = std::chrono::high_resolution_clock::now();

    // Compress the file
    auto compressor = std::make_shared<SingleFileCompressor>(
        io_context_, large_file, output_file);
    compressor->start();

    // Wait for completion
    waitForCompletion();

    // Record end time
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Calculate compression ratio
    double original_size = static_cast<double>(fs::file_size(large_file));
    double compressed_size = static_cast<double>(fs::file_size(output_file));
    double compression_ratio = original_size / compressed_size;

    // Log performance metrics
    std::cout << "Compression time: " << duration << "ms\n";
    std::cout << "Original size: " << original_size << " bytes\n";
    std::cout << "Compressed size: " << compressed_size << " bytes\n";
    std::cout << "Compression ratio: " << compression_ratio << ":1\n";

    // Expect reasonable compression ratio for our test data
    EXPECT_GT(compression_ratio, 2.0) << "Compression ratio is lower than expected";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
