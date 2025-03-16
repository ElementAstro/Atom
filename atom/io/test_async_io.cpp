#include <gtest/gtest.h>
#include "async_io.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

class AsyncIOTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::unique_ptr<asio::io_context::work> work;
    std::thread io_thread;
    atom::async::io::AsyncFile async_file;
    atom::async::io::AsyncDirectory async_dir;
    fs::path test_dir;

    AsyncIOTest() : async_file(io_context), async_dir(io_context) {
        // Keep the io_context running
        work = std::make_unique<asio::io_context::work>(io_context);
        io_thread = std::thread([this]() { io_context.run(); });
    }

    void SetUp() override {
        // Create temporary test directory
        test_dir = fs::temp_directory_path() / "atom_async_io_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test directory
        try {
            if (fs::exists(test_dir)) {
                fs::remove_all(test_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to clean up test directory: " << e.what()
                      << std::endl;
        }
    }

    ~AsyncIOTest() override {
        // Stop the io_context and join the thread
        work.reset();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    // Helper function to create a test file with given content
    fs::path createTestFile(const std::string& name,
                            const std::string& content) {
        fs::path file_path = test_dir / name;
        std::ofstream file(file_path);
        file << content;
        file.close();
        return file_path;
    }

    // Helper function to read a file synchronously
    std::string readFileSync(const fs::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return "";
        }

        auto size = file.tellg();
        file.seekg(0);
        std::string content(static_cast<size_t>(size), '\0');
        file.read(content.data(), size);
        return content;
    }

    // Helper for running async operations with a promise/future
    template <typename T, typename Func, typename... Args>
    atom::async::io::AsyncResult<T> runAsyncOp(Func&& func, Args&&... args) {
        std::promise<atom::async::io::AsyncResult<T>> promise;
        std::future<atom::async::io::AsyncResult<T>> future =
            promise.get_future();

        std::invoke(std::forward<Func>(func), async_file,
                    std::forward<Args>(args)...,
                    [&promise](atom::async::io::AsyncResult<T> result) {
                        promise.set_value(std::move(result));
                    });

        // Wait for completion or timeout
        if (future.wait_for(std::chrono::seconds(5)) ==
            std::future_status::timeout) {
            atom::async::io::AsyncResult<T> timeout_result;
            timeout_result.success = false;
            timeout_result.error_message = "Operation timed out";
            return timeout_result;
        }

        return future.get();
    }

    // Helper for directory operations
    template <typename T, typename Func, typename... Args>
    atom::async::io::AsyncResult<T> runDirAsyncOp(Func&& func, Args&&... args) {
        std::promise<atom::async::io::AsyncResult<T>> promise;
        std::future<atom::async::io::AsyncResult<T>> future =
            promise.get_future();

        std::invoke(std::forward<Func>(func), &async_dir,
                    std::forward<Args>(args)...,
                    [&promise](atom::async::io::AsyncResult<T> result) {
                        promise.set_value(std::move(result));
                    });

        // Wait for completion or timeout
        if (future.wait_for(std::chrono::seconds(5)) ==
            std::future_status::timeout) {
            atom::async::io::AsyncResult<T> timeout_result;
            timeout_result.success = false;
            timeout_result.error_message = "Operation timed out";
            return timeout_result;
        }

        return future.get();
    }

    // Generate random content
    std::string generateRandomContent(size_t size) {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<char> dist(
            32, 126);  // ASCII printable chars

        std::string content(size, 0);
        std::generate(content.begin(), content.end(),
                      [&]() { return dist(rng); });
        return content;
    }
};

// Test async file read
TEST_F(AsyncIOTest, AsyncReadSuccess) {
    const std::string content = "Hello, world!";
    fs::path file_path = createTestFile("read_test.txt", content);

    auto result = this->runAsyncOp<std::string>(
        &atom::async::io::AsyncFile::asyncRead, file_path);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, content);
    EXPECT_TRUE(result.error_message.empty());
}

// Test async file read with non-existent file
TEST_F(AsyncIOTest, AsyncReadNonExistentFile) {
    fs::path file_path = test_dir / "non_existent.txt";

    auto result = runAsyncOp<std::string>(
        &atom::async::io::AsyncFile::asyncRead, file_path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test async file read with empty path
TEST_F(AsyncIOTest, AsyncReadEmptyPath) {
    auto result =
        runAsyncOp<std::string>(&atom::async::io::AsyncFile::asyncRead, "");

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test async file write
TEST_F(AsyncIOTest, AsyncWriteSuccess) {
    const std::string content = "Test content for writing";
    fs::path file_path = test_dir / "write_test.txt";
    std::span<const char> content_span(content.data(), content.size());

    auto result = runAsyncOp<void>(&atom::async::io::AsyncFile::asyncWrite,
                                   file_path, content_span);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(fs::exists(file_path));
    EXPECT_EQ(readFileSync(file_path), content);
}

// Test async file write with empty path
TEST_F(AsyncIOTest, AsyncWriteEmptyPath) {
    const std::string content = "Test content";
    std::span<const char> content_span(content.data(), content.size());

    auto result = runAsyncOp<void>(&atom::async::io::AsyncFile::asyncWrite, "",
                                   content_span);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test async file delete
TEST_F(AsyncIOTest, AsyncDeleteSuccess) {
    fs::path file_path = createTestFile("delete_test.txt", "Delete me");
    ASSERT_TRUE(fs::exists(file_path));

    auto result =
        runAsyncOp<void>(&atom::async::io::AsyncFile::asyncDelete, file_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_FALSE(fs::exists(file_path));
}

// Test async file delete with non-existent file
TEST_F(AsyncIOTest, AsyncDeleteNonExistentFile) {
    fs::path file_path = test_dir / "non_existent.txt";

    auto result =
        runAsyncOp<void>(&atom::async::io::AsyncFile::asyncDelete, file_path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test async file copy
TEST_F(AsyncIOTest, AsyncCopySuccess) {
    const std::string content = "Content for copying";
    fs::path src_path = createTestFile("source.txt", content);
    fs::path dest_path = test_dir / "destination.txt";

    auto result = runAsyncOp<void>(&atom::async::io::AsyncFile::asyncCopy,
                                   src_path, dest_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(fs::exists(dest_path));
    EXPECT_EQ(readFileSync(dest_path), content);
}

// Test async file copy with non-existent source
TEST_F(AsyncIOTest, AsyncCopyNonExistentSource) {
    fs::path src_path = test_dir / "non_existent.txt";
    fs::path dest_path = test_dir / "destination.txt";

    auto result = runAsyncOp<void>(&atom::async::io::AsyncFile::asyncCopy,
                                   src_path, dest_path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_FALSE(fs::exists(dest_path));
}

// Test async read with timeout
TEST_F(AsyncIOTest, AsyncReadWithTimeout) {
    const std::string content = "Content for timeout test";
    fs::path file_path = createTestFile("timeout_test.txt", content);

    std::promise<atom::async::io::AsyncResult<std::string>> promise;
    std::future<atom::async::io::AsyncResult<std::string>> future =
        promise.get_future();

    async_file.asyncReadWithTimeout(
        file_path, std::chrono::milliseconds(1000),
        [&promise](atom::async::io::AsyncResult<std::string> result) {
            promise.set_value(std::move(result));
        });

    auto result = future.get();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, content);
}

// Test async batch read
TEST_F(AsyncIOTest, AsyncBatchRead) {
    // Create multiple files
    std::vector<std::string> contents = {"Content 1", "Content 2", "Content 3"};
    std::vector<std::string> file_paths;

    for (size_t i = 0; i < contents.size(); ++i) {
        fs::path file_path =
            createTestFile("batch_" + std::to_string(i) + ".txt", contents[i]);
        file_paths.push_back(file_path.string());
    }

    std::promise<atom::async::io::AsyncResult<std::vector<std::string>>>
        promise;
    std::future<atom::async::io::AsyncResult<std::vector<std::string>>> future =
        promise.get_future();

    async_file.asyncBatchRead(
        file_paths,
        [&promise](
            atom::async::io::AsyncResult<std::vector<std::string>> result) {
            promise.set_value(std::move(result));
        });

    auto result = future.get();
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.value.size(), contents.size());

    for (size_t i = 0; i < contents.size(); ++i) {
        EXPECT_EQ(result.value[i], contents[i]);
    }
}

// Test async file status check
TEST_F(AsyncIOTest, AsyncStat) {
    fs::path file_path = createTestFile("stat_test.txt", "Status test");

    auto result = runAsyncOp<std::filesystem::file_status>(
        &atom::async::io::AsyncFile::asyncStat, file_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(fs::is_regular_file(result.value));
}

// Test async file move
TEST_F(AsyncIOTest, AsyncMove) {
    const std::string content = "Content for moving";
    fs::path src_path = createTestFile("move_source.txt", content);
    fs::path dest_path = test_dir / "move_destination.txt";

    auto result = runAsyncOp<void>(&atom::async::io::AsyncFile::asyncMove,
                                   src_path, dest_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_FALSE(fs::exists(src_path));
    EXPECT_TRUE(fs::exists(dest_path));
    EXPECT_EQ(readFileSync(dest_path), content);
}

// Test async file permission change
TEST_F(AsyncIOTest, AsyncChangePermissions) {
    fs::path file_path =
        createTestFile("permissions_test.txt", "Permission test");

    auto result = runAsyncOp<void>(
        &atom::async::io::AsyncFile::asyncChangePermissions, file_path,
        fs::perms::owner_read | fs::perms::owner_write);

    EXPECT_TRUE(result.success);

    // Check new permissions (platform-dependent)
    auto perms = fs::status(file_path).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::owner_write) != fs::perms::none);
}

// Test async directory create
TEST_F(AsyncIOTest, AsyncCreateDirectory) {
    fs::path dir_path = test_dir / "new_directory";

    auto result = runAsyncOp<void>(
        &atom::async::io::AsyncFile::asyncCreateDirectory, dir_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(fs::exists(dir_path));
    EXPECT_TRUE(fs::is_directory(dir_path));
}

// Test async file exists check
TEST_F(AsyncIOTest, AsyncExists) {
    fs::path file_path = createTestFile("exists_test.txt", "Exists test");
    fs::path non_existent = test_dir / "non_existent.txt";

    // Test with existing file
    auto result1 =
        runAsyncOp<bool>(&atom::async::io::AsyncFile::asyncExists, file_path);

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result1.value);

    // Test with non-existent file
    auto result2 = runAsyncOp<bool>(&atom::async::io::AsyncFile::asyncExists,
                                    non_existent);

    EXPECT_TRUE(result2.success);
    EXPECT_FALSE(result2.value);
}

// Test coroutine-based file read
TEST_F(AsyncIOTest, CoroutineReadFile) {
    const std::string content = "Coroutine read test";
    fs::path file_path = createTestFile("coroutine_read.txt", content);

    auto task = async_file.readFile(file_path);
    auto result = task.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, content);
}

// Test coroutine-based file write
TEST_F(AsyncIOTest, CoroutineWriteFile) {
    const std::string content = "Coroutine write test";
    std::span<const char> content_span(content.data(), content.size());
    fs::path file_path = test_dir / "coroutine_write.txt";

    auto task = async_file.writeFile(file_path, content_span);
    auto result = task.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(file_path));
    EXPECT_EQ(readFileSync(file_path), content);
}

// Test async directory operations
TEST_F(AsyncIOTest, AsyncDirectoryCreate) {
    fs::path dir_path = test_dir / "async_dir_create";

    auto result = runDirAsyncOp<void>(
        &atom::async::io::AsyncDirectory::asyncCreate, dir_path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(dir_path));
    EXPECT_TRUE(fs::is_directory(dir_path));
}

TEST_F(AsyncIOTest, AsyncDirectoryRemove) {
    fs::path dir_path = test_dir / "async_dir_remove";
    fs::create_directories(dir_path);
    ASSERT_TRUE(fs::exists(dir_path));

    auto result = runDirAsyncOp<void>(
        &atom::async::io::AsyncDirectory::asyncRemove, dir_path);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(fs::exists(dir_path));
}

TEST_F(AsyncIOTest, AsyncDirectoryListContents) {
    fs::path dir_path = test_dir / "list_contents";
    fs::create_directories(dir_path);

    // Create some files in the directory
    std::vector<fs::path> created_files;
    for (int i = 0; i < 3; ++i) {
        fs::path file_path = dir_path / ("file_" + std::to_string(i) + ".txt");
        std::ofstream file(file_path);
        file << "Content " << i;
        file.close();
        created_files.push_back(file_path);
    }

    auto result = runDirAsyncOp<std::vector<fs::path>>(
        &atom::async::io::AsyncDirectory::asyncListContents, dir_path);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.value.size(), created_files.size());

    // Convert to strings and sort for comparison (since directory iteration
    // order is unspecified)
    std::vector<std::string> result_paths;
    std::transform(result.value.begin(), result.value.end(),
                   std::back_inserter(result_paths),
                   [](const fs::path& p) { return p.string(); });

    std::vector<std::string> expected_paths;
    std::transform(created_files.begin(), created_files.end(),
                   std::back_inserter(expected_paths),
                   [](const fs::path& p) { return p.string(); });

    std::sort(result_paths.begin(), result_paths.end());
    std::sort(expected_paths.begin(), expected_paths.end());

    EXPECT_EQ(result_paths, expected_paths);
}

TEST_F(AsyncIOTest, AsyncDirectoryExists) {
    fs::path dir_path = test_dir / "exists_dir";
    fs::path non_existent = test_dir / "non_existent_dir";

    fs::create_directories(dir_path);

    // Test with existing directory
    auto result1 = runDirAsyncOp<bool>(
        &atom::async::io::AsyncDirectory::asyncExists, dir_path);

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result1.value);

    // Test with non-existent directory
    auto result2 = runDirAsyncOp<bool>(
        &atom::async::io::AsyncDirectory::asyncExists, non_existent);

    EXPECT_TRUE(result2.success);
    EXPECT_FALSE(result2.value);
}

// Test coroutine-based directory listing
TEST_F(AsyncIOTest, CoroutineListContents) {
    fs::path dir_path = test_dir / "coroutine_list";
    fs::create_directories(dir_path);

    // Create some files in the directory
    std::vector<fs::path> created_files;
    for (int i = 0; i < 3; ++i) {
        fs::path file_path = dir_path / ("file_" + std::to_string(i) + ".txt");
        std::ofstream file(file_path);
        file << "Content " << i;
        file.close();
        created_files.push_back(file_path);
    }

    auto task = async_dir.listContents(dir_path);
    auto result = task.get();

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.value.size(), created_files.size());

    // Convert to strings and sort for comparison
    std::vector<std::string> result_paths;
    std::transform(result.value.begin(), result.value.end(),
                   std::back_inserter(result_paths),
                   [](const fs::path& p) { return p.string(); });

    std::vector<std::string> expected_paths;
    std::transform(created_files.begin(), created_files.end(),
                   std::back_inserter(expected_paths),
                   [](const fs::path& p) { return p.string(); });

    std::sort(result_paths.begin(), result_paths.end());
    std::sort(expected_paths.begin(), expected_paths.end());

    EXPECT_EQ(result_paths, expected_paths);
}

// Test reading a large file
TEST_F(AsyncIOTest, AsyncReadLargeFile) {
    // Create a large file (1MB)
    const size_t size = 1024 * 1024;
    std::string large_content = generateRandomContent(size);
    fs::path file_path = test_dir / "large_file.txt";

    std::ofstream file(file_path, std::ios::binary);
    file.write(large_content.data(), large_content.size());
    file.close();

    auto result = runAsyncOp<std::string>(
        &atom::async::io::AsyncFile::asyncRead, file_path);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value.size(), size);
    EXPECT_EQ(result.value, large_content);
}

// Test concurrent operations
TEST_F(AsyncIOTest, ConcurrentOperations) {
    const int num_operations = 10;
    std::vector<fs::path> file_paths;
    std::vector<std::string> contents;

    // Create test files
    for (int i = 0; i < num_operations; ++i) {
        std::string content = "Content " + std::to_string(i);
        fs::path file_path =
            createTestFile("concurrent_" + std::to_string(i) + ".txt", content);
        file_paths.push_back(file_path);
        contents.push_back(content);
    }

    // Run concurrent read operations
    std::vector<std::future<atom::async::io::AsyncResult<std::string>>> futures;

    for (int i = 0; i < num_operations; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i,
                                                          &file_paths]() {
            std::promise<atom::async::io::AsyncResult<std::string>> promise;
            std::future<atom::async::io::AsyncResult<std::string>> future =
                promise.get_future();

            async_file.asyncRead(
                file_paths[i],
                [&promise](atom::async::io::AsyncResult<std::string> result) {
                    promise.set_value(std::move(result));
                });

            return future.get();
        }));
    }

    // Check results
    for (int i = 0; i < num_operations; ++i) {
        auto result = futures[i].get();
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.value, contents[i]);
    }
}

// Test the Task's is_ready method
TEST_F(AsyncIOTest, TaskIsReady) {
    fs::path file_path = createTestFile("is_ready_test.txt", "Ready test");

    auto task = async_file.readFile(file_path);

    // Wait a bit to ensure task completes
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(task.is_ready());
    auto result = task.get();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, "Ready test");
}

// Test behavior with invalid path format
TEST_F(AsyncIOTest, InvalidPathFormat) {
    // A path that's syntactically invalid on most systems
    const std::string invalid_path = "invalid\0path";
    auto result = runAsyncOp<std::string>(
        &atom::async::io::AsyncFile::asyncRead, invalid_path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test behavior with file paths containing special characters
TEST_F(AsyncIOTest, SpecialCharactersInPath) {
    // Create a file with special characters in the name
    const std::string filename = "special_chars_!@#$%^&().txt";
    fs::path file_path = test_dir / filename;
    const std::string content = "Special content";

    try {
        std::ofstream file(file_path);
        file << content;
        file.close();

        auto result = runAsyncOp<std::string>(
            &atom::async::io::AsyncFile::asyncRead, file_path);

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.value, content);
    } catch (const std::exception& e) {
        // Some filesystems might not support certain special characters
        GTEST_SKIP() << "Filesystem doesn't support the test filename: "
                     << e.what();
    }
}

// Test exception handling in coroutines
TEST_F(AsyncIOTest, CoroutineExceptionHandling) {
    // Invalid filename for coroutine method
    auto task = async_file.readFile("");
    auto result = task.get();

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test handling of a very long file path
TEST_F(AsyncIOTest, VeryLongFilePath) {
    // Create a very long filename (may exceed limits on some filesystems)
    std::string long_filename(100, 'a');
    long_filename += ".txt";

    fs::path file_path = test_dir / long_filename;
    const std::string content = "Long filename content";

    try {
        std::ofstream file(file_path);
        file << content;
        file.close();

        auto result = runAsyncOp<std::string>(
            &atom::async::io::AsyncFile::asyncRead, file_path);

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.value, content);
    } catch (const std::exception& e) {
        // Some filesystems might have path length restrictions
        GTEST_SKIP() << "Filesystem doesn't support the long filename: "
                     << e.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}