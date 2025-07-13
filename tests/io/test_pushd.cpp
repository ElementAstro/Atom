#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "atom/io/pushd.hpp"

namespace fs = std::filesystem;

class DirectoryStackTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::unique_ptr<asio::io_context::work> work;
    std::thread io_thread;
    atom::io::DirectoryStack dir_stack;
    fs::path original_path;
    fs::path test_dir;
    std::vector<fs::path> test_subdirs;

    DirectoryStackTest() : dir_stack(io_context) {
        // Keep io_context running
        work = std::make_unique<asio::io_context::work>(io_context);
        io_thread = std::thread([this]() { io_context.run(); });
    }

    void SetUp() override {
        // Store original path to restore later
        original_path = fs::current_path();

        // Create temporary test directory structure
        test_dir = fs::temp_directory_path() / "atom_pushd_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);

        // Create subdirectories for testing
        for (int i = 0; i < 3; ++i) {
            fs::path subdir = test_dir / ("subdir_" + std::to_string(i));
            fs::create_directories(subdir);
            test_subdirs.push_back(subdir);
        }
    }

    void TearDown() override {
        // Return to original path
        fs::current_path(original_path);

        // Clean up test directory
        if (fs::exists(test_dir)) {
            try {
                fs::remove_all(test_dir);
            } catch (const std::exception& e) {
                std::cerr << "Failed to remove test directory: " << e.what()
                          << std::endl;
            }
        }

        // Reset stack
        dir_stack.clear();
    }

    ~DirectoryStackTest() override {
        // Stop io_context and join thread
        work.reset();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    // Helper to run async operation and wait for completion
    template <typename Func, typename... Args>
    std::error_code runAsyncOp(Func&& func, Args&&... args) {
        std::promise<std::error_code> promise;
        std::future<std::error_code> future = promise.get_future();

        // Call the async function with completion handler that sets the promise
        // value
        std::invoke(
            std::forward<Func>(func), &dir_stack, std::forward<Args>(args)...,
            [&promise](const std::error_code& ec) { promise.set_value(ec); });

        // Wait for completion or timeout
        if (future.wait_for(std::chrono::seconds(5)) ==
            std::future_status::timeout) {
            return std::make_error_code(std::errc::timed_out);
        }

        return future.get();
    }

    // Helper to get current directory asynchronously
    fs::path getCurrentDirAsync() {
        std::promise<fs::path> promise;
        std::future<fs::path> future = promise.get_future();

        dir_stack.asyncGetCurrentDirectory(
            [&promise](const fs::path& path) { promise.set_value(path); });

        // Wait for completion or timeout
        if (future.wait_for(std::chrono::seconds(5)) ==
            std::future_status::timeout) {
            return {};
        }

        return future.get();
    }
};

// Test basic push/pop functionality
TEST_F(DirectoryStackTest, AsyncPushdPopd) {
    // Start at test_dir
    fs::current_path(test_dir);
    ASSERT_EQ(fs::current_path(), test_dir);

    // Push to first subdir
    auto ec =
        runAsyncOp(&atom::io::DirectoryStack::asyncPushd, test_subdirs[0]);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_subdirs[0]);
    EXPECT_EQ(dir_stack.size(), 1);
    EXPECT_EQ(dir_stack.peek(), test_dir);

    // Push to second subdir
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, test_subdirs[1]);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_subdirs[1]);
    EXPECT_EQ(dir_stack.size(), 2);

    // Pop back to first subdir
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPopd);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_subdirs[0]);
    EXPECT_EQ(dir_stack.size(), 1);

    // Pop back to test_dir
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPopd);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_dir);
    EXPECT_EQ(dir_stack.size(), 0);
    EXPECT_TRUE(dir_stack.isEmpty());

    // Try to pop empty stack
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPopd);
    EXPECT_TRUE(ec);
}

// Test coroutine based push/pop
/*
TODO: Fix coroutine test
TEST_F(DirectoryStackTest, CoroutinePushdPopd) {
    // Start at test_dir
    fs::current_path(test_dir);

    auto pushd_task = [this]() -> std::optional<std::string> {
        try {
            co_await dir_stack.pushd(test_subdirs[0]);
            co_return "success";
        } catch (const std::exception& e) {
            co_return std::nullopt;
        }
    };

    auto popd_task = [this]() -> std::optional<std::string> {
        try {
            co_await dir_stack.popd();
            co_return "success";
        } catch (const std::exception& e) {
            co_return std::nullopt;
        }
    };

    // Push directory
    auto push_task = pushd_task();
    EXPECT_EQ(fs::current_path(), test_subdirs[0]);
    EXPECT_EQ(dir_stack.size(), 1);
    EXPECT_EQ(dir_stack.peek(), test_dir);

    // Pop directory
    auto pop_task = popd_task();
    EXPECT_EQ(fs::current_path(), test_dir);
    EXPECT_EQ(dir_stack.size(), 0);

    // Try popping empty stack - should throw
    bool exception_thrown = false;
    try {
        co_await dir_stack.popd();
    } catch (...) {
        exception_thrown = true;
    }
    EXPECT_TRUE(exception_thrown);
}
*/

// Test invalid paths
TEST_F(DirectoryStackTest, InvalidPaths) {
    fs::current_path(test_dir);

    // Try pushing to non-existent directory
    fs::path non_existent = test_dir / "non_existent";
    auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, non_existent);
    EXPECT_TRUE(ec);
    EXPECT_EQ(fs::current_path(), test_dir);
    EXPECT_EQ(dir_stack.size(), 0);

    // Try with empty path
    fs::path empty_path;
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, empty_path);
    EXPECT_TRUE(ec);
    EXPECT_EQ(fs::current_path(), test_dir);
    EXPECT_EQ(dir_stack.size(), 0);

    // Push valid directory first
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, test_subdirs[0]);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_subdirs[0]);

    // Corrupt the stack to contain invalid path
    dir_stack.clear();
    /*
    {
        // Need to use pushd to bypass validation
        auto corrupt_task = [this]() -> void {
            try {
                co_await dir_stack.pushd(test_subdirs[0]);
                // Now we have one entry on the stack
                // Manipulate the stack to contain invalid path
                dir_stack.remove(0);  // Remove valid entry

                // Create invalid path directly
                fs::path invalid_path = "\\\\?\\invalid:path*";

                // Force add it to stack via another pushd/swap trick
                co_await dir_stack.pushd(test_subdirs[0]);  // Add valid path
                dir_stack.swap(0, 1);  // Won't work as expected, but helps test
            } catch (...) {
                // Ignore
            }
        };
        auto task = corrupt_task();
    }
    */

    // Try to popd (should fail safely)
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncPopd);
    // May or may not error depending on platform and stack state
}

// Test basic directory management operations
TEST_F(DirectoryStackTest, DirectoryOperations) {
    fs::current_path(test_dir);

    // Push all subdirs
    for (const auto& subdir : test_subdirs) {
        auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, subdir);
        EXPECT_FALSE(ec);
    }

    // Check stack size
    EXPECT_EQ(dir_stack.size(), test_subdirs.size());
    EXPECT_FALSE(dir_stack.isEmpty());

    // Get directory stack contents
    auto dirs = dir_stack.dirs();
    EXPECT_EQ(dirs.size(), test_subdirs.size());

    // Test peek
    EXPECT_EQ(dir_stack.peek(), test_subdirs[test_subdirs.size() - 2]);

    // Test clear
    dir_stack.clear();
    EXPECT_EQ(dir_stack.size(), 0);
    EXPECT_TRUE(dir_stack.isEmpty());
}

// Test goto index functionality
TEST_F(DirectoryStackTest, GotoIndex) {
    fs::current_path(test_dir);

    // Push all subdirs
    for (const auto& subdir : test_subdirs) {
        auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, subdir);
        EXPECT_FALSE(ec);
    }

    // Current directory should be last subdir
    EXPECT_EQ(fs::current_path(), test_subdirs.back());

    // Go to second directory in stack (index 1)
    auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncGotoIndex, 1);
    EXPECT_FALSE(ec);
    EXPECT_EQ(fs::current_path(), test_subdirs[0]);

    // Go to invalid index
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncGotoIndex, 99);
    EXPECT_TRUE(ec);

    // Stack should be unchanged
    EXPECT_EQ(dir_stack.size(), test_subdirs.size());
}

// Test save and load stack
TEST_F(DirectoryStackTest, SaveLoadStack) {
    fs::current_path(test_dir);
    fs::path stack_file = test_dir / "dirs.stack";

    // Push all subdirs
    for (const auto& subdir : test_subdirs) {
        auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, subdir);
        EXPECT_FALSE(ec);
    }

    // Save the stack
    auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncSaveStackToFile,
                         stack_file.string());
    EXPECT_FALSE(ec);
    EXPECT_TRUE(fs::exists(stack_file));

    // Clear the stack
    dir_stack.clear();
    EXPECT_EQ(dir_stack.size(), 0);

    // Load the stack back
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncLoadStackFromFile,
                    stack_file.string());
    EXPECT_FALSE(ec);

    // Check if loaded correctly
    EXPECT_EQ(dir_stack.size(), test_subdirs.size());
    auto dirs = dir_stack.dirs();

    // The dirs should be in reverse order compared to how we pushed them
    for (size_t i = 0; i < dirs.size(); ++i) {
        EXPECT_EQ(dirs[i], test_subdirs[i]);
    }

    // Test with invalid file
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncLoadStackFromFile,
                    "nonexistent.stack");
    EXPECT_TRUE(ec);
}

// Test remove and swap operations
TEST_F(DirectoryStackTest, RemoveSwapOperations) {
    fs::current_path(test_dir);

    // Push all subdirs
    for (const auto& subdir : test_subdirs) {
        auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncPushd, subdir);
        EXPECT_FALSE(ec);
    }

    // Get initial stack
    auto initial_dirs = dir_stack.dirs();

    // Remove the middle directory
    dir_stack.remove(1);
    EXPECT_EQ(dir_stack.size(), test_subdirs.size() - 1);

    auto dirs_after_remove = dir_stack.dirs();
    EXPECT_EQ(dirs_after_remove[0], initial_dirs[0]);
    EXPECT_EQ(dirs_after_remove[1], initial_dirs[2]);

    // Test swap (after remove we now have [0, 2])
    dir_stack.swap(0, 1);
    auto dirs_after_swap = dir_stack.dirs();
    EXPECT_EQ(dirs_after_swap[0], initial_dirs[2]);
    EXPECT_EQ(dirs_after_swap[1], initial_dirs[0]);

    // Test invalid index operations (should be safe)
    dir_stack.remove(99);  // Out of bounds
    EXPECT_EQ(dir_stack.size(), 2);

    dir_stack.swap(0, 99);  // Out of bounds
    EXPECT_EQ(dir_stack.size(), 2);
}

// Test concurrent operations
TEST_F(DirectoryStackTest, ConcurrentOperations) {
    fs::current_path(test_dir);

    // Vector to store threads
    std::vector<std::thread> threads;

    // Thread count
    const int thread_count = 10;

    // Mutex for synchronizing output
    std::mutex output_mutex;

    // Vector to store error codes
    std::vector<std::error_code> error_codes(thread_count);

    // Barrier to start all threads at the same time
    std::atomic<bool> start_flag(false);
    std::atomic<int> ready_count(0);

    // Prepare operation threads
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            ready_count++;
            // Wait for all threads to be ready
            while (!start_flag) {
                std::this_thread::yield();
            }

            try {
                // Half threads push, half threads check dir
                if (i % 2 == 0) {
                    // Even threads push directories
                    std::error_code ec;
                    dir_stack.asyncPushd(
                        test_subdirs[i % test_subdirs.size()],
                        [&ec](const std::error_code& result_ec) {
                            ec = result_ec;
                        });

                    // Simple timeout wait for async operation
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    error_codes[i] = ec;
                } else {
                    // Odd threads get current directory
                    fs::path current;
                    dir_stack.asyncGetCurrentDirectory(
                        [&current](const fs::path& path) { current = path; });

                    // Simple timeout wait for async operation
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cerr << "Thread " << i << " exception: " << e.what()
                          << std::endl;
            }
        });
    }

    // Wait for all threads to be ready
    while (ready_count < thread_count) {
        std::this_thread::yield();
    }

    // Start all threads at once
    start_flag = true;

    // Join all threads
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Check results - not all operations will succeed due to race conditions
    // but the code shouldn't crash
    int success_count = 0;
    for (int i = 0; i < thread_count; i += 2) {  // Only check push operations
        if (!error_codes[i]) {
            success_count++;
        }
    }

    // At least some operations should succeed
    EXPECT_GT(success_count, 0);

    // Stack shouldn't be empty if any succeeded
    EXPECT_GT(dir_stack.size(), 0);
}

// Test getting current directory
TEST_F(DirectoryStackTest, GetCurrentDirectory) {
    fs::current_path(test_dir);

    // Get current directory asynchronously
    fs::path current = getCurrentDirAsync();
    EXPECT_EQ(current, test_dir);

    // Change directory manually
    fs::current_path(test_subdirs[0]);
    current = getCurrentDirAsync();
    EXPECT_EQ(current, test_subdirs[0]);

    // Change using pushd
    auto ec =
        runAsyncOp(&atom::io::DirectoryStack::asyncPushd, test_subdirs[1]);
    EXPECT_FALSE(ec);
    current = getCurrentDirAsync();
    EXPECT_EQ(current, test_subdirs[1]);
}

// Test error handling for various scenarios
TEST_F(DirectoryStackTest, ErrorHandling) {
    fs::current_path(test_dir);

    // Test saving to invalid path
    fs::path invalid_file = "/nonexistent/dir/file.stack";
    auto ec = runAsyncOp(&atom::io::DirectoryStack::asyncSaveStackToFile,
                         invalid_file.string());
    EXPECT_TRUE(ec);

    // Test with empty filename
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncSaveStackToFile, "");
    EXPECT_TRUE(ec);

    // Test loading non-existent file
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncLoadStackFromFile,
                    "nonexistent.stack");
    EXPECT_TRUE(ec);

    // Save a file with invalid path contents
    fs::path corrupt_file = test_dir / "corrupt.stack";
    {
        std::ofstream file(corrupt_file);
        file << ":/invalid:path*\n";
        file.close();
    }

    // Try loading the corrupt file
    ec = runAsyncOp(&atom::io::DirectoryStack::asyncLoadStackFromFile,
                    corrupt_file.string());
    EXPECT_TRUE(ec);
}

// Test move operations
TEST_F(DirectoryStackTest, MoveOperations) {
    fs::current_path(test_dir);

    // Push a directory to have something in the stack
    auto ec =
        runAsyncOp(&atom::io::DirectoryStack::asyncPushd, test_subdirs[0]);
    EXPECT_FALSE(ec);
    EXPECT_EQ(dir_stack.size(), 1);

    // Create new io_context for new DirectoryStack
    asio::io_context new_io_context;
    auto new_work = std::make_unique<asio::io_context::work>(new_io_context);
    std::thread new_thread([&new_io_context]() { new_io_context.run(); });

    // Move construct
    atom::io::DirectoryStack moved_stack(std::move(dir_stack));

    // Original should be empty (moved-from state)
    EXPECT_EQ(dir_stack.size(), 0);

    // Moved stack should have the entry
    EXPECT_EQ(moved_stack.size(), 1);
    EXPECT_EQ(moved_stack.peek(), test_dir);

    // Create another stack and move-assign
    atom::io::DirectoryStack another_stack(new_io_context);
    another_stack = std::move(moved_stack);

    // Moved-from stack should be empty
    EXPECT_EQ(moved_stack.size(), 0);

    // Assigned-to stack should have the entry
    EXPECT_EQ(another_stack.size(), 1);
    EXPECT_EQ(another_stack.peek(), test_dir);

    // Clean up
    new_work.reset();
    if (new_thread.joinable()) {
        new_thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
