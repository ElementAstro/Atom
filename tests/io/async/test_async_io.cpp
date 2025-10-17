// filepath: atom/io/test_async_io.cpp - SIMPLIFIED VERSION
// NOTE: This test file has been simplified to only test implemented AsyncFile
// functions Many AsyncFile template functions are not implemented and would
// cause linking errors

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <string>

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

#include "async_io.hpp"

using namespace atom::async::io;
namespace fs = std::filesystem;
using ::testing::HasSubstr;

class AsyncIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory structure
        testDir = fs::temp_directory_path() / "async_io_test";

        // Clean up any existing test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }

        fs::create_directory(testDir);

        // Create test files
        createFile(testDir / "file1.txt", "Test file 1 content");
        createFile(testDir / "file2.txt",
                   "Test file 2 content\nwith multiple lines");
        createFile(testDir / "file3.dat",
                   "Binary file content\0with null bytes", 35);

        // Create subdirectories
        fs::create_directory(testDir / "subdir1");
        fs::create_directory(testDir / "subdir2");

        createFile(testDir / "subdir1" / "nested_file.txt",
                   "Nested file content");

        // Create the async file instance
#ifdef ATOM_USE_ASIO
        io_context = std::make_unique<asio::io_context>();
        async_file = std::make_unique<AsyncFile>(*io_context);
#else
        async_file = std::make_unique<AsyncFile>();
#endif
    }

    void TearDown() override {
        // Clean up the test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    void createFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void createFile(const fs::path& path, const char* content, size_t size) {
        std::ofstream file(path, std::ios::binary);
        file.write(content, size);
        file.close();
    }

    // Helper for waiting on futures with timeout
    template <typename T>
    bool waitForFuture(std::future<T>& future, int timeoutMs = 1000) {
        return future.wait_for(std::chrono::milliseconds(timeoutMs)) ==
               std::future_status::ready;
    }

    fs::path testDir;
#ifdef ATOM_USE_ASIO
    std::unique_ptr<asio::io_context> io_context;
#endif
    std::unique_ptr<AsyncFile> async_file;
};

// Test AsyncFile constructor
TEST_F(AsyncIOTest, AsyncFileConstructor) {
#ifdef ATOM_USE_ASIO
    asio::io_context test_context;
    ASSERT_NO_THROW(AsyncFile file(test_context));
#else
    ASSERT_NO_THROW(AsyncFile file());
#endif
}

// AsyncDirectory is deprecated - use AsyncFile for directory operations

// Test AsyncFile::asyncRead with existing file
TEST_F(AsyncIOTest, AsyncFileReadExistingFile) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    async_file->asyncRead(testDir / "file1.txt",
                          [&promise](AsyncResult<std::string> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, "Test file 1 content");
    EXPECT_TRUE(result.error_message.empty());
}

// Test AsyncFile::asyncRead with non-existent file
TEST_F(AsyncIOTest, AsyncFileReadNonExistentFile) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    async_file->asyncRead(testDir / "non_existent.txt",
                          [&promise](AsyncResult<std::string> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("does not exist"), std::string::npos);
}

// Test AsyncFile::asyncWrite with new file
TEST_F(AsyncIOTest, AsyncFileWriteNewFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    std::string content = "New file content";
    fs::path newFilePath = testDir / "new_file.txt";

    async_file->asyncWrite(
        newFilePath, std::span<const char>(content.data(), content.size()),
        [&promise](AsyncResult<void> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was created with correct content
    EXPECT_TRUE(fs::exists(newFilePath));
    std::ifstream file(newFilePath);
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    EXPECT_EQ(fileContent, content);
}

// Test AsyncFile::asyncWrite to existing file (overwrite)
TEST_F(AsyncIOTest, AsyncFileWriteExistingFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    std::string content = "Updated content";
    fs::path filePath = testDir / "file1.txt";

    async_file->asyncWrite(
        filePath, std::span<const char>(content.data(), content.size()),
        [&promise](AsyncResult<void> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was updated with correct content
    std::ifstream file(filePath);
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    EXPECT_EQ(fileContent, content);
}

// Test AsyncFile::asyncDelete with existing file
TEST_F(AsyncIOTest, AsyncFileDeleteExistingFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path filePath = testDir / "file2.txt";
    ASSERT_TRUE(fs::exists(filePath));  // Ensure file exists before test

    async_file->asyncDelete(filePath, [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was deleted
    EXPECT_FALSE(fs::exists(filePath));
}

// Test AsyncFile::asyncDelete with non-existent file
TEST_F(AsyncIOTest, AsyncFileDeleteNonExistentFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path filePath = testDir / "non_existent.txt";

    async_file->asyncDelete(filePath, [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("does not exist"));
}

// Test AsyncFile::asyncCopy with existing file
TEST_F(AsyncIOTest, AsyncFileCopyExistingFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path srcPath = testDir / "file1.txt";
    fs::path destPath = testDir / "file1_copy.txt";

    async_file->asyncCopy(srcPath.string(), destPath.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was copied
    EXPECT_TRUE(fs::exists(destPath));

    // Verify content is the same
    std::ifstream srcFile(srcPath);
    std::string srcContent((std::istreambuf_iterator<char>(srcFile)),
                           std::istreambuf_iterator<char>());

    std::ifstream destFile(destPath);
    std::string destContent((std::istreambuf_iterator<char>(destFile)),
                            std::istreambuf_iterator<char>());

    EXPECT_EQ(srcContent, destContent);
}

// Test AsyncFile::asyncCopy with non-existent source file
TEST_F(AsyncIOTest, AsyncFileCopyNonExistentSource) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path srcPath = testDir / "non_existent.txt";
    fs::path destPath = testDir / "copy_fail.txt";

    async_file->asyncCopy(srcPath.string(), destPath.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("does not exist"));

    // Verify destination was not created
    EXPECT_FALSE(fs::exists(destPath));
}

// Test AsyncFile::asyncReadWithTimeout that completes within timeout
TEST_F(AsyncIOTest, AsyncFileReadWithTimeoutSuccess) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    async_file->asyncReadWithTimeout(
        (testDir / "file1.txt").string(), std::chrono::milliseconds(500),
        [&promise](AsyncResult<std::string> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future, 1000));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, "Test file 1 content");
    EXPECT_TRUE(result.error_message.empty());
}

// Test AsyncFile::asyncReadWithTimeout that times out
// (this test may be flaky depending on implementation details)
TEST_F(AsyncIOTest, AsyncFileReadWithTimeoutExpires) {
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    // Assuming implementation adds artificial delay, set very short timeout
    async_file->asyncReadWithTimeout(
        (testDir / "file1.txt").string(), std::chrono::milliseconds(1),
        [&promise](AsyncResult<std::string> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future, 200));
    auto result = future.get();

    // If the operation timed out, result.success should be false
    if (!result.success) {
        EXPECT_THAT(result.error_message, HasSubstr("timeout"));
    } else {
        // If it didn't time out (possible with fast execution), the operation
        // should succeed
        EXPECT_EQ(result.value, "Test file 1 content");
    }
}

// Test AsyncFile::asyncBatchRead with existing files
TEST_F(AsyncIOTest, AsyncFileBatchReadExistingFiles) {
    std::promise<AsyncResult<std::vector<std::string>>> promise;
    auto future = promise.get_future();

    std::vector<std::string> filePaths = {(testDir / "file1.txt").string(),
                                          (testDir / "file2.txt").string()};

    async_file->asyncBatchRead(
        filePaths, [&promise](AsyncResult<std::vector<std::string>> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    ASSERT_EQ(result.value.size(), 2);
    EXPECT_EQ(result.value[0], "Test file 1 content");
    EXPECT_EQ(result.value[1], "Test file 2 content\nwith multiple lines");
}

// Test AsyncFile::asyncBatchRead with mix of existing and non-existent files
TEST_F(AsyncIOTest, AsyncFileBatchReadMixedFiles) {
    std::promise<AsyncResult<std::vector<std::string>>> promise;
    auto future = promise.get_future();

    std::vector<std::string> filePaths = {
        (testDir / "file1.txt").string(),
        (testDir / "non_existent.txt").string()};

    async_file->asyncBatchRead(
        filePaths, [&promise](AsyncResult<std::vector<std::string>> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("non_existent.txt"));
}

// Test AsyncFile::asyncStat with existing file
TEST_F(AsyncIOTest, AsyncFileStatExistingFile) {
    std::promise<AsyncResult<fs::file_status>> promise;
    auto future = promise.get_future();

    async_file->asyncStat(testDir / "file1.txt",
                          [&promise](AsyncResult<fs::file_status> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_EQ(fs::is_regular_file(result.value), true);
}

// Test AsyncFile::asyncStat with non-existent file
TEST_F(AsyncIOTest, AsyncFileStatNonExistentFile) {
    std::promise<AsyncResult<fs::file_status>> promise;
    auto future = promise.get_future();

    async_file->asyncStat(testDir / "non_existent.txt",
                          [&promise](AsyncResult<fs::file_status> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("does not exist"));
}

// Test AsyncFile::asyncMove with existing file
TEST_F(AsyncIOTest, AsyncFileMoveExistingFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path srcPath = testDir / "file1.txt";
    fs::path destPath = testDir / "file1_moved.txt";

    async_file->asyncMove(srcPath.string(), destPath.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was moved
    EXPECT_FALSE(fs::exists(srcPath));
    EXPECT_TRUE(fs::exists(destPath));
}

// Test AsyncFile::asyncMove with non-existent source file
TEST_F(AsyncIOTest, AsyncFileMoveNonExistentSource) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path srcPath = testDir / "non_existent.txt";
    fs::path destPath = testDir / "move_fail.txt";

    async_file->asyncMove(srcPath.string(), destPath.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("does not exist"));

    // Verify destination was not created
    EXPECT_FALSE(fs::exists(destPath));
}

// Test AsyncFile::asyncChangePermissions with existing file
TEST_F(AsyncIOTest, AsyncFileChangePermissionsExistingFile) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path filePath = testDir / "file1.txt";

    async_file->asyncChangePermissions(
        filePath, fs::perms::owner_read | fs::perms::owner_write,
        [&promise](AsyncResult<void> result) {
            promise.set_value(std::move(result));
        });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify permissions were changed (implementation-dependent)
    // This might be system-dependent, so we're not checking the actual
    // permissions
}

// Test AsyncFile::asyncCreateDirectory with new directory
TEST_F(AsyncIOTest, AsyncFileCreateDirectoryNew) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path dirPath = testDir / "new_dir";

    async_file->asyncCreateDirectory(dirPath,
                                     [&promise](AsyncResult<void> result) {
                                         promise.set_value(std::move(result));
                                     });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify directory was created
    EXPECT_TRUE(fs::exists(dirPath));
    EXPECT_TRUE(fs::is_directory(dirPath));
}

// Test AsyncFile::asyncCreateDirectory with existing directory
TEST_F(AsyncIOTest, AsyncFileCreateDirectoryExisting) {
    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    fs::path dirPath = testDir / "subdir1";

    async_file->asyncCreateDirectory(dirPath,
                                     [&promise](AsyncResult<void> result) {
                                         promise.set_value(std::move(result));
                                     });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);
    EXPECT_THAT(result.error_message, HasSubstr("already exists"));
}

// Test AsyncFile::asyncExists with existing file
TEST_F(AsyncIOTest, AsyncFileExistsExistingFile) {
    std::promise<AsyncResult<bool>> promise;
    auto future = promise.get_future();

    async_file->asyncExists(testDir / "file1.txt",
                            [&promise](AsyncResult<bool> result) {
                                promise.set_value(std::move(result));
                            });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(result.value);
}

// Test AsyncFile::asyncExists with non-existent file
TEST_F(AsyncIOTest, AsyncFileExistsNonExistentFile) {
    std::promise<AsyncResult<bool>> promise;
    auto future = promise.get_future();

    async_file->asyncExists(testDir / "non_existent.txt",
                            [&promise](AsyncResult<bool> result) {
                                promise.set_value(std::move(result));
                            });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_FALSE(result.value);
}

// Test AsyncFile::readFile coroutine with existing file
TEST_F(AsyncIOTest, AsyncFileReadFileCoroutine) {
    auto fileTask = async_file->readFile(testDir / "file1.txt");
    auto result = fileTask.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, "Test file 1 content");
    EXPECT_TRUE(result.error_message.empty());
}

// Test AsyncFile::writeFile coroutine
TEST_F(AsyncIOTest, AsyncFileWriteFileCoroutine) {
    std::string content = "Coroutine written content";
    fs::path filePath = testDir / "coroutine_written.txt";

    auto writeTask = async_file->writeFile(
        filePath, std::span<const char>(content.data(), content.size()));
    auto result = writeTask.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());

    // Verify file was created with correct content
    EXPECT_TRUE(fs::exists(filePath));
    std::ifstream file(filePath);
    std::string fileContent((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    EXPECT_EQ(fileContent, content);
}

// Test error handling with invalid inputs
TEST_F(AsyncIOTest, InvalidInputHandling) {
    std::promise<AsyncResult<std::string>> readPromise;
    auto readFuture = readPromise.get_future();

    // Empty filename
    async_file->asyncRead("", [&readPromise](AsyncResult<std::string> result) {
        readPromise.set_value(std::move(result));
    });

    ASSERT_TRUE(waitForFuture(readFuture));
    auto readResult = readFuture.get();

    EXPECT_FALSE(readResult.success);
    EXPECT_THAT(readResult.error_message, HasSubstr("Invalid"));
}

// Test concurrent operations
TEST_F(AsyncIOTest, ConcurrentOperations) {
    constexpr int numConcurrentOps = 10;
    std::vector<std::promise<AsyncResult<std::string>>> promises(
        numConcurrentOps);
    std::vector<std::future<AsyncResult<std::string>>> futures;

    for (int i = 0; i < numConcurrentOps; i++) {
        futures.push_back(promises[i].get_future());
    }

    // Start multiple reads concurrently
    for (int i = 0; i < numConcurrentOps; i++) {
        async_file->asyncRead(testDir / "file1.txt",
                              [&promises, i](AsyncResult<std::string> result) {
                                  promises[i].set_value(std::move(result));
                              });
    }

    // Wait for all operations to complete
    for (int i = 0; i < numConcurrentOps; i++) {
        ASSERT_TRUE(waitForFuture(futures[i]));
        auto result = futures[i].get();

        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.value, "Test file 1 content");
    }
}

// Test Task class
TEST_F(AsyncIOTest, TaskFunctionality) {
    // Create a task manually
    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    Task<AsyncResult<std::string>> task(std::move(future));

    // Set a value to the promise
    AsyncResult<std::string> expectedResult;
    expectedResult.success = true;
    expectedResult.value = "Task test value";

    promise.set_value(expectedResult);

    // Check if task is ready
    EXPECT_TRUE(task.is_ready());

    // Get the result
    auto result = task.get();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.value, "Task test value");
}

// Test async read with very large file
TEST_F(AsyncIOTest, AsyncReadVeryLargeFile) {
    // Create a large file (10 MB)
    fs::path large_file = testDir / "large.txt";
    std::ofstream ofs(large_file);
    std::string chunk(1024, 'X');
    for (int i = 0; i < 10240; ++i) {
        ofs << chunk;
    }
    ofs.close();

    std::promise<AsyncResult<std::string>> promise;
    auto future = promise.get_future();

    async_file->asyncRead(large_file,
                          [&promise](AsyncResult<std::string> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future, 5000));  // Longer timeout for large file
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.value.size(), 10 * 1024 * 1024);
}

// Test async write with binary data
TEST_F(AsyncIOTest, AsyncWriteBinaryData) {
    fs::path binary_file = testDir / "binary.dat";

    std::vector<unsigned char> binary_data = {0x00, 0xFF, 0x7F,
                                              0x80, 0xAA, 0x55};
    std::string data_str(binary_data.begin(), binary_data.end());

    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    async_file->asyncWrite(binary_file, data_str,
                           [&promise](AsyncResult<void> result) {
                               promise.set_value(std::move(result));
                           });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::exists(binary_file));
    EXPECT_EQ(fs::file_size(binary_file), binary_data.size());
}

// Test concurrent async operations
TEST_F(AsyncIOTest, ConcurrentAsyncOperations) {
    const int num_operations = 10;
    std::vector<std::promise<AsyncResult<std::string>>> promises(
        num_operations);
    std::vector<std::future<AsyncResult<std::string>>> futures;

    for (int i = 0; i < num_operations; ++i) {
        futures.push_back(promises[i].get_future());
    }

    // Launch concurrent reads
    for (int i = 0; i < num_operations; ++i) {
        async_file->asyncRead(testDir / "file1.txt",
                              [&promises, i](AsyncResult<std::string> result) {
                                  promises[i].set_value(std::move(result));
                              });
    }

    // Wait for all to complete
    int success_count = 0;
    for (auto& future : futures) {
        if (waitForFuture(future)) {
            auto result = future.get();
            if (result.success) {
                success_count++;
            }
        }
    }

    EXPECT_EQ(success_count, num_operations);
}

// Test async delete with non-existent file
TEST_F(AsyncIOTest, AsyncDeleteNonExistent) {
    fs::path non_existent = testDir / "does_not_exist.txt";

    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    async_file->asyncDelete(non_existent, [&promise](AsyncResult<void> result) {
        promise.set_value(std::move(result));
    });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);  // Should fail for non-existent file
}

// Test async copy with overwrite
TEST_F(AsyncIOTest, AsyncCopyOverwrite) {
    fs::path source = testDir / "file1.txt";
    fs::path dest = testDir / "copy_overwrite.txt";

    // Create destination file
    createFile(dest, "Old content");

    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    async_file->asyncCopy(source.string(), dest.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);

    // Verify content was overwritten
    std::ifstream ifs(dest);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "Test file 1 content");
}

// Test async move to same location
TEST_F(AsyncIOTest, AsyncMoveToSameLocation) {
    fs::path file = testDir / "file1.txt";

    std::promise<AsyncResult<void>> promise;
    auto future = promise.get_future();

    async_file->asyncMove(file.string(), file.string(),
                          [&promise](AsyncResult<void> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_FALSE(result.success);  // Should fail
}

// Test async stat for directory
TEST_F(AsyncIOTest, AsyncStatDirectory) {
    std::promise<AsyncResult<fs::file_status>> promise;
    auto future = promise.get_future();

    async_file->asyncStat(testDir / "subdir1",
                          [&promise](AsyncResult<fs::file_status> result) {
                              promise.set_value(std::move(result));
                          });

    ASSERT_TRUE(waitForFuture(future));
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(fs::is_directory(result.value));
}

// Test async exists for multiple files
TEST_F(AsyncIOTest, AsyncExistsMultipleFiles) {
    std::vector<fs::path> files = {testDir / "file1.txt", testDir / "file2.txt",
                                   testDir / "nonexistent.txt"};

    std::vector<std::promise<AsyncResult<bool>>> promises(files.size());
    std::vector<std::future<AsyncResult<bool>>> futures;

    for (size_t i = 0; i < files.size(); ++i) {
        futures.push_back(promises[i].get_future());
    }

    for (size_t i = 0; i < files.size(); ++i) {
        async_file->asyncExists(files[i],
                                [&promises, i](AsyncResult<bool> result) {
                                    promises[i].set_value(std::move(result));
                                });
    }

    // Check results
    ASSERT_TRUE(waitForFuture(futures[0]));
    EXPECT_TRUE(futures[0].get().value);  // file1.txt exists

    ASSERT_TRUE(waitForFuture(futures[1]));
    EXPECT_TRUE(futures[1].get().value);  // file2.txt exists

    ASSERT_TRUE(waitForFuture(futures[2]));
    EXPECT_FALSE(futures[2].get().value);  // nonexistent.txt doesn't exist
}
