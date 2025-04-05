// filepath: atom/io/test_async_glob.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "async_glob.hpp"
#include "atom/error/exception.hpp"

using namespace atom::io;
namespace fs = std::filesystem;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class AsyncGlobTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary test directory structure
        testDir = fs::temp_directory_path() / "async_glob_test";
        
        // Clean up any existing test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
        
        fs::create_directory(testDir);
        fs::create_directory(testDir / "dir1");
        fs::create_directory(testDir / "dir2");
        fs::create_directory(testDir / "dir1" / "subdir1");
        fs::create_directory(testDir / "dir1" / "subdir2");
        fs::create_directory(testDir / "dir2" / "subdir1");
        fs::create_directory(testDir / ".hidden_dir");
        
        // Create some test files
        createFile(testDir / "file1.txt", "Test file 1");
        createFile(testDir / "file2.txt", "Test file 2");
        createFile(testDir / "file3.dat", "Test file 3");
        createFile(testDir / "dir1" / "file1.txt", "Test file in dir1");
        createFile(testDir / "dir1" / "file2.dat", "Test file in dir1");
        createFile(testDir / "dir2" / "file1.log", "Test file in dir2");
        createFile(testDir / "dir1" / "subdir1" / "nested.txt", "Nested file");
        createFile(testDir / ".hidden_file.txt", "Hidden file");
        
        // Initialize IO context
        io_context = std::make_unique<asio::io_context>();
    }
    
    void TearDown() override {
        // Clean up the test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
        
        // Make sure IO context is stopped
        io_context->stop();
    }
    
    void createFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }
    
    // Helper to run the io_context
    void runContext() {
        io_context->run_for(std::chrono::milliseconds(100));
        io_context->restart();
    }
    
    fs::path testDir;
    std::unique_ptr<asio::io_context> io_context;
};

// Test basic constructor
TEST_F(AsyncGlobTest, Constructor) {
    ASSERT_NO_THROW(AsyncGlob glob(*io_context));
}

// Test glob_sync with simple pattern
TEST_F(AsyncGlobTest, GlobSyncSimplePattern) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "*.txt").string());
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
}

// Test glob_sync with directory pattern
TEST_F(AsyncGlobTest, GlobSyncDirectoryPattern) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "dir*").string(), false, true);
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "dir1"));
    EXPECT_THAT(result, Contains(testDir / "dir2"));
}

// Test glob_sync with recursive search
TEST_F(AsyncGlobTest, GlobSyncRecursive) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir).string(), true, false);
    
    // Should find all non-hidden files and directories
    EXPECT_GT(result.size(), 10);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "dir1" / "subdir1" / "nested.txt"));
}

// Test glob with callback
TEST_F(AsyncGlobTest, GlobWithCallback) {
    AsyncGlob glob(*io_context);
    std::vector<fs::path> callbackResult;
    std::promise<void> callbackPromise;
    auto callbackFuture = callbackPromise.get_future();
    
    glob.glob((testDir / "*.txt").string(), 
              [&callbackResult, &callbackPromise](std::vector<fs::path> result) {
                  callbackResult = std::move(result);
                  callbackPromise.set_value();
              });
    
    runContext();
    
    // Wait for the callback to be called
    ASSERT_EQ(callbackFuture.wait_for(std::chrono::seconds(1)), 
              std::future_status::ready);
    
    ASSERT_EQ(callbackResult.size(), 2);
    EXPECT_THAT(callbackResult, Contains(testDir / "file1.txt"));
    EXPECT_THAT(callbackResult, Contains(testDir / "file2.txt"));
}

// Test glob_async with coroutine
TEST_F(AsyncGlobTest, GlobAsync) {
    AsyncGlob glob(*io_context);
    
    auto task = glob.glob_async((testDir / "*.txt").string());
    auto result = task.get_result();
    
    runContext();
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
}

// Test with complex pattern
TEST_F(AsyncGlobTest, ComplexPattern) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "dir1" / "*" / "*.txt").string());
    
    ASSERT_EQ(result.size(), 1);
    EXPECT_THAT(result, Contains(testDir / "dir1" / "subdir1" / "nested.txt"));
}

// Test with question mark wildcard
TEST_F(AsyncGlobTest, QuestionMarkWildcard) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "file?.txt").string());
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
}

// Test with character class wildcard
TEST_F(AsyncGlobTest, CharacterClassWildcard) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "file[1-2].txt").string());
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
}

// Test with negated character class
TEST_F(AsyncGlobTest, NegatedCharacterClass) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "file[!3].txt").string());
    
    ASSERT_EQ(result.size(), 2);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
}

// Test with recursive pattern
TEST_F(AsyncGlobTest, RecursivePattern) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "**" / "*.txt").string());
    
    EXPECT_GT(result.size(), 3);
    EXPECT_THAT(result, Contains(testDir / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "file2.txt"));
    EXPECT_THAT(result, Contains(testDir / "dir1" / "file1.txt"));
    EXPECT_THAT(result, Contains(testDir / "dir1" / "subdir1" / "nested.txt"));
}

// Test with non-existent directory
TEST_F(AsyncGlobTest, NonExistentDirectory) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "non_existent_dir" / "*.txt").string());
    
    EXPECT_TRUE(result.empty());
}

// Test with empty directory
TEST_F(AsyncGlobTest, EmptyDirectory) {
    // Create an empty directory
    fs::create_directory(testDir / "empty_dir");
    
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "empty_dir" / "*.txt").string());
    
    EXPECT_TRUE(result.empty());
}

// Test with dir-only flag
TEST_F(AsyncGlobTest, DirOnlyFlag) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "*").string(), false, true);
    
    // Should only match directories, not files
    for (const auto& path : result) {
        EXPECT_TRUE(fs::is_directory(path));
    }
    EXPECT_THAT(result, Contains(testDir / "dir1"));
    EXPECT_THAT(result, Contains(testDir / "dir2"));
}

// Test with hidden files
TEST_F(AsyncGlobTest, HiddenFiles) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / ".*").string());
    
    // Should find hidden files/directories
    EXPECT_THAT(result, Contains(testDir / ".hidden_file.txt"));
    EXPECT_THAT(result, Contains(testDir / ".hidden_dir"));
}

// Test with tilde expansion
TEST_F(AsyncGlobTest, TildeExpansion) {
    // This test is platform-dependent, so we'll make a conditional test
    AsyncGlob glob(*io_context);
    
    // Just verify it doesn't throw - actual expansion is platform-dependent
    EXPECT_NO_THROW(glob.glob_sync("~/test_pattern"));
}

// Test with multiple patterns in parallel
TEST_F(AsyncGlobTest, ParallelGlob) {
    AsyncGlob glob(*io_context);
    std::vector<std::future<std::vector<fs::path>>> futures;
    
    // Start multiple glob operations in parallel
    for (int i = 0; i < 5; i++) {
        futures.push_back(std::async(std::launch::async, [&glob, this]() {
            return glob.glob_sync((testDir / "*.txt").string());
        }));
    }
    
    // Check results from all operations
    for (auto& future : futures) {
        auto result = future.get();
        ASSERT_EQ(result.size(), 2);
        EXPECT_THAT(result, Contains(testDir / "file1.txt"));
        EXPECT_THAT(result, Contains(testDir / "file2.txt"));
    }
}

// Test error handling with invalid pattern
TEST_F(AsyncGlobTest, InvalidPattern) {
    AsyncGlob glob(*io_context);
    
    // Unbalanced bracket should be handled gracefully
    auto result = glob.glob_sync((testDir / "file[1.txt").string());
    
    // Should either be empty or return a valid subset of files
    if (!result.empty()) {
        for (const auto& path : result) {
            EXPECT_TRUE(fs::exists(path));
        }
    }
}

// Test with pattern ending in directory separator
TEST_F(AsyncGlobTest, PatternEndingInSeparator) {
    AsyncGlob glob(*io_context);
    
    auto result = glob.glob_sync((testDir / "dir1/").string(), false, true);
    
    // Should match the directory
    ASSERT_EQ(result.size(), 1);
    EXPECT_THAT(result, Contains(testDir / "dir1"));
}

// Test with absolute and relative paths
TEST_F(AsyncGlobTest, AbsoluteVsRelativePaths) {
    AsyncGlob glob(*io_context);
    
    // Change to the test directory
    auto originalPath = fs::current_path();
    fs::current_path(testDir);
    
    // Do a relative path glob
    auto relativeResult = glob.glob_sync("*.txt");
    
    // Change back to original directory
    fs::current_path(originalPath);
    
    // Do an absolute path glob
    auto absoluteResult = glob.glob_sync((testDir / "*.txt").string());
    
    // The number of results should be the same
    ASSERT_EQ(relativeResult.size(), absoluteResult.size());
    ASSERT_EQ(relativeResult.size(), 2);
    
    // But the paths will be different (relative vs absolute)
    EXPECT_THAT(relativeResult, Contains(fs::path("file1.txt")));
    EXPECT_THAT(relativeResult, Contains(fs::path("file2.txt")));
    EXPECT_THAT(absoluteResult, Contains(testDir / "file1.txt"));
    EXPECT_THAT(absoluteResult, Contains(testDir / "file2.txt"));
}

// Test with very deep directory structure
TEST_F(AsyncGlobTest, DeepDirectoryStructure) {
    // Create a deep directory structure
    fs::path deepDir = testDir / "deep";
    fs::create_directory(deepDir);
    
    fs::path currentPath = deepDir;
    for (int i = 0; i < 20; i++) {
        currentPath = currentPath / ("level" + std::to_string(i));
        fs::create_directory(currentPath);
    }
    
    // Create a file at the deepest level
    createFile(currentPath / "deep_file.txt", "Deep file");
    
    AsyncGlob glob(*io_context);
    
    // Test recursive glob on deep structure
    auto result = glob.glob_sync((deepDir / "**" / "*.txt").string());
    
    ASSERT_EQ(result.size(), 1);
    EXPECT_THAT(result, Contains(currentPath / "deep_file.txt"));
}

// Test performance with large number of files
TEST_F(AsyncGlobTest, PerformanceWithManyFiles) {
    // Create directory with many files
    fs::path manyFilesDir = testDir / "many_files";
    fs::create_directory(manyFilesDir);
    
    const int numFiles = 100;  // Can increase for more thorough testing
    for (int i = 0; i < numFiles; i++) {
        createFile(manyFilesDir / ("file" + std::to_string(i) + ".txt"), 
                   "Content " + std::to_string(i));
    }
    
    AsyncGlob glob(*io_context);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = glob.glob_sync((manyFilesDir / "*.txt").string());
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    ASSERT_EQ(result.size(), numFiles);
    
    // Performance check - should be reasonably fast
    std::cout << "Time to glob " << numFiles << " files: " << duration << "ms" << std::endl;
    
    // This is not a strict test as timing depends on the system,
    // but we can output the timing for information
}

// Test with concurrent modification of directory
TEST_F(AsyncGlobTest, ConcurrentModification) {
    // Create a directory for the test
    fs::path concurrentDir = testDir / "concurrent";
    fs::create_directory(concurrentDir);
    
    // Add some initial files
    createFile(concurrentDir / "file1.txt", "Initial file 1");
    createFile(concurrentDir / "file2.txt", "Initial file 2");
    
    AsyncGlob glob(*io_context);
    
    // Start a glob operation in a separate thread
    std::promise<std::vector<fs::path>> resultPromise;
    auto resultFuture = resultPromise.get_future();
    
    std::thread globThread([&glob, &concurrentDir, &resultPromise]() {
        // Simulate a slow glob operation
        auto result = glob.glob_sync((concurrentDir / "*.txt").string());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        resultPromise.set_value(result);
    });
    
    // Modify the directory while the glob is running
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    createFile(concurrentDir / "file3.txt", "Added during glob");
    fs::remove(concurrentDir / "file1.txt");
    
    // Wait for the glob to complete
    auto result = resultFuture.get();
    globThread.join();
    
    // We can't make strict assertions about what should be returned, as it depends
    // on timing, but we can verify it didn't crash and returned something reasonable
    for (const auto& path : result) {
        std::cout << "Found in concurrent test: " << path << std::endl;
    }
    
    // Verify the final state
    auto finalResult = glob.glob_sync((concurrentDir / "*.txt").string());
    ASSERT_EQ(finalResult.size(), 2);
    EXPECT_THAT(finalResult, Contains(concurrentDir / "file2.txt"));
    EXPECT_THAT(finalResult, Contains(concurrentDir / "file3.txt"));
}

// Test with special characters in filenames
TEST_F(AsyncGlobTest, SpecialCharacters) {
    // Create files with special characters
    createFile(testDir / "file with spaces.txt", "Space file");
    createFile(testDir / "file_with_[brackets].txt", "Bracket file");
    createFile(testDir / "file-with-dashes.txt", "Dash file");
    createFile(testDir / "file+with+plus.txt", "Plus file");
    createFile(testDir / "file.with.dots.txt", "Dot file");
    
    AsyncGlob glob(*io_context);
    
    // Test glob with space in pattern
    auto spaceResult = glob.glob_sync((testDir / "file with*.txt").string());
    ASSERT_EQ(spaceResult.size(), 1);
    EXPECT_THAT(spaceResult, Contains(testDir / "file with spaces.txt"));
    
    // Test glob with bracket in pattern (requires escaping)
    auto bracketResult = glob.glob_sync((testDir / "file_with_\\[*").string());
    ASSERT_EQ(bracketResult.size(), 1);
    EXPECT_THAT(bracketResult, Contains(testDir / "file_with_[brackets].txt"));
    
    // Test glob with various special characters
    auto mixedResult = glob.glob_sync((testDir / "file*").string());
    ASSERT_EQ(mixedResult.size(), 8); // Includes the original files
    EXPECT_THAT(mixedResult, Contains(testDir / "file-with-dashes.txt"));
    EXPECT_THAT(mixedResult, Contains(testDir / "file+with+plus.txt"));
    EXPECT_THAT(mixedResult, Contains(testDir / "file.with.dots.txt"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}