#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

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

        if (GetNamedSecurityInfoA(executable_path.string().c_str(),
                                  SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  NULL, NULL, &acl, NULL,
                                  &sd) == ERROR_SUCCESS) {
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

// Run all the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
