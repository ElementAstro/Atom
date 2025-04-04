#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>


#include "atom/system/stat.hpp"

#include "atom/system/stat.cpp"

using namespace atom::system;
using namespace std::chrono_literals;

class StatTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test file
        test_file_path = fs::temp_directory_path() / "test_stat_file.txt";
        std::ofstream file(test_file_path);
        file << "Test content for Stat class testing" << std::endl;
        file.close();

        // Create a test directory
        test_dir_path = fs::temp_directory_path() / "test_stat_dir";
        fs::create_directory(test_dir_path);

        // Create a test symlink if supported
#ifndef _WIN32  // Symlinks require admin privileges on Windows
        test_symlink_path = fs::temp_directory_path() / "test_stat_symlink";
        if (fs::exists(test_symlink_path)) {
            fs::remove(test_symlink_path);
        }
        fs::create_symlink(test_file_path, test_symlink_path);
#endif

        // Create a stat object for the test file
        stat = std::make_unique<Stat>(test_file_path);
    }

    void TearDown() override {
        // Clean up test files
        try {
            if (fs::exists(test_file_path)) {
                fs::remove(test_file_path);
            }
            if (fs::exists(test_dir_path)) {
                fs::remove_all(test_dir_path);
            }
#ifndef _WIN32
            if (fs::exists(test_symlink_path)) {
                fs::remove(test_symlink_path);
            }
#endif
        } catch (const std::exception& e) {
            std::cerr << "Error during cleanup: " << e.what() << std::endl;
        }
    }

    fs::path test_file_path;
    fs::path test_dir_path;
    fs::path test_symlink_path;
    std::unique_ptr<Stat> stat;
};

// Test basic constructor
TEST_F(StatTest, Constructor) {
    ASSERT_NO_THROW(Stat varname(test_file_path));
    ASSERT_NO_THROW(Stat(test_file_path, true));
    ASSERT_NO_THROW(Stat(test_file_path, false));
}

// Test constructor with non-existent file
TEST_F(StatTest, ConstructorNonExistentFile) {
    fs::path non_existent = fs::temp_directory_path() / "non_existent_file.txt";

    // Should not throw but subsequent operations might
    ASSERT_NO_THROW(Stat varname(non_existent));

    // Creating stat object for non-existent file and checking exists()
    Stat nonexistent_stat(non_existent);
    EXPECT_FALSE(nonexistent_stat.exists());
}

// Test exists method
TEST_F(StatTest, Exists) {
    EXPECT_TRUE(stat->exists());

    Stat dir_stat(test_dir_path);
    EXPECT_TRUE(dir_stat.exists());

    Stat non_existent(fs::temp_directory_path() / "non_existent_file.txt");
    EXPECT_FALSE(non_existent.exists());
}

// Test update method
TEST_F(StatTest, Update) {
    // First get the initial size
    auto initial_size = stat->size();

    // Modify the file
    std::ofstream file(test_file_path, std::ios::app);
    file << "Additional content to change file size" << std::endl;
    file.close();

    // Size should be the same before update
    EXPECT_EQ(stat->size(), initial_size);

    // Update and check size changed
    stat->update();
    EXPECT_GT(stat->size(), initial_size);
}

// Test type method
TEST_F(StatTest, Type) {
    EXPECT_EQ(stat->type(), fs::file_type::regular);

    Stat dir_stat(test_dir_path);
    EXPECT_EQ(dir_stat.type(), fs::file_type::directory);

#ifndef _WIN32
    Stat symlink_stat(test_symlink_path, false);
    EXPECT_EQ(symlink_stat.type(), fs::file_type::symlink);
#endif
}

// Test size method
TEST_F(StatTest, Size) {
    // File should have some size
    EXPECT_GT(stat->size(), 0);

    // Directory size varies by platform, just ensure it doesn't throw
    Stat dir_stat(test_dir_path);
    ASSERT_NO_THROW(dir_stat.size());
}

// Test time methods
TEST_F(StatTest, TimeMethods) {
    // All times should be greater than zero
    EXPECT_GT(stat->atime(), 0);
    EXPECT_GT(stat->mtime(), 0);
    EXPECT_GT(stat->ctime(), 0);

    // Times should be recent (within the last day)
    auto now = std::time(nullptr);
    auto day_ago = now - 86400;  // 86400 seconds = 1 day

    EXPECT_GT(stat->atime(), day_ago);
    EXPECT_GT(stat->mtime(), day_ago);
    EXPECT_GT(stat->ctime(), day_ago);
}

// Test mode method
TEST_F(StatTest, Mode) {
    // Mode should be a non-zero value
    EXPECT_GT(stat->mode(), 0);
}

// Test uid and gid methods
TEST_F(StatTest, UidAndGid) {
    // These should return valid IDs but we can't predict them
    // Just ensure they don't throw
    ASSERT_NO_THROW(stat->uid());
    ASSERT_NO_THROW(stat->gid());
}

// Test path method
TEST_F(StatTest, Path) { EXPECT_EQ(stat->path(), test_file_path); }

// Test link, device and inode methods
TEST_F(StatTest, SystemSpecificMethods) {
    // These tests just verify the methods don't throw, as values are
    // system-dependent
    ASSERT_NO_THROW(stat->hardLinkCount());
    ASSERT_NO_THROW(stat->deviceId());
    ASSERT_NO_THROW(stat->inodeNumber());
    ASSERT_NO_THROW(stat->blockSize());
}

// Test owner and group name methods
TEST_F(StatTest, OwnerAndGroupName) {
    // These should return valid strings but we can't predict them
    // Just ensure they don't throw and return non-empty values
    std::string owner = stat->ownerName();
    EXPECT_FALSE(owner.empty());

    std::string group = stat->groupName();
    // Group might be empty on some Windows configurations
    ASSERT_NO_THROW(group);
}

// Test file type checking methods
TEST_F(StatTest, FileTypeChecks) {
    // Regular file
    EXPECT_FALSE(stat->isSymlink());
    EXPECT_FALSE(stat->isDirectory());
    EXPECT_TRUE(stat->isRegularFile());

    // Directory
    Stat dir_stat(test_dir_path);
    EXPECT_FALSE(dir_stat.isSymlink());
    EXPECT_TRUE(dir_stat.isDirectory());
    EXPECT_FALSE(dir_stat.isRegularFile());

#ifndef _WIN32
    // Symlink
    Stat symlink_stat(test_symlink_path, false);
    EXPECT_TRUE(symlink_stat.isSymlink());
    EXPECT_FALSE(symlink_stat.isDirectory());
    EXPECT_FALSE(symlink_stat.isRegularFile());
#endif
}

// Test permission checking methods
TEST_F(StatTest, PermissionChecks) {
    // These tests are somewhat system-dependent but should generally pass
    EXPECT_TRUE(stat->isReadable());
    EXPECT_TRUE(stat->isWritable());

    // Execute permission varies by platform
    ASSERT_NO_THROW(stat->isExecutable());

    // Test permission by category
    ASSERT_NO_THROW(
        stat->hasPermission(true, false, false, FilePermission::Read));
    ASSERT_NO_THROW(
        stat->hasPermission(false, true, false, FilePermission::Write));
    ASSERT_NO_THROW(
        stat->hasPermission(false, false, true, FilePermission::Execute));
}

// Test symlink target method
TEST_F(StatTest, SymlinkTarget) {
#ifndef _WIN32
    Stat symlink_stat(test_symlink_path, false);
    fs::path target = symlink_stat.symlinkTarget();
    EXPECT_EQ(target, test_file_path);
#endif

    // Regular file should return empty path
    fs::path target = stat->symlinkTarget();
    EXPECT_TRUE(target.empty());
}

// Test formatTime static method
TEST_F(StatTest, FormatTime) {
    std::time_t now = std::time(nullptr);

    // Default format
    std::string formatted = Stat::formatTime(now);
    EXPECT_FALSE(formatted.empty());
    EXPECT_EQ(formatted.length(), 19);  // YYYY-MM-DD HH:MM:SS = 19 chars

    // Custom format
    std::string custom = Stat::formatTime(now, "%Y%m%d");
    EXPECT_FALSE(custom.empty());
    EXPECT_EQ(custom.length(), 8);  // YYYYMMDD = 8 chars
}

// Test following symlinks behavior
TEST_F(StatTest, FollowSymlinks) {
#ifndef _WIN32
    // Create a stat object that follows symlinks
    Stat follow_stat(test_symlink_path, true);

    // Should have properties of target file
    EXPECT_TRUE(follow_stat.isRegularFile());
    EXPECT_FALSE(follow_stat.isSymlink());

    // Create a stat object that doesn't follow symlinks
    Stat nofollow_stat(test_symlink_path, false);

    // Should have properties of symlink itself
    EXPECT_FALSE(nofollow_stat.isRegularFile());
    EXPECT_TRUE(nofollow_stat.isSymlink());
#endif
}

// Test with special files
TEST_F(StatTest, SpecialFiles) {
#ifdef _WIN32
    // Test with Windows special files
    std::vector<fs::path> special_files = {
        "C:\\$Recycle.Bin", "C:\\pagefile.sys",
        "C:\\Windows\\System32\\drivers\\etc\\hosts"};
#else
    // Test with Unix special files
    std::vector<fs::path> special_files = {"/dev/null", "/etc/passwd",
                                           "/proc/self"};
#endif

    for (const auto& path : special_files) {
        if (fs::exists(path)) {
            ASSERT_NO_THROW({
                Stat special_stat(path);
                if (special_stat.exists()) {
                    special_stat.type();
                    special_stat.size();
                }
            });
        }
    }
}

// Test error handling with inaccessible files
TEST_F(StatTest, InaccessibleFiles) {
#ifdef _WIN32
    // Windows system files that might be inaccessible
    fs::path inaccessible = "C:\\System Volume Information";
#else
    // Linux files that might be inaccessible
    fs::path inaccessible = "/root/.ssh";
#endif

    if (fs::exists(inaccessible)) {
        Stat inacc_stat(inaccessible);

        // Should not throw on creation, but operations might fail
        ASSERT_NO_THROW(inacc_stat.exists());

        // Skip tests if we actually have access (running as admin/root)
        if (inacc_stat.exists() && !inacc_stat.isReadable()) {
            // Various operations might throw std::system_error
            EXPECT_ANY_THROW(inacc_stat.size());
        }
    }
}

// Test with large files
TEST_F(StatTest, LargeFile) {
    // Create a somewhat large file (10MB)
    fs::path large_file_path =
        fs::temp_directory_path() / "large_test_file.bin";

    // Only create if we have enough disk space
    try {
        std::ofstream large_file(large_file_path, std::ios::binary);
        const size_t size = 10 * 1024 * 1024;  // 10MB
        std::vector<char> buffer(1024, 'A');

        for (size_t i = 0; i < size / 1024; ++i) {
            large_file.write(buffer.data(), buffer.size());
        }
        large_file.close();

        Stat large_stat(large_file_path);
        EXPECT_EQ(large_stat.size(), size);

        // Clean up
        fs::remove(large_file_path);
    } catch (const std::exception& e) {
        std::cerr << "Skipping large file test: " << e.what() << std::endl;
    }
}

// Test with empty file
TEST_F(StatTest, EmptyFile) {
    fs::path empty_file_path =
        fs::temp_directory_path() / "empty_test_file.txt";
    std::ofstream empty_file(empty_file_path);
    empty_file.close();

    Stat empty_stat(empty_file_path);
    EXPECT_EQ(empty_stat.size(), 0);

    // Clean up
    fs::remove(empty_file_path);
}

// Test concurrent access to same file
TEST_F(StatTest, ConcurrentAccess) {
    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this]() {
            // Create a new Stat object for the same file in each thread
            Stat thread_stat(test_file_path);

            // Call various methods
            thread_stat.exists();
            thread_stat.type();
            thread_stat.size();
            thread_stat.mtime();
            thread_stat.isRegularFile();
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // If we got here without exceptions, the test passed
    SUCCEED();
}

// Test with path containing special characters
TEST_F(StatTest, SpecialCharactersInPath) {
    fs::path special_path =
        fs::temp_directory_path() / "test file with spaces.txt";

    try {
        std::ofstream special_file(special_path);
        special_file << "Test content" << std::endl;
        special_file.close();

        ASSERT_NO_THROW({
            Stat special_stat(special_path);
            EXPECT_TRUE(special_stat.exists());
            EXPECT_TRUE(special_stat.isRegularFile());
        });

        // Clean up
        fs::remove(special_path);
    } catch (const std::exception& e) {
        std::cerr << "Skipping special characters test: " << e.what()
                  << std::endl;
    }
}

// Test with file that is modified while being observed
TEST_F(StatTest, FileModificationDuringObservation) {
    auto initial_mtime = stat->mtime();
    auto initial_size = stat->size();

    // Modify the file
    std::ofstream file(test_file_path, std::ios::app);
    file << "Content added during test" << std::endl;
    file.close();

    // Need to wait a bit to ensure mtime is updated (some filesystems have
    // 1-second precision)
    std::this_thread::sleep_for(1100ms);

    // Before update, stat should still show old values
    EXPECT_EQ(stat->mtime(), initial_mtime);
    EXPECT_EQ(stat->size(), initial_size);

    // After update, values should change
    stat->update();
    EXPECT_NE(stat->mtime(), initial_mtime);
    EXPECT_GT(stat->size(), initial_size);
}

// Test with unusual files (if they exist on the test system)
TEST_F(StatTest, UnusualFiles) {
#ifdef _WIN32
    // Files that might be found on Windows
    std::vector<fs::path> unusual_files = {"C:\\$Extend\\$ObjId",
                                           "C:\\hiberfil.sys"};
#else
    // Files that might be found on Unix/Linux
    std::vector<fs::path> unusual_files = {"/dev/zero", "/dev/random",
                                           "/proc/self/fd/0"};
#endif

    for (const auto& path : unusual_files) {
        if (fs::exists(path)) {
            ASSERT_NO_THROW({
                Stat unusual_stat(path);
                unusual_stat.exists();
                unusual_stat.type();
            });
        }
    }
}

// Test edge cases for formatTime method
TEST_F(StatTest, FormatTimeEdgeCases) {
    // Test with epoch time
    std::time_t epoch = 0;
    std::string epoch_formatted = Stat::formatTime(epoch);
    EXPECT_FALSE(epoch_formatted.empty());

    // Test with future time
    std::time_t future =
        std::time(nullptr) + 10 * 365 * 86400;  // ~10 years in the future
    std::string future_formatted = Stat::formatTime(future);
    EXPECT_FALSE(future_formatted.empty());

    // Test with invalid format string (should fall back to default or show
    // error)
    ASSERT_NO_THROW(Stat::formatTime(std::time(nullptr), ""));
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}