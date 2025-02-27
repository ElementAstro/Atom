#include "file_info.hpp"
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>

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
    try {
        // Input validation
        if (filePath.empty()) {
            throw std::invalid_argument("Empty file path provided.");
        }

        FileInfo info;

        // Check if the file exists
        if (!fs::exists(filePath)) {
            THROW_FAIL_TO_READ_FILE("File does not exist: " +
                                    filePath.string());
        }

        // Populate basic information
        info.filePath = fs::absolute(filePath).string();
        info.fileName = filePath.filename().string();
        info.extension = filePath.extension().string();
        info.fileSize =
            fs::is_regular_file(filePath) ? fs::file_size(filePath) : 0;

        // Determine file type and symbolic link target
        if (fs::is_directory(filePath)) {
            info.fileType = "Directory";
        } else if (fs::is_regular_file(filePath)) {
            info.fileType = "Regular file";
        } else if (fs::is_symlink(filePath)) {
            info.fileType = "Symbolic link";
#ifdef _WIN32
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
        info.lastModifiedTime.pop_back();  // Remove newline

#ifdef _WIN32
        // Windows: encapsulate file attributes retrieval in a try-catch block
        try {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExW(filePath.wstring().c_str(),
                                     GetFileExInfoStandard, &fileInfo)) {
                SYSTEMTIME sysTime;
                FILETIME creationTime = fileInfo.ftCreationTime;
                FILETIME accessTime = fileInfo.ftLastAccessTime;

                auto convertTime =
                    [&sysTime](const FILETIME& ft) noexcept -> std::string {
                    if (FileTimeToSystemTime(&ft, &sysTime)) {
                        char buffer[100];
                        snprintf(buffer, sizeof(buffer),
                                 "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear,
                                 sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                                 sysTime.wMinute, sysTime.wSecond);
                        return buffer;
                    }
                    return "Unavailable";
                };

                info.creationTime = convertTime(creationTime);
                info.lastAccessTime = convertTime(accessTime);
                info.owner = "Owner retrieval not implemented.";
            } else {
                info.creationTime = "Unavailable";
                info.lastAccessTime = "Unavailable";
                info.owner = "Unavailable";
            }
        } catch (...) {
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
        }
#else
        // Non-Windows: Use stat and std::async for concurrent time conversion
        struct stat fileStat;
        if (stat(filePath.string().c_str(), &fileStat) == 0) {
            auto getCreationTime = [&fileStat]() -> std::string {
#ifdef __APPLE__
                auto time_val = fileStat.st_birthtimespec.tv_sec;
#else
                auto time_val = fileStat.st_ctim.tv_sec;
#endif
                std::string s = std::ctime(&time_val);
                s.pop_back();
                return s;
            };

            auto getAccessTime = [&fileStat]() -> std::string {
                auto time_val = fileStat.st_atim.tv_sec;
                std::string s = std::ctime(&time_val);
                s.pop_back();
                return s;
            };

            std::future<std::string> futureCreation =
                std::async(std::launch::async, getCreationTime);
            std::future<std::string> futureAccess =
                std::async(std::launch::async, getAccessTime);
            info.creationTime = futureCreation.get();
            info.lastAccessTime = futureAccess.get();

            // Owner and group information
            struct passwd* pw = getpwuid(fileStat.st_uid);
            info.owner = pw ? pw->pw_name : "Unavailable";
            struct group* gr = getgrgid(fileStat.st_gid);
            info.group = gr ? gr->gr_name : "Unavailable";
        } else {
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
            info.group = "Unavailable";
        }
#endif

        // Retrieve file permissions using a lambda and a constexpr array
        try {
            fs::perms p = fs::status(filePath).permissions();
            constexpr std::array<std::pair<fs::perms, char>, 9> permMapping{
                {{fs::perms::owner_read, 'r'},
                 {fs::perms::owner_write, 'w'},
                 {fs::perms::owner_exec, 'x'},
                 {fs::perms::group_read, 'r'},
                 {fs::perms::group_write, 'w'},
                 {fs::perms::group_exec, 'x'},
                 {fs::perms::others_read, 'r'},
                 {fs::perms::others_write, 'w'},
                 {fs::perms::others_exec, 'x'}}};
            auto buildPermString =
                [p](const auto& mapping) noexcept -> std::string {
                std::string perms;
                for (const auto& [mask, ch] : mapping)
                    perms.push_back(((p & mask) != fs::perms::none) ? ch : '-');
                return perms;
            };
            info.permissions = buildPermString(permMapping);
        } catch (const fs::filesystem_error&) {
            info.permissions = "Unavailable";
        }

        // Determine if the file is hidden
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(filePath.wstring().c_str());
        info.isHidden = (attrs != INVALID_FILE_ATTRIBUTES &&
                         (attrs & FILE_ATTRIBUTE_HIDDEN));
#else
        info.isHidden = (filePath.filename().string().front() == '.');
#endif

        return info;
    } catch (const std::exception& ex) {
        // Wrap any exception with a more descriptive message
        throw std::runtime_error("getFileInfo failed: " +
                                 std::string(ex.what()));
    }
}

void printFileInfo(const FileInfo& info) {
    try {
        std::cout << "File Path: " << info.filePath << std::endl;
        std::cout << "File Name: " << info.fileName << std::endl;
        std::cout << "Extension: " << info.extension << std::endl;
        std::cout << "File Size: " << info.fileSize << " bytes" << std::endl;
        std::cout << "File Type: " << info.fileType << std::endl;
        std::cout << "Creation Time: " << info.creationTime << std::endl;
        std::cout << "Last Modified Time: " << info.lastModifiedTime
                  << std::endl;
        std::cout << "Last Access Time: " << info.lastAccessTime << std::endl;
        std::cout << "Permissions: " << info.permissions << std::endl;
        std::cout << "Is Hidden: " << (info.isHidden ? "Yes" : "No")
                  << std::endl;
#ifdef _WIN32
        std::cout << "Owner: " << info.owner << std::endl;
#else
        std::cout << "Owner: " << info.owner << std::endl;
        std::cout << "Group: " << info.group << std::endl;
        if (!info.symlinkTarget.empty()) {
            std::cout << "Symlink Target: " << info.symlinkTarget << std::endl;
        }
#endif
    } catch (const std::exception& ex) {
        std::cerr << "printFileInfo encountered an error: " << ex.what()
                  << std::endl;
    }
}

}  // namespace atom::io
