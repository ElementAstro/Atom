#include "atom/system/user.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Get user groups
    auto userGroups = getUserGroups();
    std::cout << "User groups:" << std::endl;
    for (const auto& group : userGroups) {
        std::wcout << group << std::endl;
    }

    // Get user name
    std::string username = getUsername();
    std::cout << "Username: " << username << std::endl;

    // Get host name
    std::string hostname = getHostname();
    std::cout << "Hostname: " << hostname << std::endl;

    // Get user id
    int userId = getUserId();
    std::cout << "User ID: " << userId << std::endl;

    // Get group id
    int groupId = getGroupId();
    std::cout << "Group ID: " << groupId << std::endl;

    // Get user profile directory
    std::string homeDirectory = getHomeDirectory();
    std::cout << "Home directory: " << homeDirectory << std::endl;

    // Get current working directory
    std::string currentWorkingDirectory = getCurrentWorkingDirectory();
    std::cout << "Current working directory: " << currentWorkingDirectory
              << std::endl;

    // Get login shell
    std::string loginShell = getLoginShell();
    std::cout << "Login shell: " << loginShell << std::endl;

#ifdef _WIN32
    // Get user profile directory (Windows only)
    std::string userProfileDirectory = getUserProfileDirectory();
    std::cout << "User profile directory: " << userProfileDirectory
              << std::endl;
#endif

    // Get login name of the user
    std::string loginName = getLogin();
    std::cout << "Login name: " << loginName << std::endl;

    // Check whether the current user has root/administrator privileges
    bool isRootUser = isRoot();
    std::cout << "Is root user: " << std::boolalpha << isRootUser << std::endl;

    return 0;
}
