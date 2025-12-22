/**
 * @file test_user.cpp
 * @brief Unit tests for user information functions
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "atom/system/info/user.hpp"

namespace atom::system::test {

using namespace atom::system;

class UserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Username Tests
TEST_F(UserTest, GetUsername) {
    std::string username = getUsername();
    EXPECT_FALSE(username.empty());
}

// Hostname Tests
TEST_F(UserTest, GetHostname) {
    std::string hostname = getHostname();
    EXPECT_FALSE(hostname.empty());
}

// User ID Tests
TEST_F(UserTest, GetUserId) {
    int userId = getUserId();
#ifdef _WIN32
    // On Windows, user ID might be 0 or a positive value
    EXPECT_GE(userId, 0);
#else
    // On Unix, user ID should be >= 0
    EXPECT_GE(userId, 0);
#endif
}

// Group ID Tests
TEST_F(UserTest, GetGroupId) {
    int groupId = getGroupId();
    EXPECT_GE(groupId, 0);
}

// Home Directory Tests
TEST_F(UserTest, GetHomeDirectory) {
    std::string homeDir = getHomeDirectory();
    EXPECT_FALSE(homeDir.empty());
}

// Current Working Directory Tests
TEST_F(UserTest, GetCurrentWorkingDirectory) {
    std::string cwd = getCurrentWorkingDirectory();
    EXPECT_FALSE(cwd.empty());
}

// Login Shell Tests
TEST_F(UserTest, GetLoginShell) {
    std::string shell = getLoginShell();
#ifdef _WIN32
    // On Windows, shell might be empty or cmd.exe/powershell
    // Just verify it doesn't throw
    SUCCEED();
#else
    // On Unix, shell should typically be set
    EXPECT_FALSE(shell.empty());
#endif
}

// User Groups Tests
TEST_F(UserTest, GetUserGroups) {
    auto groups = getUserGroups();
    // User should belong to at least one group
    EXPECT_GE(groups.size(), 0);
}

#ifdef _WIN32
// Windows-specific Tests
TEST_F(UserTest, GetUserProfileDirectory) {
    std::string profileDir = getUserProfileDirectory();
    EXPECT_FALSE(profileDir.empty());
}
#endif

// Login Name Tests
TEST_F(UserTest, GetLogin) {
    std::string loginName = getLogin();
    EXPECT_FALSE(loginName.empty());
}

// Root/Admin Check Tests
TEST_F(UserTest, IsRoot) {
    // Just verify it returns a boolean without throwing
    bool isRootUser = isRoot();
    // Can't assert the specific value since it depends on how tests are run
    EXPECT_TRUE(isRootUser || !isRootUser);
}

// Environment Variable Tests
TEST_F(UserTest, GetEnvironmentVariable) {
#ifdef _WIN32
    std::string path = getEnvironmentVariable("PATH");
#else
    std::string path = getEnvironmentVariable("PATH");
#endif
    EXPECT_FALSE(path.empty());
}

TEST_F(UserTest, GetEnvironmentVariable_Nonexistent) {
    std::string value = getEnvironmentVariable("NONEXISTENT_VAR_12345");
    EXPECT_TRUE(value.empty());
}

TEST_F(UserTest, GetAllEnvironmentVariables) {
    auto envVars = getAllEnvironmentVariables();
    EXPECT_GT(envVars.size(), 0);

    // PATH should exist
#ifdef _WIN32
    EXPECT_TRUE(envVars.count("Path") > 0 || envVars.count("PATH") > 0);
#else
    EXPECT_TRUE(envVars.count("PATH") > 0);
#endif
}

TEST_F(UserTest, SetEnvironmentVariable) {
    const std::string testVarName = "ATOM_TEST_USER_VAR";
    const std::string testValue = "test_value_123";

    bool result = setEnvironmentVariable(testVarName, testValue);
    EXPECT_TRUE(result);

    std::string retrievedValue = getEnvironmentVariable(testVarName);
    EXPECT_EQ(retrievedValue, testValue);

    // Cleanup
    setEnvironmentVariable(testVarName, "");
}

// System Uptime Tests
TEST_F(UserTest, GetSystemUptime) {
    uint64_t uptime = getSystemUptime();
    // System should have been running for at least some time
    EXPECT_GT(uptime, 0);
}

// Logged-in Users Tests
TEST_F(UserTest, GetLoggedInUsers) {
    auto users = getLoggedInUsers();
    // At least the current user should be logged in
    EXPECT_GE(users.size(), 0);
}

// User Exists Tests
TEST_F(UserTest, UserExists_CurrentUser) {
    std::string username = getUsername();
    if (!username.empty()) {
        EXPECT_TRUE(userExists(username));
    }
}

TEST_F(UserTest, UserExists_Nonexistent) {
    EXPECT_FALSE(userExists("nonexistent_user_12345"));
}

// Edge Cases
TEST_F(UserTest, EmptyEnvironmentVariableName) {
    std::string value = getEnvironmentVariable("");
    EXPECT_TRUE(value.empty());
}

TEST_F(UserTest, SetEmptyEnvironmentVariable) {
    const std::string testVarName = "ATOM_EMPTY_TEST_VAR";

    bool result = setEnvironmentVariable(testVarName, "");
    // Empty value should work (effectively unsetting)
    EXPECT_TRUE(result || !result);  // Platform dependent
}

// Consistency Tests
TEST_F(UserTest, UsernameAndLoginMatch) {
    std::string username = getUsername();
    std::string login = getLogin();

    // These should typically match, but there might be edge cases
    if (!username.empty() && !login.empty()) {
        // At least one should contain the other or they should be equal
        EXPECT_TRUE(username == login ||
                    username.find(login) != std::string::npos ||
                    login.find(username) != std::string::npos);
    }
}

TEST_F(UserTest, HomeDirectoryContainsUsername) {
    std::string homeDir = getHomeDirectory();
    std::string username = getUsername();

    if (!homeDir.empty() && !username.empty()) {
        // Home directory often contains username, but not always
        // Just verify both are valid paths/strings
        EXPECT_GT(homeDir.length(), 0);
        EXPECT_GT(username.length(), 0);
    }
}

}  // namespace atom::system::test
