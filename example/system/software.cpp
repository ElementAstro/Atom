#include "atom/system/software.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Check whether the specified software is installed
    std::string softwareName = "example_software";
    bool isInstalled = checkSoftwareInstalled(softwareName);
    std::cout << "Is " << softwareName << " installed: " << std::boolalpha
              << isInstalled << std::endl;

    // Get the version of the specified application
    fs::path appPath = "/path/to/application";
    std::string appVersion = getAppVersion(appPath);
    std::cout << "Version of the application: " << appVersion << std::endl;

    // Get the path to the specified application
    fs::path appPathRetrieved = getAppPath(softwareName);
    std::cout << "Path to the application: " << appPathRetrieved << std::endl;

    // Get the permissions of the specified application
    auto permissions = getAppPermissions(appPath);
    std::cout << "Permissions of the application:" << std::endl;
    for (const auto& permission : permissions) {
        std::cout << permission << std::endl;
    }

    return 0;
}