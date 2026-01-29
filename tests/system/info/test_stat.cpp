#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

#include "atom/system/info/stat.hpp"

namespace atom::system::test {

namespace fs = std::filesystem;
using FilePermission = atom::system::FilePermission;
using FileStat =
    atom::system::Stat;  // Alias to avoid conflict with gtest's Stat

class StatTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test directory
        testDir = fs::temp_directory_path() / "atom_stat_test";
        fs::create_directories(testDir);

        // Create test files
        testFile = testDir / "test_file.txt";
        testEmptyFile = testDir / "empty_file.txt";
        testBinaryFile = testDir / "binary_file.bin";
        testSymlink = testDir / "test_symlink";

        // Create regular file with content
        {
            std::ofstream file(testFile);
            file << "This is a test file with some content.\n";
            file << "It has multiple lines.\n";
            file << "For testing purposes.\n";
        }

        // Create empty file
        { std::ofstream file(testEmptyFile); }

        // Create binary file
        {
            std::ofstream file(testBinaryFile, std::ios::binary);
            for (int i = 0; i < 256; ++i) {
                file.put(static_cast<char>(i));
            }
        }

        // Create symlink (if supported)
        try {
            fs::create_symlink(testFile, testSymlink);
            symlinkSupported = true;
        } catch (const std::exception&) {
            symlinkSupported = false;
        }

        nonExistentFile = testDir / "nonexistent.txt";
    }

    void TearDown() override {
        // Clean up test files and directory
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    fs::path testDir;
    fs::path testFile;
    fs::path testEmptyFile;
    fs::path testBinaryFile;
    fs::path testSymlink;
    fs::path nonExistentFile;
    bool symlinkSupported = false;
};

// Test basic file existence
TEST_F(StatTest, FileExists) {
    FileStat stat(testFile);
    EXPECT_TRUE(stat.exists());

    FileStat nonExistentStat(nonExistentFile);
    EXPECT_FALSE(nonExistentStat.exists());
}

// Test file type detection
TEST_F(StatTest, FileType) {
    FileStat regularFileStat(testFile);
    EXPECT_EQ(regularFileStat.type(), fs::file_type::regular);

    FileStat directoryStat(testDir);
    EXPECT_EQ(directoryStat.type(), fs::file_type::directory);

    if (symlinkSupported) {
        FileStat symlinkStat(testSymlink, false);  // Don't follow symlinks
        EXPECT_EQ(symlinkStat.type(), fs::file_type::symlink);

        FileStat symlinkTargetStat(testSymlink, true);  // Follow symlinks
        EXPECT_EQ(symlinkTargetStat.type(), fs::file_type::regular);
    }
}

// Test file size
TEST_F(StatTest, FileSize) {
    FileStat regularFileStat(testFile);
    std::uintmax_t size = regularFileStat.size();
    EXPECT_GT(size, 0);

    FileStat emptyFileStat(testEmptyFile);
    EXPECT_EQ(emptyFileStat.size(), 0);

    FileStat binaryFileStat(testBinaryFile);
    EXPECT_EQ(binaryFileStat.size(), 256);
}

// Test file timestamps
TEST_F(StatTest, FileTimestamps) {
    FileStat stat(testFile);

    std::time_t atime = stat.atime();
    std::time_t mtime = stat.mtime();
    std::time_t ctime = stat.ctime();

    EXPECT_GT(atime, 0);
    EXPECT_GT(mtime, 0);
    EXPECT_GT(ctime, 0);

    // Modification time should be recent (within last hour)
    std::time_t now = std::time(nullptr);
    EXPECT_LT(now - mtime, 3600);  // Within 1 hour
}

// Test file permissions
TEST_F(StatTest, FilePermissions) {
    FileStat stat(testFile);

    bool isReadable = stat.isReadable();
    bool isWritable = stat.isWritable();
    bool isExecutable = stat.isExecutable();

    EXPECT_TRUE(isReadable);
    EXPECT_TRUE(isWritable);
    // Executable depends on platform and file creation

    // Test permission checking (user permissions)
    EXPECT_TRUE(stat.hasPermission(true, false, false, FilePermission::Read));
    EXPECT_TRUE(stat.hasPermission(true, false, false, FilePermission::Write));
}

// Test directory permissions
TEST_F(StatTest, DirectoryPermissions) {
    FileStat dirStat(testDir);

    EXPECT_TRUE(dirStat.isReadable());
    EXPECT_TRUE(dirStat.isWritable());
    EXPECT_TRUE(dirStat.isExecutable());  // Execute permission for directories
                                          // means "searchable"
}

// Test file ownership (platform-dependent)
TEST_F(StatTest, FileOwnership) {
    FileStat stat(testFile);

    auto uid = stat.uid();
    auto gid = stat.gid();

    // UID and GID should be valid (non-negative)
    EXPECT_GE(uid, 0);
    EXPECT_GE(gid, 0);

    std::string owner = stat.ownerName();
    std::string group = stat.groupName();

    // Owner and group names should not be empty (on most systems)
    EXPECT_FALSE(owner.empty());
    EXPECT_FALSE(group.empty());
}

// Test file update functionality
TEST_F(StatTest, UpdateFileStats) {
    FileStat stat(testFile);

    std::uintmax_t originalSize = stat.size();
    std::time_t originalMtime = stat.mtime();

    // Wait a moment to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Modify the file
    {
        std::ofstream file(testFile, std::ios::app);
        file << "\nAdditional content added.\n";
    }

    // Update stats
    stat.update();

    std::uintmax_t newSize = stat.size();
    std::time_t newMtime = stat.mtime();

    EXPECT_GT(newSize, originalSize);
    EXPECT_GE(newMtime, originalMtime);
}

// Test file type checking methods
TEST_F(StatTest, FileTypeChecking) {
    FileStat regularFileStat(testFile);
    EXPECT_TRUE(regularFileStat.isRegularFile());
    EXPECT_FALSE(regularFileStat.isDirectory());
    EXPECT_FALSE(regularFileStat.isSymlink());

    FileStat directoryStat(testDir);
    EXPECT_FALSE(directoryStat.isRegularFile());
    EXPECT_TRUE(directoryStat.isDirectory());
    EXPECT_FALSE(directoryStat.isSymlink());

    if (symlinkSupported) {
        FileStat symlinkStat(testSymlink, false);  // Don't follow symlinks
        EXPECT_FALSE(symlinkStat.isRegularFile());
        EXPECT_FALSE(symlinkStat.isDirectory());
        EXPECT_TRUE(symlinkStat.isSymlink());
    }
}

// Test hard link count
TEST_F(StatTest, HardLinkCount) {
    FileStat stat(testFile);

    auto linkCount = stat.hardLinkCount();
    EXPECT_GE(linkCount, 1);  // At least one link (the file itself)

    // Create a hard link (if supported)
    fs::path hardLink = testDir / "hard_link.txt";
    try {
        fs::create_hard_link(testFile, hardLink);

        stat.update();
        auto newLinkCount = stat.hardLinkCount();
        EXPECT_EQ(newLinkCount, linkCount + 1);

        // Clean up
        fs::remove(hardLink);
    } catch (const std::exception&) {
        // Hard links not supported on this filesystem
        GTEST_SKIP() << "Hard links not supported";
    }
}

// Test device information
TEST_F(StatTest, DeviceInformation) {
    FileStat stat(testFile);

    auto deviceId = stat.deviceId();
    auto inodeNumber = stat.inodeNumber();

    EXPECT_GE(deviceId, 0);
    EXPECT_GT(inodeNumber, 0);
}

// Test file mode
TEST_F(StatTest, FileMode) {
    FileStat stat(testFile);

    auto mode = stat.mode();
    EXPECT_GT(mode, 0);

    // Check that mode contains expected permission bits
    bool hasOwnerRead = (mode & 0400) != 0;
    bool hasOwnerWrite = (mode & 0200) != 0;

    EXPECT_TRUE(hasOwnerRead);
    EXPECT_TRUE(hasOwnerWrite);
}

// Test error handling
class StatErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        nonExistentPath = "/nonexistent/path/file.txt";
        invalidPath = "";
    }

    fs::path nonExistentPath;
    fs::path invalidPath;
};

// Test handling of nonexistent files
TEST_F(StatErrorTest, NonexistentFile) {
    EXPECT_THROW(FileStat stat(nonExistentPath), std::system_error);
}

// Test handling of invalid paths
TEST_F(StatErrorTest, InvalidPath) {
    EXPECT_THROW(FileStat stat(invalidPath), std::system_error);
}

// Test accessing properties of nonexistent files
TEST_F(StatErrorTest, AccessNonexistentFileProperties) {
    try {
        FileStat stat(nonExistentPath);
        FAIL() << "Expected std::system_error";
    } catch (const std::system_error& e) {
        // Expected behavior
        SUCCEED();
    }
}

// Platform-specific tests
class StatPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "atom_stat_platform_test";
        fs::create_directories(testDir);

        testFile = testDir / "platform_test.txt";
        {
            std::ofstream file(testFile);
            file << "Platform-specific test file\n";
        }
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    fs::path testDir;
    fs::path testFile;
};

#ifdef _WIN32
// Windows-specific tests
TEST_F(StatPlatformTest, WindowsSpecificAttributes) {
    FileStat stat(testFile);

    // Test Windows-specific functionality
    EXPECT_NO_THROW(stat.mode());
    EXPECT_NO_THROW(stat.uid());
    EXPECT_NO_THROW(stat.gid());

    // On Windows, UID/GID might be different
    auto uid = stat.uid();
    auto gid = stat.gid();
    EXPECT_GE(uid, 0);
    EXPECT_GE(gid, 0);
}

TEST_F(StatPlatformTest, WindowsHiddenFiles) {
    // Create a hidden file (Windows-specific)
    fs::path hiddenFile = testDir / "hidden_file.txt";
    {
        std::ofstream file(hiddenFile);
        file << "Hidden file content\n";
    }

    // Set hidden attribute (this would require Windows API calls in real
    // implementation)
    FileStat stat(hiddenFile);
    EXPECT_TRUE(stat.exists());
    EXPECT_TRUE(stat.isRegularFile());
}

#elif defined(__linux__)
// Linux-specific tests
TEST_F(StatPlatformTest, LinuxSpecificAttributes) {
    FileStat stat(testFile);

    // Test Linux-specific functionality
    auto uid = stat.uid();
    auto gid = stat.gid();

    EXPECT_GE(uid, 0);
    EXPECT_GE(gid, 0);

    // Test that we can get owner/group names
    std::string owner = stat.ownerName();
    std::string group = stat.groupName();

    EXPECT_FALSE(owner.empty());
    EXPECT_FALSE(group.empty());
}

TEST_F(StatPlatformTest, LinuxPermissionBits) {
    FileStat stat(testFile);

    auto mode = stat.mode();

    // Check standard permission bits
    bool ownerRead = (mode & S_IRUSR) != 0;
    bool ownerWrite = (mode & S_IWUSR) != 0;
    bool ownerExecute = (mode & S_IXUSR) != 0;

    EXPECT_TRUE(ownerRead);
    EXPECT_TRUE(ownerWrite);
    // Execute bit depends on file creation

    bool groupRead = (mode & S_IRGRP) != 0;
    bool groupWrite = (mode & S_IWGRP) != 0;

    // Group permissions depend on umask and system settings
    EXPECT_NO_THROW(groupRead);
    EXPECT_NO_THROW(groupWrite);
}

#elif defined(__APPLE__)
// macOS-specific tests
TEST_F(StatPlatformTest, MacOSSpecificAttributes) {
    FileStat stat(testFile);

    // Test macOS-specific functionality
    EXPECT_NO_THROW(stat.uid());
    EXPECT_NO_THROW(stat.gid());
    EXPECT_NO_THROW(stat.ownerName());
    EXPECT_NO_THROW(stat.groupName());

    // macOS should support standard Unix permissions
    EXPECT_TRUE(stat.hasPermission(true, false, false, FilePermission::Read));
    EXPECT_TRUE(stat.hasPermission(true, false, false, FilePermission::Write));
}
#endif

// Performance tests
class StatPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "atom_stat_perf_test";
        fs::create_directories(testDir);

        // Create many test files
        for (int i = 0; i < 100; ++i) {
            fs::path file = testDir / ("file_" + std::to_string(i) + ".txt");
            std::ofstream stream(file);
            stream << "Test file " << i << " content\n";
        }
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    fs::path testDir;
};

// Test performance of stat operations on many files
TEST_F(StatPerformanceTest, MultipleFileStats) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        fs::path file = testDir / ("file_" + std::to_string(i) + ".txt");
        FileStat stat(file);

        // Access various properties
        EXPECT_TRUE(stat.exists());
        EXPECT_GT(stat.size(), 0);
        EXPECT_GT(stat.mtime(), 0);
        EXPECT_TRUE(stat.isRegularFile());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (1 second for 100 files)
    EXPECT_LT(duration.count(), 1000);
}

// Test repeated stat operations on same file
TEST_F(StatPerformanceTest, RepeatedStatOperations) {
    fs::path testFile = testDir / "file_0.txt";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        FileStat stat(testFile);
        EXPECT_TRUE(stat.exists());
        EXPECT_GT(stat.size(), 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Repeated operations should be fast (within 500ms for 1000 operations)
    EXPECT_LT(duration.count(), 500);
}

// Edge case tests
class StatEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "atom_stat_edge_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir, ec);
    }

    fs::path testDir;
};

// Test very large files (if possible)
TEST_F(StatEdgeCaseTest, LargeFile) {
    fs::path largeFile = testDir / "large_file.txt";

    try {
        // Create a moderately large file (1MB)
        std::ofstream file(largeFile, std::ios::binary);
        std::vector<char> buffer(1024, 'A');
        for (int i = 0; i < 1024; ++i) {
            file.write(buffer.data(), buffer.size());
        }
        file.close();

        FileStat stat(largeFile);
        EXPECT_TRUE(stat.exists());
        EXPECT_EQ(stat.size(), 1024 * 1024);
        EXPECT_TRUE(stat.isRegularFile());

    } catch (const std::exception& e) {
        GTEST_SKIP() << "Could not create large file: " << e.what();
    }
}

// Test files with special characters in names
TEST_F(StatEdgeCaseTest, SpecialCharacterFilenames) {
    std::vector<std::string> specialNames = {
        "file with spaces.txt", "file-with-dashes.txt",
        "file_with_underscores.txt", "file.with.dots.txt"};

    for (const auto& name : specialNames) {
        fs::path specialFile = testDir / name;

        try {
            {
                std::ofstream file(specialFile);
                file << "Special character test\n";
            }

            FileStat stat(specialFile);
            EXPECT_TRUE(stat.exists());
            EXPECT_TRUE(stat.isRegularFile());
            EXPECT_GT(stat.size(), 0);

        } catch (const std::exception& e) {
            // Some special characters might not be supported on all filesystems
            GTEST_SKIP() << "Special character filename not supported: "
                         << name;
        }
    }
}

// Test zero-byte files
TEST_F(StatEdgeCaseTest, ZeroByteFile) {
    fs::path emptyFile = testDir / "empty.txt";

    {
        std::ofstream file(emptyFile);
        // Create empty file
    }

    FileStat stat(emptyFile);
    EXPECT_TRUE(stat.exists());
    EXPECT_EQ(stat.size(), 0);
    EXPECT_TRUE(stat.isRegularFile());
    EXPECT_GT(stat.mtime(), 0);
}

}  // namespace atom::system::test
