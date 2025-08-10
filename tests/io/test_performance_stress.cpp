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
#include <random>
#include <algorithm>
#include <iomanip>

#include "atom/io/io.hpp"
#include "atom/io/async_io.hpp"
#include "atom/io/async_compress.hpp"
#include "atom/io/file_info.hpp"
#include "atom/io/glob.hpp"

namespace fs = std::filesystem;
using namespace testing;

class PerformanceStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "atom_performance_test";
        
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        
        fs::create_directories(test_dir_);
        fs::create_directories(test_dir_ / "stress_test");
        
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
    
    void createTestFile(const fs::path& path, size_t size_bytes) {
        std::ofstream file(path, std::ios::binary);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        const size_t buffer_size = 8192;
        std::vector<char> buffer(buffer_size);
        
        size_t remaining = size_bytes;
        while (remaining > 0) {
            size_t to_write = std::min(remaining, buffer_size);
            for (size_t i = 0; i < to_write; ++i) {
                buffer[i] = static_cast<char>(dis(gen));
            }
            file.write(buffer.data(), to_write);
            remaining -= to_write;
        }
    }
    
    struct PerformanceMetrics {
        std::chrono::milliseconds duration;
        size_t operations_count;
        size_t bytes_processed;
        double operations_per_second;
        double mbytes_per_second;
    };
    
    PerformanceMetrics measurePerformance(std::function<void()> operation, 
                                        size_t operations_count = 1, 
                                        size_t bytes_processed = 0) {
        auto start = std::chrono::high_resolution_clock::now();
        operation();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        double seconds = duration.count() / 1000.0;
        
        PerformanceMetrics metrics;
        metrics.duration = duration;
        metrics.operations_count = operations_count;
        metrics.bytes_processed = bytes_processed;
        metrics.operations_per_second = operations_count / seconds;
        metrics.mbytes_per_second = (bytes_processed / (1024.0 * 1024.0)) / seconds;
        
        return metrics;
    }
    
protected:
    fs::path test_dir_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
};

// Test file copy performance with different file sizes
TEST_F(PerformanceStressTest, FileCopyPerformance) {
    std::vector<size_t> file_sizes = {
        1024,           // 1 KB
        1024 * 1024,    // 1 MB
        10 * 1024 * 1024, // 10 MB
        50 * 1024 * 1024  // 50 MB
    };
    
    for (size_t size : file_sizes) {
        fs::path source = test_dir_ / ("source_" + std::to_string(size) + ".dat");
        fs::path dest = test_dir_ / ("dest_" + std::to_string(size) + ".dat");
        
        // Create test file
        createTestFile(source, size);
        ASSERT_TRUE(fs::exists(source));
        ASSERT_EQ(fs::file_size(source), size);
        
        // Measure copy performance
        auto metrics = measurePerformance([&]() {
            EXPECT_TRUE(atom::io::copyFile(source, dest));
        }, 1, size);
        
        EXPECT_TRUE(fs::exists(dest));
        EXPECT_EQ(fs::file_size(dest), size);
        
        std::cout << "File copy (" << size / (1024 * 1024) << " MB): "
                  << metrics.duration.count() << "ms, "
                  << std::fixed << std::setprecision(2) 
                  << metrics.mbytes_per_second << " MB/s" << std::endl;
        
        // Performance expectations (adjust based on system capabilities)
        if (size >= 1024 * 1024) { // For files >= 1MB
            EXPECT_GT(metrics.mbytes_per_second, 10.0) 
                << "Copy performance too slow for " << size << " byte file";
        }
    }
}

// Test concurrent file operations stress
TEST_F(PerformanceStressTest, ConcurrentFileOperationsStress) {
    const int num_threads = 8;
    const int operations_per_thread = 50;
    const size_t file_size = 1024; // 1KB files for fast operations
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    std::atomic<int> failed_operations{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, file_size, 
                             &successful_operations, &failed_operations]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    fs::path file_path = test_dir_ / "stress_test" / 
                        ("thread_" + std::to_string(t) + "_file_" + std::to_string(i) + ".dat");
                    
                    // Create file
                    createTestFile(file_path, file_size);
                    
                    // Copy file
                    fs::path copy_path = test_dir_ / "stress_test" / 
                        ("thread_" + std::to_string(t) + "_copy_" + std::to_string(i) + ".dat");
                    
                    if (atom::io::copyFile(file_path, copy_path)) {
                        successful_operations++;
                    } else {
                        failed_operations++;
                    }
                    
                    // Remove original
                    atom::io::removeFile(file_path);
                    
                } catch (const std::exception& e) {
                    failed_operations++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    int total_operations = num_threads * operations_per_thread;
    double ops_per_second = (total_operations * 1000.0) / duration.count();
    
    std::cout << "Concurrent stress test: " << total_operations << " operations in "
              << duration.count() << "ms (" << std::fixed << std::setprecision(2)
              << ops_per_second << " ops/sec)" << std::endl;
    std::cout << "Successful: " << successful_operations.load() 
              << ", Failed: " << failed_operations.load() << std::endl;
    
    EXPECT_EQ(successful_operations.load(), total_operations) 
        << "Some operations failed during stress test";
    EXPECT_EQ(failed_operations.load(), 0) << "Unexpected failures occurred";
}

// Test async I/O performance
TEST_F(PerformanceStressTest, AsyncIOPerformance) {
    atom::async::io::AsyncFile async_file(*io_context_);
    
    const int num_files = 100;
    const size_t file_size = 10 * 1024; // 10KB per file
    
    std::vector<std::atomic<bool>> completions(num_files);
    std::vector<atom::async::io::AsyncResult<void>> results(num_files);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Start all async write operations
    for (int i = 0; i < num_files; ++i) {
        fs::path file_path = test_dir_ / ("async_" + std::to_string(i) + ".dat");
        std::string content(file_size, 'A' + (i % 26));
        
        async_file.writeFile(file_path, content,
            [&completions, &results, i](atom::async::io::AsyncResult<void> result) {
                results[i] = result;
                completions[i] = true;
            });
    }
    
    // Wait for all operations to complete
    bool all_completed = false;
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    
    while (!all_completed && std::chrono::steady_clock::now() < timeout) {
        all_completed = true;
        for (const auto& completion : completions) {
            if (!completion.load()) {
                all_completed = false;
                break;
            }
        }
        if (!all_completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ASSERT_TRUE(all_completed) << "Not all async operations completed within timeout";
    
    // Verify all operations succeeded
    int successful = 0;
    for (const auto& result : results) {
        if (result.success) {
            successful++;
        }
    }
    
    double ops_per_second = (num_files * 1000.0) / duration.count();
    double total_mb = (num_files * file_size) / (1024.0 * 1024.0);
    double mb_per_second = total_mb * 1000.0 / duration.count();
    
    std::cout << "Async I/O performance: " << num_files << " files (" 
              << std::fixed << std::setprecision(2) << total_mb << " MB) in "
              << duration.count() << "ms" << std::endl;
    std::cout << "Throughput: " << ops_per_second << " ops/sec, "
              << mb_per_second << " MB/sec" << std::endl;
    
    EXPECT_EQ(successful, num_files) << "Some async operations failed";
    EXPECT_GT(ops_per_second, 50.0) << "Async I/O performance too slow";
}

// Test glob performance with many files
TEST_F(PerformanceStressTest, GlobPerformanceWithManyFiles) {
    const int num_files = 1000;
    const int num_dirs = 10;
    
    // Create directory structure with many files
    for (int d = 0; d < num_dirs; ++d) {
        fs::path dir = test_dir_ / ("dir_" + std::to_string(d));
        fs::create_directories(dir);
        
        for (int f = 0; f < num_files / num_dirs; ++f) {
            fs::path file = dir / ("file_" + std::to_string(f) + ".txt");
            std::ofstream(file) << "Content " << f;
        }
    }
    
    // Change to test directory
    fs::current_path(test_dir_);
    
    // Test different glob patterns
    std::vector<std::pair<std::string, bool>> patterns = {
        {"*.txt", false},           // Non-recursive
        {"**/*.txt", true},         // Recursive
        {"dir_*/*.txt", false},     // Pattern with directory
        {"**/file_1*.txt", true}    // Complex recursive pattern
    };
    
    for (const auto& [pattern, recursive] : patterns) {
        auto metrics = measurePerformance([&]() {
            auto results = atom::io::glob(pattern, recursive);
            EXPECT_GT(results.size(), 0) << "No results for pattern: " << pattern;
        });
        
        std::cout << "Glob pattern '" << pattern << "' (recursive=" << recursive 
                  << "): " << metrics.duration.count() << "ms" << std::endl;
        
        // Performance expectation: should complete within reasonable time
        EXPECT_LT(metrics.duration.count(), 5000) 
            << "Glob operation too slow for pattern: " << pattern;
    }
}

// Test memory usage during large file operations
TEST_F(PerformanceStressTest, MemoryUsageDuringLargeOperations) {
    // This test is more observational - it creates large files and operations
    // to ensure memory usage remains reasonable
    
    const size_t large_file_size = 100 * 1024 * 1024; // 100 MB
    fs::path large_file = test_dir_ / "large_memory_test.dat";
    
    // Create large file
    createTestFile(large_file, large_file_size);
    ASSERT_TRUE(fs::exists(large_file));
    
    // Test file info retrieval (should not load entire file into memory)
    auto metrics = measurePerformance([&]() {
        auto info = atom::io::getFileInfo(large_file);
        EXPECT_EQ(info.fileSize, large_file_size);
    });
    
    std::cout << "File info for 100MB file: " << metrics.duration.count() << "ms" << std::endl;
    
    // Should be very fast since it doesn't read file content
    EXPECT_LT(metrics.duration.count(), 100) 
        << "File info retrieval too slow for large file";
    
    // Test file copy (this will use memory but should be efficient)
    fs::path copy_file = test_dir_ / "large_memory_test_copy.dat";
    
    auto copy_metrics = measurePerformance([&]() {
        EXPECT_TRUE(atom::io::copyFile(large_file, copy_file));
    }, 1, large_file_size);
    
    std::cout << "Copy 100MB file: " << copy_metrics.duration.count() << "ms, "
              << std::fixed << std::setprecision(2) 
              << copy_metrics.mbytes_per_second << " MB/s" << std::endl;
    
    EXPECT_TRUE(fs::exists(copy_file));
    EXPECT_EQ(fs::file_size(copy_file), large_file_size);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
