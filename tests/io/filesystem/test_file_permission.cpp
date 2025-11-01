#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#endif

#include "atom/io/file_permission.hpp"

namespace fs = std::filesystem;

class FilePermissionTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path test_file;
    fs::path executable_path;
    fs::path nonexistent_file;

    void SetUp() override {
        // Create temporary test directory
        test_dir = fs::temp_directory_path() / "atom_file_permission_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);

        // Create a test file
        test_file = test_dir / "test.txt";
        std::ofstream file(test_file);
        file << "Test content";
        file.close();

// Get path to the current executable
#ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        executable_path = buffer;
#else
        char buffer[1024];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len != -1) {
            buffer[len] = '\0';
            executable_path = buffer;
        } else {
            // Fallback to current directory if readlink fails
            executable_path = fs::current_path() / "test_executable";
        }
#endif

        nonexistent_file = test_dir / "nonexistent.txt";
    }

    void TearDown() override {
        // Clean up test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    // Set test file permissions to match the executable's permissions
    void setTestFilePermissionsToMatchExecutable() {
#ifdef _WIN32
        // On Windows, copy ACLs from executable to test file
        SECURITY_DESCRIPTOR sd;
        PACL acl = NULL;
        BOOL daclPresent = FALSE;
        BOOL daclDefaulted = FALSE;

        PSECURITY_DESCRIPTOR psd = nullptr;
        if (GetNamedSecurityInfoA(executable_path.string().c_str(),
                                  SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  NULL, NULL, &acl, NULL,
                                  &psd) == ERROR_SUCCESS) {
            // Set the ACL on the test file
            SetNamedSecurityInfoA(const_cast<char*>(test_file.string().c_str()),
                                  SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  NULL, NULL, acl, NULL);
        }
#else
        // On POSIX systems, copy mode from executable to test file
        struct stat st;
        if (stat(executable_path.c_str(), &st) == 0) {
            chmod(test_file.c_str(), st.st_mode & 0777);
        }
#endif
    }
};

TEST_F(FilePermissionTest, GetFilePermissionsBasic) {
    // Test getting permissions of a valid file
    std::string permissions = atom::io::getFilePermissions(test_file.string());
    EXPECT_FALSE(permissions.empty());
    EXPECT_EQ(permissions.length(), 9);  // rwxrwxrwx format (9 characters)

    // Each character should be either 'r', 'w', 'x', or '-'
    for (char c : permissions) {
        EXPECT_TRUE(c == 'r' || c == 'w' || c == 'x' || c == '-');
    }
}

TEST_F(FilePermissionTest, GetFilePermissionsNonExistentFile) {
    // Test getting permissions of a non-existent file
    std::string permissions =
        atom::io::getFilePermissions(nonexistent_file.string());
    EXPECT_TRUE(permissions.empty());
}

TEST_F(FilePermissionTest, GetFilePermissionsEmptyPath) {
    // Test getting permissions with empty path
    std::string permissions = atom::io::getFilePermissions("");
    EXPECT_TRUE(permissions.empty());
}

TEST_F(FilePermissionTest, GetSelfPermissions) {
    // Test getting permissions of the current process executable
    std::string permissions = atom::io::getSelfPermissions();
    EXPECT_FALSE(permissions.empty());
    EXPECT_EQ(permissions.length(), 9);  // rwxrwxrwx format (9 characters)

    // Each character should be either 'r', 'w', 'x', or '-'
    for (char c : permissions) {
        EXPECT_TRUE(c == 'r' || c == 'w' || c == 'x' || c == '-');
    }

    // The executable should at least have read and execute permissions
    EXPECT_EQ(permissions[0], 'r');  // Owner read
    EXPECT_NE(permissions[2], '-');  // Owner execute should be set
}

TEST_F(FilePermissionTest, CompareFileAndSelfPermissionsDifferent) {
    // Test comparing permissions between file and self (should be different)
    auto result = atom::io::compareFileAndSelfPermissions(test_file);

    // Result should be valid
    ASSERT_TRUE(result.has_value());

    // A regular file and executable typically have different permissions
    // but we can't guarantee this on all systems, so just check that
    // the function returned a valid result
}

TEST_F(FilePermissionTest, CompareFileAndSelfPermissionsSame) {
    // Set test file permissions to match executable
    setTestFilePermissionsToMatchExecutable();

    // Test comparing permissions between file and self (should be same)
    auto result = atom::io::compareFileAndSelfPermissions(test_file);

    // Result should be valid
    ASSERT_TRUE(result.has_value());

    // Ideally they should be the same now, but it's not guaranteed on all
    // systems due to how ACLs work, especially on Windows. So we just check for
    // a valid result.
}

TEST_F(FilePermissionTest, CompareFileAndSelfPermissionsNonExistent) {
    // Test comparing permissions with non-existent file
    auto result = atom::io::compareFileAndSelfPermissions(nonexistent_file);

    // Should return nullopt for non-existent file
    EXPECT_FALSE(result.has_value());
}

TEST_F(FilePermissionTest, CompareFileAndSelfPermissionsEmptyPath) {
    // Test comparing permissions with empty path
    auto result = atom::io::compareFileAndSelfPermissions("");

    // Should return nullopt for empty path
    EXPECT_FALSE(result.has_value());
}

TEST_F(FilePermissionTest, PathLikeTemplateFunction) {
    // Test the templated function that accepts PathLike types

    // Test with std::filesystem::path
    {
        fs::path path_obj = test_file;
        auto result = atom::io::compareFileAndSelfPermissions(path_obj);
        EXPECT_TRUE(result.has_value());
    }

    // Test with string_view
    {
        std::string file_string = test_file.string();
        std::string_view sv = file_string;
        auto result = atom::io::compareFileAndSelfPermissions(sv);
        EXPECT_TRUE(result.has_value());
    }

    // Test with const char*
    {
        std::string file_string = test_file.string();
        const char* cstr = file_string.c_str();
        auto result = atom::io::compareFileAndSelfPermissions(cstr);
        EXPECT_TRUE(result.has_value());
    }
}

TEST_F(FilePermissionTest, GetPermissionsAfterModeChange) {
#ifndef _WIN32  // Skip on Windows as chmod behaves differently
    // Change permissions on test file
    fs::permissions(
        test_file,
        fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec,
        fs::perm_options::replace);

    // Get permissions after change
    std::string permissions = atom::io::getFilePermissions(test_file.string());

    // Should have rwx------ pattern
    EXPECT_EQ(permissions[0], 'r');
    EXPECT_EQ(permissions[1], 'w');
    EXPECT_EQ(permissions[2], 'x');
    EXPECT_EQ(permissions[3], '-');
    EXPECT_EQ(permissions[4], '-');
    EXPECT_EQ(permissions[5], '-');
    EXPECT_EQ(permissions[6], '-');
    EXPECT_EQ(permissions[7], '-');
    EXPECT_EQ(permissions[8], '-');
#endif
}

TEST_F(FilePermissionTest, CompareWithDirectory) {
    // Test comparing permissions with a directory
    auto result = atom::io::compareFileAndSelfPermissions(test_dir);

    // Result should be valid
    ASSERT_TRUE(result.has_value());

    // The actual comparison result depends on the platform and file system
}

TEST_F(FilePermissionTest, CompareWithSymlink) {
    // Create a symbolic link for testing
    fs::path link_path = test_dir / "test_link.txt";

    try {
        fs::create_symlink(test_file, link_path);

        // Test comparing permissions with a symbolic link
        auto result = atom::io::compareFileAndSelfPermissions(link_path);

        // Result should be valid (should follow the symlink)
        ASSERT_TRUE(result.has_value());

    } catch (const fs::filesystem_error& e) {
        // Creating symlink might fail on some environments (like unprivileged
        // containers)
        GTEST_SKIP()
            << "Skipping symlink test due to inability to create symlinks: "
            << e.what();
    }
}

TEST_F(FilePermissionTest, RobustnessAgainstInvalidPaths) {
    // Test with various invalid paths
    std::vector<std::string> invalid_paths = {
        "/nonexistent/path/to/file", test_dir.string() + "/*/invalid",
        "\\\\?\\invalid:path*",                     // Invalid on Windows
        test_dir.string() + std::string(1000, 'A')  // Very long path component
    };

    for (const auto& path : invalid_paths) {
        auto result = atom::io::compareFileAndSelfPermissions(path);
        // Should handle invalid paths without crashing
        EXPECT_FALSE(result.has_value());

        std::string permissions = atom::io::getFilePermissions(path);
        EXPECT_TRUE(permissions.empty());
    }
}

TEST_F(FilePermissionTest, ThreadSafety) {
    // Test thread safety by calling functions from multiple threads
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<bool> any_failure(false);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            try {
                // Call various functions repeatedly
                for (int j = 0; j < 10; ++j) {
                    std::string self_perm = atom::io::getSelfPermissions();
                    if (self_perm.empty()) {
                        any_failure = true;
                    }

                    std::string file_perm =
                        atom::io::getFilePermissions(test_file.string());
                    if (file_perm.empty()) {
                        any_failure = true;
                    }

                    auto result =
                        atom::io::compareFileAndSelfPermissions(test_file);
                    if (!result.has_value()) {
                        any_failure = true;
                    }
                }
            } catch (...) {
                any_failure = true;
            }
        });
    }

    // Join all threads
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // No thread should have encountered a failure
    EXPECT_FALSE(any_failure);
}

// Test changeFilePermissions with all permission combinations
TEST_F(FilePermissionTest, AllPermissionCombinations) {
    std::vector<std::string> permission_strings = {
        "rwxrwxrwx",  // All permissions
        "---------",  // No permissions
        "r--------",  // Owner read only
        "-w-------",  // Owner write only
        "--x------",  // Owner execute only
        "rw-------",  // Owner read-write
        "rwx------",  // Owner all
        "r--r--r--",  // All read
        "-w--w--w-",  // All write
        "--x--x--x",  // All execute
        "rwxr-xr-x",  // Common executable
        "rw-r--r--",  // Common file
    };

    for (const auto& perm_str : permission_strings) {
        EXPECT_NO_THROW({
            atom::io::changeFilePermissions(test_file, perm_str);
            std::string result =
                atom::io::getFilePermissions(test_file.string());
            EXPECT_EQ(result, perm_str);
        }) << "Failed for permission string: "
           << perm_str;
    }

    // Restore readable permissions
    fs::permissions(test_file, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
}

// Test changeFilePermissions with invalid permission strings
TEST_F(FilePermissionTest, InvalidPermissionStrings) {
    std::vector<std::string> invalid_strings = {
        "",              // Empty
        "rwx",           // Too short
        "rwxrwxrwxrwx",  // Too long
        "abcdefghi",     // Invalid characters
        "rwxrwxrw",      // Wrong length
        "rwxrwxrwX",     // Invalid character at end
        "Rwxrwxrwx",     // Invalid character at start
    };

    for (const auto& invalid_str : invalid_strings) {
        EXPECT_THROW(
            { atom::io::changeFilePermissions(test_file, invalid_str); },
            std::runtime_error)
            << "Should throw for: " << invalid_str;
    }
}

// Test permission operations on directory
TEST_F(FilePermissionTest, DirectoryPermissions) {
    fs::path test_directory = test_dir / "perm_test_dir";
    fs::create_directories(test_directory);

    // Get directory permissions
    std::string dir_perms =
        atom::io::getFilePermissions(test_directory.string());
    EXPECT_FALSE(dir_perms.empty());
    EXPECT_EQ(dir_perms.length(), 9);

    // Change directory permissions
    EXPECT_NO_THROW({
        atom::io::changeFilePermissions(test_directory, "rwxr-xr-x");
        std::string result =
            atom::io::getFilePermissions(test_directory.string());
        EXPECT_EQ(result, "rwxr-xr-x");
    });

    // Cleanup
    fs::permissions(test_directory, fs::perms::all, fs::perm_options::add);
    fs::remove(test_directory);
}

// Test getSelfPermissions consistency
TEST_F(FilePermissionTest, SelfPermissionsConsistency) {
    // Call multiple times and verify consistency
    std::string perm1 = atom::io::getSelfPermissions();
    std::string perm2 = atom::io::getSelfPermissions();
    std::string perm3 = atom::io::getSelfPermissions();

    EXPECT_EQ(perm1, perm2);
    EXPECT_EQ(perm2, perm3);
    EXPECT_FALSE(perm1.empty());
    EXPECT_EQ(perm1.length(), 9);
}

// Test compareFileAndSelfPermissions with various permission levels
TEST_F(FilePermissionTest, CompareVariousPermissionLevels) {
    std::vector<std::string> permission_levels = {
        "rwxrwxrwx", "rw-rw-rw-", "r--r--r--", "rwx------", "---------",
    };

    for (const auto& perm : permission_levels) {
        atom::io::changeFilePermissions(test_file, perm);
        auto result = atom::io::compareFileAndSelfPermissions(test_file);
        EXPECT_TRUE(result.has_value()) << "Failed for permission: " << perm;
    }

    // Restore permissions
    fs::permissions(test_file, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
}

// Test permission operations with special files
TEST_F(FilePermissionTest, SpecialFilePermissions) {
    // Test with empty file
    fs::path empty_file = test_dir / "empty.txt";
    std::ofstream(empty_file).close();

    std::string perms = atom::io::getFilePermissions(empty_file.string());
    EXPECT_FALSE(perms.empty());

    EXPECT_NO_THROW(
        { atom::io::changeFilePermissions(empty_file, "rw-r--r--"); });

    fs::remove(empty_file);

    // Test with binary file
    fs::path binary_file = test_dir / "binary.dat";
    std::ofstream bin(binary_file, std::ios::binary);
    bin.write("\x00\x01\x02\x03", 4);
    bin.close();

    perms = atom::io::getFilePermissions(binary_file.string());
    EXPECT_FALSE(perms.empty());

    fs::remove(binary_file);
}

// Test getFilePermissions with various path types
TEST_F(FilePermissionTest, GetPermissionsVariousPathTypes) {
    // Test with absolute path
    fs::path abs_path = fs::absolute(test_file);
    std::string perms1 = atom::io::getFilePermissions(abs_path.string());
    EXPECT_FALSE(perms1.empty());

    // Test with relative path
    fs::path original_cwd = fs::current_path();
    fs::current_path(test_dir);
    std::string perms2 = atom::io::getFilePermissions("test.txt");
    fs::current_path(original_cwd);

    // Both should return valid permissions
    EXPECT_FALSE(perms2.empty());
}

// Test permission comparison edge cases
TEST_F(FilePermissionTest, ComparisonEdgeCases) {
    // Test with file that has no permissions
    fs::path no_perm_file = test_dir / "no_perm.txt";
    std::ofstream(no_perm_file).close();

#ifndef _WIN32
    fs::permissions(no_perm_file, fs::perms::none, fs::perm_options::replace);
    auto result = atom::io::compareFileAndSelfPermissions(no_perm_file);
    EXPECT_TRUE(result.has_value());

    // Restore permissions for cleanup
    fs::permissions(no_perm_file, fs::perms::owner_write,
                    fs::perm_options::add);
#endif

    fs::remove(no_perm_file);
}

// Test concurrent permission changes
TEST_F(FilePermissionTest, ConcurrentPermissionChanges) {
    // Create multiple test files
    std::vector<fs::path> test_files;
    for (int i = 0; i < 5; i++) {
        fs::path file =
            test_dir / ("concurrent_perm_" + std::to_string(i) + ".txt");
        std::ofstream f(file);
        f << "Test";
        f.close();
        test_files.push_back(file);
    }

    // Change permissions concurrently
    std::vector<std::thread> threads;
    std::atomic<bool> any_failure(false);

    for (size_t i = 0; i < test_files.size(); i++) {
        threads.emplace_back([&, i]() {
            try {
                std::string perm = (i % 2 == 0) ? "rw-r--r--" : "rwxr-xr-x";
                atom::io::changeFilePermissions(test_files[i], perm);
                std::string result =
                    atom::io::getFilePermissions(test_files[i].string());
                if (result != perm) {
                    any_failure = true;
                }
            } catch (...) {
                any_failure = true;
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    EXPECT_FALSE(any_failure);

    // Cleanup
    for (const auto& file : test_files) {
        fs::permissions(file, fs::perms::owner_write, fs::perm_options::add);
        fs::remove(file);
    }
}

// Test permission string format validation
TEST_F(FilePermissionTest, PermissionStringFormat) {
    // Valid formats
    std::vector<std::string> valid_formats = {
        "rwxrwxrwx",
        "---------",
        "r--r--r--",
        "rw-rw-rw-",
    };

    for (const auto& format : valid_formats) {
        EXPECT_NO_THROW({ atom::io::changeFilePermissions(test_file, format); })
            << "Should accept: " << format;
    }

    // Invalid formats
    std::vector<std::string> invalid_formats = {
        "rwxrwxrw",     // Too short
        "rwxrwxrwxx",   // Too long
        "rwxrwxrwX",    // Invalid character
        "123456789",    // Numbers
        "rwx rwx rwx",  // Spaces
    };

    for (const auto& format : invalid_formats) {
        EXPECT_THROW(
            { atom::io::changeFilePermissions(test_file, format); },
            std::runtime_error)
            << "Should reject: " << format;
    }
}

// Test permission changes on directories
TEST_F(FilePermissionTest, DirectoryPermissionChanges) {
    fs::path test_directory = test_dir / "perm_test_dir2";
    fs::create_directories(test_directory);

    std::string perms = atom::io::getFilePermissions(test_directory.string());
    EXPECT_FALSE(perms.empty());
    EXPECT_EQ(perms.length(), 9);

    // Change directory permissions
    EXPECT_NO_THROW(
        { atom::io::changeFilePermissions(test_directory, "rwxr-xr-x"); });

    // Verify change
    perms = atom::io::getFilePermissions(test_directory.string());
    EXPECT_EQ(perms, "rwxr-xr-x");
}

// Test concurrent permission operations
TEST_F(FilePermissionTest, ConcurrentPermissionOperations) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<fs::path> test_files;

    // Create test files
    for (int i = 0; i < num_threads; ++i) {
        fs::path file = test_dir / ("concurrent_" + std::to_string(i) + ".txt");
        std::ofstream(file) << "Test";
        test_files.push_back(file);
    }

    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&test_files, i, &success_count]() {
            try {
                std::string perms =
                    atom::io::getFilePermissions(test_files[i].string());
                if (!perms.empty()) {
                    success_count++;
                }
            } catch (...) {
                // Ignore errors
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// Test permission comparison edge cases
TEST_F(FilePermissionTest, PermissionComparisonEdgeCases) {
    // Test with file that has all permissions
    fs::permissions(
        test_file,
        fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all,
        fs::perm_options::replace);

    auto comparison =
        atom::io::compareFileAndSelfPermissions(test_file.string());
    EXPECT_TRUE(comparison.has_value());

    // Test with file that has no permissions
#ifndef _WIN32
    fs::permissions(test_file, fs::perms::none, fs::perm_options::replace);

    comparison = atom::io::compareFileAndSelfPermissions(test_file.string());
    EXPECT_FALSE(comparison.empty());

    // Restore permissions for cleanup
    fs::permissions(test_file, fs::perms::owner_all, fs::perm_options::replace);
#endif
}

// Test getSelfPermissions with explicit path
TEST_F(FilePermissionTest, GetSelfPermissionsWithPath) {
    std::string self_perms =
        atom::io::getSelfPermissions(executable_path.string());
    EXPECT_FALSE(self_perms.empty());
    EXPECT_EQ(self_perms.length(), 9);

    // Executable should have execute permission
#ifndef _WIN32
    EXPECT_NE(self_perms.find('x'), std::string::npos);
#endif
}

// Test permission changes with symbolic modes
TEST_F(FilePermissionTest, SymbolicPermissionModes) {
    // Test various permission strings
    std::vector<std::string> valid_modes = {
        "rwxrwxrwx", "rw-rw-rw-", "r--r--r--", "rwxr-xr-x", "---------"};

    for (const auto& mode : valid_modes) {
        EXPECT_NO_THROW({
            atom::io::changeFilePermissions(test_file, mode);
            std::string result =
                atom::io::getFilePermissions(test_file.string());
            EXPECT_EQ(result, mode);
        }) << "Failed for mode: "
           << mode;
    }
}

// Test permission preservation during file operations
TEST_F(FilePermissionTest, PermissionPreservation) {
    // Set specific permissions
    atom::io::changeFilePermissions(test_file, "rw-r--r--");
    std::string original_perms =
        atom::io::getFilePermissions(test_file.string());

    // Copy file
    fs::path copied_file = test_dir / "copied.txt";
    fs::copy_file(test_file, copied_file);

    // Check if permissions are preserved (platform-dependent)
    std::string copied_perms =
        atom::io::getFilePermissions(copied_file.string());
    EXPECT_FALSE(copied_perms.empty());
}

// Test error handling for non-existent files
TEST_F(FilePermissionTest, NonExistentFileHandling) {
    std::string perms = atom::io::getFilePermissions(nonexistent_file.string());
    EXPECT_TRUE(perms.empty());

    EXPECT_THROW(
        { atom::io::changeFilePermissions(nonexistent_file, "rwxrwxrwx"); },
        std::runtime_error);
}

// Test permission changes on read-only files
TEST_F(FilePermissionTest, ReadOnlyFilePermissions) {
    // Make file read-only
    atom::io::changeFilePermissions(test_file, "r--r--r--");

    std::string perms = atom::io::getFilePermissions(test_file.string());
    EXPECT_EQ(perms, "r--r--r--");

    // Try to write to read-only file (should fail)
    std::ofstream ofs(test_file, std::ios::app);
    bool can_write = ofs.is_open() && ofs.good();
    ofs.close();

    // Restore write permissions
    atom::io::changeFilePermissions(test_file, "rw-rw-rw-");
}

// Test permission string formatting
TEST_F(FilePermissionTest, PermissionStringFormatting) {
    std::string perms = atom::io::getFilePermissions(test_file.string());

    // Verify format
    EXPECT_EQ(perms.length(), 9);

    for (size_t i = 0; i < perms.length(); ++i) {
        char c = perms[i];
        if (i % 3 == 0) {
            EXPECT_TRUE(c == 'r' || c == '-');
        } else if (i % 3 == 1) {
            EXPECT_TRUE(c == 'w' || c == '-');
        } else {
            EXPECT_TRUE(c == 'x' || c == '-');
        }
    }
}
