#include "atom/system/stat.hpp"

#include <iomanip>
#include <iostream>

using namespace atom::system;

int main() {
    // Specify the file path
    fs::path filePath = "/path/to/file";

    // Create a Stat object for the specified file path
    Stat fileStat(filePath);

    // Update the file statistics
    fileStat.update();

    // Get the type of the file
    auto fileType = fileStat.type();
    std::cout << "File type: " << static_cast<int>(fileType) << std::endl;

    // Get the size of the file
    auto fileSize = fileStat.size();
    std::cout << "File size: " << fileSize << " bytes" << std::endl;

    // Get the last access time of the file
    auto accessTime = fileStat.atime();
    std::cout << "Last access time: "
              << std::put_time(std::localtime(&accessTime), "%F %T")
              << std::endl;

    // Get the last modification time of the file
    auto modificationTime = fileStat.mtime();
    std::cout << "Last modification time: "
              << std::put_time(std::localtime(&modificationTime), "%F %T")
              << std::endl;

    // Get the creation time of the file
    auto creationTime = fileStat.ctime();
    std::cout << "Creation time: "
              << std::put_time(std::localtime(&creationTime), "%F %T")
              << std::endl;

    // Get the file mode/permissions
    auto fileMode = fileStat.mode();
    std::cout << "File mode: " << std::oct << fileMode << std::dec << std::endl;

    // Get the user ID of the file owner
    auto userId = fileStat.uid();
    std::cout << "User ID: " << userId << std::endl;

    // Get the group ID of the file owner
    auto groupId = fileStat.gid();
    std::cout << "Group ID: " << groupId << std::endl;

    // Get the path of the file
    auto filePathRetrieved = fileStat.path();
    std::cout << "File path: " << filePathRetrieved << std::endl;

    return 0;
}
