#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "atom/io/async_io.hpp"
#include "atom/io/async_compress.hpp"
#include "atom/io/async_glob.hpp"

namespace fs = std::filesystem;
using namespace testing;
using namespace atom::async::io;

class AsyncOperationsEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "atom_async_enhanced_test";
        
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        
        fs::create_directories(test_dir_);
        fs::create_directories(test_dir_ / "input");
        fs::create_directories(test_dir_ / "output");
        
        // Create test files of various sizes
        createTestFile(test_dir_ / "small.txt", "Small file content");
        createTestFile(test_dir_ / "medium.txt", std::string(10000, 'M'));
        createTestFile(test_dir_ / "large.txt", std::string(100000, 'L'));
        
        // Create files with different patterns for glob testing
        createTestFile(test_dir_ / "test1.txt", "Test file 1");
        createTestFile(test_dir_ / "test2.txt", "Test file 2");
        createTestFile(test_dir_ / "data.csv", "CSV data");
        createTestFile(test_dir_ / "config.json", "JSON config");
        
        io_context_ = std::make_unique<asio::io_context>();
        work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            io_context_->get_executor());
        
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }
    
    void TearDown() override {
        work_guard_.reset();
        io_context_->stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }
    
    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
    }
    
    template<typename T>
    bool waitForResult(std::atomic<bool>& completed, T& result, 
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(10000)) {
        auto start = std::chrono::steady_clock::now();
        while (!completed.load() && 
               std::chrono::steady_clock::now() - start < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return completed.load();
    }
    
protected:
    fs::path test_dir_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
};

// Test async file read/write with different file sizes
TEST_F(AsyncOperationsEnhancedTest, AsyncFileReadWriteDifferentSizes) {
    AsyncFile async_file(*io_context_);
    
    std::vector<std::pair<std::string, std::string>> test_cases = {
        {"small_async.txt", "Small content"},
        {"medium_async.txt", std::string(5000, 'M')},
        {"large_async.txt", std::string(50000, 'L')}
    };
    
    for (const auto& [filename, content] : test_cases) {
        fs::path file_path = test_dir_ / filename;
        
        // Test write
        std::atomic<bool> write_completed{false};
        AsyncResult<void> write_result;
        
        async_file.writeFile(file_path, content,
            [&write_completed, &write_result](AsyncResult<void> result) {
                write_result = result;
                write_completed = true;
            });
        
        ASSERT_TRUE(waitForResult(write_completed, write_result));
        EXPECT_TRUE(write_result.success) << "Write failed for " << filename;
        EXPECT_TRUE(fs::exists(file_path));
        
        // Test read
        std::atomic<bool> read_completed{false};
        AsyncResult<std::string> read_result;
        
        async_file.readFile(file_path,
            [&read_completed, &read_result](AsyncResult<std::string> result) {
                read_result = result;
                read_completed = true;
            });
        
        ASSERT_TRUE(waitForResult(read_completed, read_result));
        EXPECT_TRUE(read_result.success) << "Read failed for " << filename;
        EXPECT_EQ(read_result.value, content) << "Content mismatch for " << filename;
    }
}

// Test async file operations with error conditions
TEST_F(AsyncOperationsEnhancedTest, AsyncFileErrorHandling) {
    AsyncFile async_file(*io_context_);
    
    // Test reading non-existent file
    std::atomic<bool> read_completed{false};
    AsyncResult<std::string> read_result;
    
    async_file.readFile(test_dir_ / "nonexistent.txt",
        [&read_completed, &read_result](AsyncResult<std::string> result) {
            read_result = result;
            read_completed = true;
        });
    
    ASSERT_TRUE(waitForResult(read_completed, read_result));
    EXPECT_FALSE(read_result.success);
    EXPECT_FALSE(read_result.error_message.empty());
    
    // Test writing to invalid path (if possible to create such condition)
    std::atomic<bool> write_completed{false};
    AsyncResult<void> write_result;
    
    // Try to write to a path that should fail (e.g., to a directory)
    async_file.writeFile(test_dir_, "content",
        [&write_completed, &write_result](AsyncResult<void> result) {
            write_result = result;
            write_completed = true;
        });
    
    ASSERT_TRUE(waitForResult(write_completed, write_result));
    EXPECT_FALSE(write_result.success);
}

// Test concurrent async operations
TEST_F(AsyncOperationsEnhancedTest, ConcurrentAsyncOperations) {
    AsyncFile async_file(*io_context_);
    
    const int num_operations = 10;
    std::vector<std::atomic<bool>> completions(num_operations);
    std::vector<AsyncResult<void>> results(num_operations);
    
    // Start multiple concurrent write operations
    for (int i = 0; i < num_operations; ++i) {
        fs::path file_path = test_dir_ / ("concurrent_" + std::to_string(i) + ".txt");
        std::string content = "Content for file " + std::to_string(i);
        
        async_file.writeFile(file_path, content,
            [&completions, &results, i](AsyncResult<void> result) {
                results[i] = result;
                completions[i] = true;
            });
    }
    
    // Wait for all operations to complete
    bool all_completed = false;
    auto start = std::chrono::steady_clock::now();
    while (!all_completed && 
           std::chrono::steady_clock::now() - start < std::chrono::seconds(30)) {
        all_completed = true;
        for (const auto& completion : completions) {
            if (!completion.load()) {
                all_completed = false;
                break;
            }
        }
        if (!all_completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    ASSERT_TRUE(all_completed) << "Not all concurrent operations completed";
    
    // Verify all operations succeeded
    for (int i = 0; i < num_operations; ++i) {
        EXPECT_TRUE(results[i].success) << "Operation " << i << " failed";
        fs::path file_path = test_dir_ / ("concurrent_" + std::to_string(i) + ".txt");
        EXPECT_TRUE(fs::exists(file_path)) << "File " << i << " was not created";
    }
}

// Test async compression with progress tracking
TEST_F(AsyncOperationsEnhancedTest, AsyncCompressionWithProgress) {
    fs::path input_file = test_dir_ / "large.txt";
    fs::path output_file = test_dir_ / "large.txt.gz";
    
    std::atomic<bool> compression_completed{false};
    std::atomic<int> progress_updates{0};
    std::atomic<double> last_progress{0.0};
    
    auto compressor = std::make_shared<SingleFileCompressor>(*io_context_, input_file, output_file);
    
    // Set progress callback
    compressor->setProgressCallback([&progress_updates, &last_progress](double progress) {
        progress_updates++;
        last_progress = progress;
    });
    
    // Set completion callback
    compressor->setCompletionCallback([&compression_completed](const std::error_code& ec) {
        compression_completed = true;
    });
    
    compressor->start();
    
    // Wait for completion
    ASSERT_TRUE(waitForResult(compression_completed, compression_completed));
    EXPECT_TRUE(fs::exists(output_file));
    EXPECT_GT(progress_updates.load(), 0) << "No progress updates received";
    EXPECT_GE(last_progress.load(), 1.0) << "Final progress should be 100%";
}

// Test async glob with different patterns
TEST_F(AsyncOperationsEnhancedTest, AsyncGlobPatterns) {
    AsyncGlob async_glob(*io_context_);
    
    struct GlobTest {
        std::string pattern;
        bool recursive;
        bool dironly;
        size_t expected_min_results;
    };
    
    std::vector<GlobTest> test_cases = {
        {"*.txt", false, false, 2},  // Should find test1.txt, test2.txt
        {"test*.txt", false, false, 2},  // Should find test1.txt, test2.txt
        {"*.csv", false, false, 1},  // Should find data.csv
        {"*", false, true, 2},  // Should find input, output directories
        {"**/*.txt", true, false, 2}  // Recursive search
    };
    
    // Change to test directory for relative path testing
    fs::current_path(test_dir_);
    
    for (const auto& test_case : test_cases) {
        std::atomic<bool> glob_completed{false};
        std::vector<fs::path> glob_results;
        
        async_glob.glob(test_case.pattern,
            [&glob_completed, &glob_results](std::vector<fs::path> results) {
                glob_results = std::move(results);
                glob_completed = true;
            },
            test_case.recursive, test_case.dironly);
        
        ASSERT_TRUE(waitForResult(glob_completed, glob_results)) 
            << "Glob operation timed out for pattern: " << test_case.pattern;
        
        EXPECT_GE(glob_results.size(), test_case.expected_min_results)
            << "Insufficient results for pattern: " << test_case.pattern
            << " (got " << glob_results.size() << ", expected at least " 
            << test_case.expected_min_results << ")";
    }
}

// Test async operations with cancellation
TEST_F(AsyncOperationsEnhancedTest, AsyncOperationCancellation) {
    // Create a large file for testing cancellation
    fs::path large_file = test_dir_ / "very_large.txt";
    {
        std::ofstream file(large_file);
        for (int i = 0; i < 100000; ++i) {
            file << "This is line " << i << " of a very large file for testing cancellation.\n";
        }
    }
    
    fs::path output_file = test_dir_ / "very_large.txt.gz";
    
    auto compressor = std::make_shared<SingleFileCompressor>(*io_context_, large_file, output_file);
    
    std::atomic<bool> started{false};
    std::atomic<bool> cancelled{false};
    
    compressor->setProgressCallback([&started](double progress) {
        started = true;
    });
    
    compressor->setCompletionCallback([&cancelled](const std::error_code& ec) {
        if (ec) {
            cancelled = true;
        }
    });
    
    compressor->start();
    
    // Wait for operation to start
    auto start_time = std::chrono::steady_clock::now();
    while (!started.load() && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ASSERT_TRUE(started.load()) << "Compression did not start";
    
    // Cancel the operation
    compressor->cancel();
    
    // Wait for cancellation to take effect
    start_time = std::chrono::steady_clock::now();
    while (!cancelled.load() && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(10)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(cancelled.load()) << "Operation was not cancelled";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
