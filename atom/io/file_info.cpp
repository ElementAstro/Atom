#include "file_info.hpp"
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <Aclapi.h>
#include <sddl.h>
#include <windows.h>
#else
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/stat.h>
#endif

#include "atom/error/exception.hpp"

namespace atom::io {
auto getFileInfo(const fs::path& filePath) -> FileInfo {
    FileInfo info;

    // Check if the file exists
    if (!fs::exists(filePath)) {
        THROW_FAIL_TO_READ_FILE("File does not exist: " + filePath.string());
    }

    // Populate basic information
    info.filePath = fs::absolute(filePath).string();
    info.fileName = filePath.filename().string();
    info.extension = filePath.extension().string();
    info.fileSize = fs::is_regular_file(filePath) ? fs::file_size(filePath) : 0;

    // Determine file type
    if (fs::is_directory(filePath)) {
        info.fileType = "Directory";
    } else if (fs::is_regular_file(filePath)) {
        info.fileType = "Regular file";
    } else if (fs::is_symlink(filePath)) {
        info.fileType = "Symbolic link";
#ifdef _WIN32
        // Windows does not provide a straightforward way to get symlink targets
        info.symlinkTarget = "Unavailable on Windows";
#else
        info.symlinkTarget = fs::read_symlink(filePath).string();
#endif
    } else {
        info.fileType = "Other";
    }

    // Retrieve last modification time
    auto ftime = fs::last_write_time(filePath);
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    std::time_t modifiedTime = std::chrono::system_clock::to_time_t(sctp);
    info.lastModifiedTime = std::ctime(&modifiedTime);
    info.lastModifiedTime.pop_back();  // Remove the newline character

    // Retrieve last access time and creation time
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(filePath.wstring().c_str(), GetFileExInfoStandard,
                             &fileInfo)) {
        // Convert FILETIME to SYSTEMTIME
        SYSTEMTIME sysTime;
        FILETIME creationTime, accessTime, writeTime;
        creationTime = fileInfo.ftCreationTime;
        accessTime = fileInfo.ftLastAccessTime;
        writeTime = fileInfo.ftLastWriteTime;

        // Creation Time
        if (FileTimeToSystemTime(&creationTime, &sysTime)) {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                     sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                     sysTime.wMinute, sysTime.wSecond);
            info.creationTime = buffer;
        } else {
            info.creationTime = "Unavailable";
        }

        // Last Access Time
        if (FileTimeToSystemTime(&accessTime, &sysTime)) {
            char buffer[100];
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                     sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                     sysTime.wMinute, sysTime.wSecond);
            info.lastAccessTime = buffer;
        } else {
            info.lastAccessTime = "Unavailable";
        }

        // Owner Information
        // Note: Retrieving file owner requires additional privileges and
        // complex code.
        info.owner = "Owner retrieval not implemented.";
    } else {
        info.creationTime = "Unavailable";
        info.lastAccessTime = "Unavailable";
        info.owner = "Unavailable";
    }
#else
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        // Creation Time (using birth time if available, otherwise use status
        // change time)
#ifdef __APPLE__
        auto creationTime = fileStat.st_birthtimespec.tv_sec;
#else
        auto creationTime = fileStat.st_ctim.tv_sec;
#endif
        info.creationTime = std::ctime(&creationTime);
        info.creationTime.pop_back();  // Remove the newline character

        // Last Access Time
        auto accessTime = fileStat.st_atim.tv_sec;
        info.lastAccessTime = std::ctime(&accessTime);
        info.lastAccessTime.pop_back();  // Remove the newline character

        // Owner Information
        struct passwd* pw = getpwuid(fileStat.st_uid);
        if (pw) {
            info.owner = pw->pw_name;
        } else {
            info.owner = "Unavailable";
        }

        // Group Information
        struct group* gr = getgrgid(fileStat.st_gid);
        if (gr != nullptr) {
            info.group = gr->gr_name;
        } else {
            info.group = "Unavailable";
        }
    } else {
        info.creationTime = "Unavailable";
        info.lastAccessTime = "Unavailable";
        info.owner = "Unavailable";
        info.group = "Unavailable";
    }
#endif

    // Retrieve file permissions
    try {
        fs::perms p = fs::status(filePath).permissions();
        std::string perms;
        perms += ((p & fs::perms::owner_read) != fs::perms::none) ? "r" : "-";
        perms += ((p & fs::perms::owner_write) != fs::perms::none) ? "w" : "-";
        perms += ((p & fs::perms::owner_exec) != fs::perms::none) ? "x" : "-";
        perms += ((p & fs::perms::group_read) != fs::perms::none) ? "r" : "-";
        perms += ((p & fs::perms::group_write) != fs::perms::none) ? "w" : "-";
        perms += ((p & fs::perms::group_exec) != fs::perms::none) ? "x" : "-";
        perms += ((p & fs::perms::others_read) != fs::perms::none) ? "r" : "-";
        perms += ((p & fs::perms::others_write) != fs::perms::none) ? "w" : "-";
        perms += ((p & fs::perms::others_exec) != fs::perms::none) ? "x" : "-";
        info.permissions = perms;
    } catch (const fs::filesystem_error& e) {
        info.permissions = "Unavailable";
    }

    // Determine if the file is hidden
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(filePath.wstring().c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
        info.isHidden = true;
    } else {
        info.isHidden = false;
    }
#else
    info.isHidden = (filePath.filename().string().front() == '.');
#endif

    return info;
}

void printFileInfo(const FileInfo& info) {
    std::cout << "File Path: " << info.filePath << std::endl;
    std::cout << "File Name: " << info.fileName << std::endl;
    std::cout << "Extension: " << info.extension << std::endl;
    std::cout << "File Size: " << info.fileSize << " bytes" << std::endl;
    std::cout << "File Type: " << info.fileType << std::endl;
    std::cout << "Creation Time: " << info.creationTime << std::endl;
    std::cout << "Last Modified Time: " << info.lastModifiedTime << std::endl;
    std::cout << "Last Access Time: " << info.lastAccessTime << std::endl;
    std::cout << "Permissions: " << info.permissions << std::endl;
    std::cout << "Is Hidden: " << (info.isHidden ? "Yes" : "No") << std::endl;
#ifdef _WIN32
    std::cout << "Owner: " << info.owner << std::endl;
#else
    std::cout << "Owner: " << info.owner << std::endl;
    std::cout << "Group: " << info.group << std::endl;
    if (!info.symlinkTarget.empty()) {
        std::cout << "Symlink Target: " << info.symlinkTarget << std::endl;
    }
#endif
}
}  // namespace atom::io
