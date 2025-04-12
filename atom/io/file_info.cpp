#include "file_info.hpp"

#include <array>
#include <chrono>
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

// Use type alias from high_performance.hpp within the implementation too
using atom::containers::String;

auto getFileInfo(const fs::path& filePath) -> FileInfo {
    try {
        // Input validation
        if (filePath.empty()) {
            throw std::invalid_argument("Empty file path provided.");
        }

        FileInfo info;

        // Check if the file exists
        if (!fs::exists(filePath)) {
            // Use String for exception message construction if desired,
            // though std::string concatenation is often simpler here.
            THROW_FAIL_TO_READ_FILE("File does not exist: " +
                                    filePath.string());
        }

        // Populate basic information using String
        info.filePath = String(fs::absolute(filePath).string());
        info.fileName = String(filePath.filename().string());
        info.extension = String(filePath.extension().string());
        info.fileSize =
            fs::is_regular_file(filePath) ? fs::file_size(filePath) : 0;

        // Determine file type and symbolic link target
        if (fs::is_directory(filePath)) {
            info.fileType =
                "Directory";  // Implicit conversion from const char*
        } else if (fs::is_regular_file(filePath)) {
            info.fileType = "Regular file";
        } else if (fs::is_symlink(filePath)) {
            info.fileType = "Symbolic link";
#ifndef _WIN32
            // Construct String explicitly from std::string
            info.symlinkTarget = String(fs::read_symlink(filePath).string());
#endif
        } else {
            info.fileType = "Other";
        }

        // Retrieve last modification time
        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        std::time_t modifiedTime = std::chrono::system_clock::to_time_t(sctp);
        // std::ctime returns char*, String constructor handles this
        info.lastModifiedTime = std::ctime(&modifiedTime);
        if (!info.lastModifiedTime.empty() &&
            info.lastModifiedTime.back() == '\n') {
            info.lastModifiedTime
                .pop_back();  // pop_back should exist for String
        }

#ifdef _WIN32
        // Windows: encapsulate file attributes retrieval in a try-catch block
        try {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExW(filePath.wstring().c_str(),
                                     GetFileExInfoStandard, &fileInfo)) {
                SYSTEMTIME sysTime;
                FILETIME creationTime = fileInfo.ftCreationTime;
                FILETIME accessTime = fileInfo.ftLastAccessTime;

                // Lambda returns std::string, assign to String (implicit
                // conversion)
                auto convertTime =
                    [&sysTime](const FILETIME& ft) noexcept -> std::string {
                    if (FileTimeToSystemTime(&ft, &sysTime)) {
                        char buffer[100];
                        // Use snprintf for safety
                        int written = snprintf(
                            buffer, sizeof(buffer),
                            "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear,
                            sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                            sysTime.wMinute, sysTime.wSecond);
                        // Check for truncation or error
                        if (written > 0 &&
                            static_cast<size_t>(written) < sizeof(buffer)) {
                            return std::string(buffer);
                        }
                    }
                    return "Unavailable";
                };

                info.creationTime = String(convertTime(creationTime));
                info.lastAccessTime = String(convertTime(accessTime));
                // Assign const char* directly
                info.owner = "Owner retrieval not implemented.";
            } else {
                info.creationTime = "Unavailable";
                info.lastAccessTime = "Unavailable";
                info.owner = "Unavailable";
            }
        } catch (...) {
            // Assign const char* directly
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
        }
#else
        // Non-Windows: Use stat and std::async for concurrent time conversion
        struct stat fileStat;
        if (stat(filePath.string().c_str(), &fileStat) == 0) {
            // Lambdas return std::string, assign to String
            auto getCreationTime = [&fileStat]() -> std::string {
#ifdef __APPLE__
                auto time_val = fileStat.st_birthtimespec.tv_sec;
#elif defined(__linux__)  // More specific check for st_ctim availability
                auto time_val = fileStat.st_ctim.tv_sec;
#else                     // Fallback or other POSIX
                auto time_val = fileStat.st_ctime;  // Standard POSIX
#endif
                std::string s = std::ctime(&time_val);
                if (!s.empty() && s.back() == '\n') {
                    s.pop_back();
                }
                return s;
            };

            auto getAccessTime = [&fileStat]() -> std::string {
#ifdef __linux__  // More specific check for st_atim availability
                auto time_val = fileStat.st_atim.tv_sec;
#else             // Fallback or other POSIX
                auto time_val = fileStat.st_atime;  // Standard POSIX
#endif
                std::string s = std::ctime(&time_val);
                if (!s.empty() && s.back() == '\n') {
                    s.pop_back();
                }
                return s;
            };

            // Launch async tasks
            std::future<std::string> futureCreation =
                std::async(std::launch::async, getCreationTime);
            std::future<std::string> futureAccess =
                std::async(std::launch::async, getAccessTime);

            // Get results and assign to String members
            info.creationTime = String(futureCreation.get());
            info.lastAccessTime = String(futureAccess.get());

            // Owner and group information (assign char* or const char*)
            struct passwd* pw = getpwuid(fileStat.st_uid);
            info.owner = pw ? String(pw->pw_name) : String("Unavailable");
            struct group* gr = getgrgid(fileStat.st_gid);
            info.group = gr ? String(gr->gr_name) : String("Unavailable");
        } else {
            // Assign const char*
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
            info.group = "Unavailable";
        }
#endif

        // Retrieve file permissions using a lambda
        try {
            fs::perms p = fs::status(filePath).permissions();
            // Use std::string internally in lambda for simplicity
            auto buildPermString = [p]() noexcept -> std::string {
                // Use constexpr array directly inside lambda if preferred
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
                std::string perms_str;
                perms_str.reserve(9);  // Pre-allocate memory
                for (const auto& [mask, ch] : permMapping) {
                    perms_str.push_back(((p & mask) != fs::perms::none) ? ch
                                                                        : '-');
                }
                return perms_str;
            };
            // Assign the std::string result to String
            info.permissions = String(buildPermString());
        } catch (const fs::filesystem_error&) {
            info.permissions = "Unavailable";  // Assign const char*
        }

        // Determine if the file is hidden
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(filePath.wstring().c_str());
        info.isHidden = (attrs != INVALID_FILE_ATTRIBUTES &&
                         (attrs & FILE_ATTRIBUTE_HIDDEN));
#else
        // Use String method if available, otherwise convert filename first
        info.isHidden = (!filePath.filename().empty() &&
                         String(filePath.filename().string()).front() == '.');
#endif

        return info;
    } catch (const std::invalid_argument& ex) {
        // Re-throw specific argument errors directly
        throw;
    } catch (const fs::filesystem_error& ex) {
        // Provide more context for filesystem errors
        throw std::runtime_error("Filesystem error accessing file info for '" +
                                 filePath.string() + "': " + ex.what());
    } catch (const std::exception& ex) {
        // Wrap other standard exceptions
        throw std::runtime_error("getFileInfo failed for '" +
                                 filePath.string() + "': " + ex.what());
    } catch (...) {
        // Catch all other unknown exceptions
        throw std::runtime_error("getFileInfo failed for '" +
                                 filePath.string() +
                                 "' due to an unknown error.");
    }
}

void printFileInfo(const FileInfo& info) {
    // Assumes String has an operator<< overload for std::ostream
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
        // Check if symlinkTarget String is not empty
        if (!info.symlinkTarget.empty()) {
            std::cout << "Symlink Target: " << info.symlinkTarget << std::endl;
        }
#endif
    } catch (const std::exception& ex) {
        // Use std::cerr for errors
        std::cerr << "printFileInfo encountered an error: " << ex.what()
                  << std::endl;
    }
}
}  // namespace atom::io
