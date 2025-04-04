
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fileapi.h>
#include <processenv.h>
#endif

#include "atom/system/user.hpp"

using namespace atom::system;
using namespace std::chrono_literals;

class UserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up a test environment variable
        setEnvironmentVariable("ATOM_TEST_ENV_VAR", "test_value");

        // Save original working directory to restore it later
        originalWorkingDir = getCurrentWorkingDirectory();
    }

    void TearDown() override {
// Clean up the test environment variable
#ifdef _WIN32
        SetEnvironmentVariableA("ATOM_TEST_ENV_VAR", nullptr);
#else
        unsetenv("ATOM_TEST_ENV_VAR");
#endif

        // Restore original working directory if it was changed
        if (!originalWorkingDir.empty() &&
            originalWorkingDir != getCurrentWorkingDirectory()) {
#ifdef _WIN32
            SetCurrentDirectoryA(originalWorkingDir.c_str());
#else
            chdir(originalWorkingDir.c_str());
#endif
        }
    }

    // Helper method to create a temporary directory for testing
    std::string createTempDirectory() {
        std::string tempPath;
#ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD length = GetTempPathA(MAX_PATH, buffer);
        if (length > 0) {
            tempPath = std::string(buffer, length);
            tempPath += "atom_test_user_" + std::to_string(std::rand());
            CreateDirectoryA(tempPath.c_str(), nullptr);
        }
#else
        tempPath = "/tmp/atom_test_user_" + std::to_string(std::rand());
        mkdir(tempPath.c_str(), 0755);
#endif
        return tempPath;
    }

    // Original working directory to restore after tests
    std::string originalWorkingDir;
};

// Test getUserGroups function
TEST_F(UserTest, GetUserGroups) {
    auto groups = getUserGroups();

    // There should be at least one group for the current user
    EXPECT_FALSE(groups.empty());

    // Print the groups for debugging
    std::cout << "User groups:" << std::endl;
    for (const auto& group : groups) {
        std::wcout << L"  - " << group << std::endl;
    }
}

// Test getUsername function
TEST_F(UserTest, GetUsername) {
    std::string username = getUsername();

    // Username should not be empty
    EXPECT_FALSE(username.empty());
    std::cout << "Username: " << username << std::endl;

    // Username should match the login name
    std::string login = getLogin();
    EXPECT_EQ(username, login);
}

// Test getHostname function
TEST_F(UserTest, GetHostname) {
    std::string hostname = getHostname();

    // Hostname should not be empty
    EXPECT_FALSE(hostname.empty());
    std::cout << "Hostname: " << hostname << std::endl;
}

// Test getUserId function
TEST_F(UserTest, GetUserId) {
    int userId = getUserId();

    // User ID should be a non-negative integer
    EXPECT_GE(userId, 0);
    std::cout << "User ID: " << userId << std::endl;
}

// Test getGroupId function
TEST_F(UserTest, GetGroupId) {
    int groupId = getGroupId();

    // Group ID should be a non-negative integer
    EXPECT_GE(groupId, 0);
    std::cout << "Group ID: " << groupId << std::endl;
}

// Test getHomeDirectory function
TEST_F(UserTest, GetHomeDirectory) {
    std::string homeDir = getHomeDirectory();

    // Home directory should not be empty
    EXPECT_FALSE(homeDir.empty());
    std::cout << "Home directory: " << homeDir << std::endl;

    // Home directory should exist
    EXPECT_TRUE(std::filesystem::exists(homeDir));
}

// Test getCurrentWorkingDirectory function
TEST_F(UserTest, GetCurrentWorkingDirectory) {
    std::string cwd = getCurrentWorkingDirectory();

    // Working directory should not be empty
    EXPECT_FALSE(cwd.empty());
    std::cout << "Current working directory: " << cwd << std::endl;

    // Working directory should exist
    EXPECT_TRUE(std::filesystem::exists(cwd));

    // Test changing directory and getting new working directory
    std::string tempDir = createTempDirectory();
    if (!tempDir.empty()) {
// Change to the temp directory
#ifdef _WIN32
        EXPECT_TRUE(SetCurrentDirectoryA(tempDir.c_str()));
#else
        EXPECT_EQ(chdir(tempDir.c_str()), 0);
#endif

        // Get the new working directory
        std::string newCwd = getCurrentWorkingDirectory();
        EXPECT_NE(cwd, newCwd);
        EXPECT_EQ(tempDir, newCwd);

// Clean up the temp directory
#ifdef _WIN32
        SetCurrentDirectoryA(cwd.c_str());
        RemoveDirectoryA(tempDir.c_str());
#else
        chdir(cwd.c_str());
        rmdir(tempDir.c_str());
#endif
    }
}

// Test getLoginShell function
TEST_F(UserTest, GetLoginShell) {
    std::string shell = getLoginShell();

    // Shell should not be empty
    EXPECT_FALSE(shell.empty());
    std::cout << "Login shell: " << shell << std::endl;

// Shell path should exist
#ifdef _WIN32
    // Windows shell (cmd.exe) always exists
    EXPECT_TRUE(std::filesystem::exists(shell));
#else
    // On Linux/Unix, shell path should be an actual file
    EXPECT_TRUE(std::filesystem::exists(shell));
#endif
}

#ifdef _WIN32
// Test getUserProfileDirectory function (Windows only)
TEST_F(UserTest, GetUserProfileDirectory) {
    std::string profileDir = getUserProfileDirectory();

    // Profile directory should not be empty
    EXPECT_FALSE(profileDir.empty());
    std::cout << "User profile directory: " << profileDir << std::endl;

    // Profile directory should exist
    EXPECT_TRUE(std::filesystem::exists(profileDir));

    // Profile directory should match environment variable
    std::string userprofileEnv = getEnvironmentVariable("USERPROFILE");
    EXPECT_EQ(profileDir, userprofileEnv);
}
#endif

// Test getLogin function
TEST_F(UserTest, GetLogin) {
    std::string login = getLogin();

    // Login name should not be empty
    EXPECT_FALSE(login.empty());
    std::cout << "Login name: " << login << std::endl;
}

// Test isRoot function
TEST_F(UserTest, IsRoot) {
    bool root = isRoot();

    // Just verify we get a boolean result
    // Can't reliably test the value as it depends on test execution environment
    std::cout << "Is root: " << (root ? "Yes" : "No") << std::endl;

// On Windows, this checks for admin privileges
// On Linux, this checks if UID is 0
#ifdef _WIN32
    // IsUserAnAdmin is unpredictable in a test context, just ensure it runs
    SUCCEED();
#else
    // We can verify the result on Linux by checking getuid()
    EXPECT_EQ(root, (::getuid() == 0));
#endif
}

// Test getEnvironmentVariable function
TEST_F(UserTest, GetEnvironmentVariable) {
    // Test our test environment variable
    std::string value = getEnvironmentVariable("ATOM_TEST_ENV_VAR");
    EXPECT_EQ(value, "test_value");

// Test a standard environment variable that should exist
#ifdef _WIN32
    std::string pathVar = getEnvironmentVariable("PATH");
#else
    std::string pathVar = getEnvironmentVariable("PATH");
#endif
    EXPECT_FALSE(pathVar.empty());

    // Test a non-existent environment variable
    std::string nonExistent = getEnvironmentVariable("ATOM_NON_EXISTENT_VAR");
    EXPECT_TRUE(nonExistent.empty());
}

// Test getAllEnvironmentVariables function
TEST_F(UserTest, GetAllEnvironmentVariables) {
    auto envVars = getAllEnvironmentVariables();

    // There should be at least several environment variables
    EXPECT_GT(envVars.size(), 5);

    // Our test variable should be in the map
    EXPECT_TRUE(envVars.find("ATOM_TEST_ENV_VAR") != envVars.end());
    EXPECT_EQ(envVars["ATOM_TEST_ENV_VAR"], "test_value");

// Standard variables like PATH should exist
#ifdef _WIN32
    EXPECT_TRUE(envVars.find("PATH") != envVars.end() ||
                envVars.find("Path") != envVars.end());
#else
    EXPECT_TRUE(envVars.find("PATH") != envVars.end());
#endif

    // Print a few environment variables for debugging
    int count = 0;
    std::cout << "Environment variables (first 5):" << std::endl;
    for (const auto& [name, value] : envVars) {
        if (count++ < 5) {
            std::cout << "  - " << name << "=" << value << std::endl;
        } else {
            break;
        }
    }
}

// Test setEnvironmentVariable function
TEST_F(UserTest, SetEnvironmentVariable) {
    // Set a new environment variable
    bool success =
        setEnvironmentVariable("ATOM_TEST_ENV_VAR2", "another_test_value");
    EXPECT_TRUE(success);

    // Verify it was set correctly
    std::string value = getEnvironmentVariable("ATOM_TEST_ENV_VAR2");
    EXPECT_EQ(value, "another_test_value");

    // Change an existing variable
    success = setEnvironmentVariable("ATOM_TEST_ENV_VAR", "modified_value");
    EXPECT_TRUE(success);

    // Verify it was changed
    value = getEnvironmentVariable("ATOM_TEST_ENV_VAR");
    EXPECT_EQ(value, "modified_value");

// Clean up
#ifdef _WIN32
    SetEnvironmentVariableA("ATOM_TEST_ENV_VAR2", nullptr);
#else
    unsetenv("ATOM_TEST_ENV_VAR2");
#endif
}

// Test getSystemUptime function
TEST_F(UserTest, GetSystemUptime) {
    uint64_t uptime = getSystemUptime();

    // System should have been up for at least a few seconds
    EXPECT_GT(uptime, 0);
    std::cout << "System uptime: " << uptime << " seconds" << std::endl;

    // Test that uptime increases
    std::this_thread::sleep_for(1100ms);
    uint64_t newUptime = getSystemUptime();
    EXPECT_GE(newUptime, uptime);
}

// Test getLoggedInUsers function
TEST_F(UserTest, GetLoggedInUsers) {
    auto users = getLoggedInUsers();

    // There should be at least one logged-in user (the current one)
    EXPECT_FALSE(users.empty());

    // Current username should be in the list
    std::string currentUser = getUsername();
    bool found = false;
    for (const auto& user : users) {
        if (user == currentUser) {
            found = true;
            break;
        }
    }

    // Note: This might fail if the implementation uses different naming
    // conventions so we're using EXPECT instead of ASSERT
    EXPECT_TRUE(found) << "Current user '" << currentUser
                       << "' not found in logged-in users list";

    // Print the users for debugging
    std::cout << "Logged-in users:" << std::endl;
    for (const auto& user : users) {
        std::cout << "  - " << user << std::endl;
    }
}

// Test userExists function
TEST_F(UserTest, UserExists) {
    std::string currentUser = getUsername();

    // Current user should exist
    EXPECT_TRUE(userExists(currentUser));

    // A non-existent user should return false
    EXPECT_FALSE(userExists("atom_non_existent_user_123456789"));

// Test with common system users that should exist on most systems
#ifdef _WIN32
    EXPECT_TRUE(userExists("Administrator"));
#else
    EXPECT_TRUE(userExists("root"));
#endif
}

// Test with various edge cases

// Test empty inputs
TEST_F(UserTest, EmptyInputs) {
    // Test with empty environment variable name
    std::string value = getEnvironmentVariable("");
    EXPECT_TRUE(value.empty());

    // Test setting empty environment variable name
    bool success = setEnvironmentVariable("", "value");
    EXPECT_FALSE(success);

    // Test setting environment variable with empty value
    success = setEnvironmentVariable("ATOM_TEST_EMPTY", "");
    EXPECT_TRUE(success);
    value = getEnvironmentVariable("ATOM_TEST_EMPTY");
    EXPECT_TRUE(value.empty());

// Clean up
#ifdef _WIN32
    SetEnvironmentVariableA("ATOM_TEST_EMPTY", nullptr);
#else
    unsetenv("ATOM_TEST_EMPTY");
#endif
}

// Test special characters in environment variables
TEST_F(UserTest, SpecialCharacters) {
    // Test with special characters
    std::string specialValue = "!@#$%^&*()_+{}[]|\\:;\"'<>,.?/";
    bool success = setEnvironmentVariable("ATOM_TEST_SPECIAL", specialValue);
    EXPECT_TRUE(success);

    std::string value = getEnvironmentVariable("ATOM_TEST_SPECIAL");
    EXPECT_EQ(value, specialValue);

// Clean up
#ifdef _WIN32
    SetEnvironmentVariableA("ATOM_TEST_SPECIAL", nullptr);
#else
    unsetenv("ATOM_TEST_SPECIAL");
#endif
}

// Test long environment variable values
TEST_F(UserTest, LongEnvironmentVariables) {
    // Create a long string (10KB)
    std::string longValue(10240, 'A');

    // Set the long environment variable
    bool success = setEnvironmentVariable("ATOM_TEST_LONG", longValue);

    // This might fail on some systems due to size limitations
    if (success) {
        std::string value = getEnvironmentVariable("ATOM_TEST_LONG");
        EXPECT_EQ(value.length(), longValue.length());

// Clean up
#ifdef _WIN32
        SetEnvironmentVariableA("ATOM_TEST_LONG", nullptr);
#else
        unsetenv("ATOM_TEST_LONG");
#endif
    } else {
        // If it fails, that's acceptable due to system limitations
        SUCCEED();
    }
}

// Test user group info
TEST_F(UserTest, UserGroupInfo) {
    // Get user ID and group ID
    int uid = getUserId();
    int gid = getGroupId();

    // Both should be non-negative
    EXPECT_GE(uid, 0);
    EXPECT_GE(gid, 0);

    // Get user groups
    auto groups = getUserGroups();

    // There should be at least one group
    EXPECT_FALSE(groups.empty());

    // Print user ID, group ID, and groups for manual verification
    std::cout << "User ID: " << uid << std::endl;
    std::cout << "Group ID: " << gid << std::endl;
    std::cout << "User groups:" << std::endl;
    for (const auto& group : groups) {
        std::wcout << L"  - " << group << std::endl;
    }
}

// Test path-related functions
TEST_F(UserTest, PathFunctions) {
    std::string homeDir = getHomeDirectory();
    std::string cwd = getCurrentWorkingDirectory();

    // Both should not be empty
    EXPECT_FALSE(homeDir.empty());
    EXPECT_FALSE(cwd.empty());

    // Both should exist
    EXPECT_TRUE(std::filesystem::exists(homeDir));
    EXPECT_TRUE(std::filesystem::exists(cwd));

    // Print for manual verification
    std::cout << "Home directory: " << homeDir << std::endl;
    std::cout << "Current working directory: " << cwd << std::endl;
}

// Test for consistent results
TEST_F(UserTest, ConsistentResults) {
    // Test that functions return consistent results when called multiple times
    std::string username1 = getUsername();
    std::string username2 = getUsername();
    EXPECT_EQ(username1, username2);

    std::string hostname1 = getHostname();
    std::string hostname2 = getHostname();
    EXPECT_EQ(hostname1, hostname2);

    int uid1 = getUserId();
    int uid2 = getUserId();
    EXPECT_EQ(uid1, uid2);

    int gid1 = getGroupId();
    int gid2 = getGroupId();
    EXPECT_EQ(gid1, gid2);

    std::string home1 = getHomeDirectory();
    std::string home2 = getHomeDirectory();
    EXPECT_EQ(home1, home2);
}

// Test for cross-function consistency
TEST_F(UserTest, CrossFunctionConsistency) {
    // Username should be the same as login
    std::string username = getUsername();
    std::string login = getLogin();
    EXPECT_EQ(username, login);

    // Current user should exist
    EXPECT_TRUE(userExists(username));

    // Current user should be in the logged-in users list
    auto loggedInUsers = getLoggedInUsers();
    bool found = false;
    for (const auto& user : loggedInUsers) {
        if (user == username) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
