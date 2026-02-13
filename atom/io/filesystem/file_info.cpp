#include "file_info.hpp"

#include <array>
#include <chrono>
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

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace atom::io {

using atom::containers::String;

auto getFileInfo(const fs::path& filePath) -> FileInfo {
    try {
        if (filePath.empty()) {
            spdlog::error("Empty file path provided");
            throw std::invalid_argument("Empty file path provided");
        }

        spdlog::debug("Getting file info for: {}", filePath.string());

        FileInfo info;

        // Edge case: Use error_code version to handle permission issues
        // gracefully
        std::error_code ec;
        if (!fs::exists(filePath, ec) || ec) {
            spdlog::error(
                "File does not exist or is inaccessible: {} (error: {})",
                filePath.string(), ec ? ec.message() : "unknown");
            throw std::runtime_error(
                "File does not exist or is inaccessible: " + filePath.string() +
                " (error: " + (ec ? ec.message() : "unknown") + ")");
        }

        // Edge case: Handle absolute path conversion errors
        auto abs_path = fs::absolute(filePath, ec);
        info.filePath = String(ec ? filePath.string() : abs_path.string());

        info.fileName = String(filePath.filename().string());
        info.extension = String(filePath.extension().string());

        // Edge case: Handle file size calculation with error checking
        if (fs::is_regular_file(filePath, ec) && !ec) {
            auto file_size = fs::file_size(filePath, ec);
            info.fileSize = ec ? 0 : file_size;
        } else {
            info.fileSize =
                0;  // Directories and special files don't have meaningful size
        }

        if (fs::is_directory(filePath)) {
            info.fileType = "Directory";
        } else if (fs::is_regular_file(filePath)) {
            info.fileType = "Regular file";
        } else if (fs::is_symlink(filePath)) {
            info.fileType = "Symbolic link";
#ifndef _WIN32
            info.symlinkTarget = String(fs::read_symlink(filePath).string());
#endif
        } else {
            info.fileType = "Other";
        }

        auto ftime = fs::last_write_time(filePath);
        auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        std::time_t modifiedTime = std::chrono::system_clock::to_time_t(sctp);
        info.lastModifiedTime = std::ctime(&modifiedTime);
        if (!info.lastModifiedTime.empty() &&
            info.lastModifiedTime.back() == '\n') {
            info.lastModifiedTime.pop_back();
        }

#ifdef _WIN32
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
                        int written = snprintf(
                            buffer, sizeof(buffer),
                            "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear,
                            sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                            sysTime.wMinute, sysTime.wSecond);
                        if (written > 0 &&
                            static_cast<size_t>(written) < sizeof(buffer)) {
                            return std::string(buffer);
                        }
                    }
                    return "Unavailable";
                };

                info.creationTime = String(convertTime(creationTime));
                info.lastAccessTime = String(convertTime(accessTime));

                // Get file owner information on Windows
                auto getOwner = [&filePath]() -> std::string {
                    try {
                        PSID pSidOwner = nullptr;
                        PSECURITY_DESCRIPTOR pSD = nullptr;

                        DWORD dwRtnCode = GetNamedSecurityInfoA(
                            filePath.string().c_str(), SE_FILE_OBJECT,
                            OWNER_SECURITY_INFORMATION, &pSidOwner, nullptr,
                            nullptr, nullptr, &pSD);

                        if (dwRtnCode != ERROR_SUCCESS) {
                            return "Unknown";
                        }

                        char szAccountName[256];
                        char szDomainName[256];
                        DWORD dwAcctName = sizeof(szAccountName);
                        DWORD dwDomainName = sizeof(szDomainName);
                        SID_NAME_USE eUse = SidTypeUnknown;

                        if (LookupAccountSidA(nullptr, pSidOwner, szAccountName,
                                              &dwAcctName, szDomainName,
                                              &dwDomainName, &eUse)) {
                            std::string result = std::string(szDomainName) +
                                                 "\\" +
                                                 std::string(szAccountName);
                            if (pSD)
                                LocalFree(pSD);
                            return result;
                        }

                        if (pSD)
                            LocalFree(pSD);
                        return "Unknown";
                    } catch (...) {
                        return "Unknown";
                    }
                };

                info.owner = getOwner();
                spdlog::debug("Retrieved Windows file times successfully");
            } else {
                spdlog::warn("Failed to get Windows file attributes for: {}",
                             filePath.string());
                info.creationTime = "Unavailable";
                info.lastAccessTime = "Unavailable";
                info.owner = "Unavailable";
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception while getting Windows file info: {}",
                          e.what());
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
        }
#else
        struct stat fileStat;
        if (stat(filePath.string().c_str(), &fileStat) == 0) {
            auto getCreationTime = [&fileStat]() -> std::string {
#ifdef __APPLE__
                auto time_val = fileStat.st_birthtimespec.tv_sec;
#elif defined(__linux__)
                auto time_val = fileStat.st_ctim.tv_sec;
#else
                auto time_val = fileStat.st_ctime;
#endif
                std::string s = std::ctime(&time_val);
                if (!s.empty() && s.back() == '\n') {
                    s.pop_back();
                }
                return s;
            };

            auto getAccessTime = [&fileStat]() -> std::string {
#ifdef __linux__
                auto time_val = fileStat.st_atim.tv_sec;
#else
                auto time_val = fileStat.st_atime;
#endif
                std::string s = std::ctime(&time_val);
                if (!s.empty() && s.back() == '\n') {
                    s.pop_back();
                }
                return s;
            };

            std::future<std::string> futureCreation =
                std::async(std::launch::async, getCreationTime);
            std::future<std::string> futureAccess =
                std::async(std::launch::async, getAccessTime);

            info.creationTime = String(futureCreation.get());
            info.lastAccessTime = String(futureAccess.get());

            struct passwd* pw = getpwuid(fileStat.st_uid);
            info.owner = pw ? String(pw->pw_name) : String("Unavailable");
            struct group* gr = getgrgid(fileStat.st_gid);
            info.group = gr ? String(gr->gr_name) : String("Unavailable");

            spdlog::debug(
                "Retrieved POSIX file times and ownership successfully");
        } else {
            spdlog::warn("Failed to get file stat for: {}", filePath.string());
            info.creationTime = "Unavailable";
            info.lastAccessTime = "Unavailable";
            info.owner = "Unavailable";
            info.group = "Unavailable";
        }
#endif

        try {
            fs::perms p = fs::status(filePath).permissions();
            auto buildPermString = [p]() noexcept -> std::string {
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
                perms_str.reserve(9);
                for (const auto& [mask, ch] : permMapping) {
                    perms_str.push_back(((p & mask) != fs::perms::none) ? ch
                                                                        : '-');
                }
                return perms_str;
            };
            info.permissions = String(buildPermString());
        } catch (const fs::filesystem_error& e) {
            spdlog::warn("Failed to get file permissions: {}", e.what());
            info.permissions = "Unavailable";
        }

#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(filePath.wstring().c_str());
        info.isHidden = (attrs != INVALID_FILE_ATTRIBUTES &&
                         (attrs & FILE_ATTRIBUTE_HIDDEN));
#else
        info.isHidden = (!filePath.filename().empty() &&
                         String(filePath.filename().string()).front() == '.');
#endif

        spdlog::info("Successfully retrieved file info for: {}",
                     filePath.string());
        return info;

    } catch (const std::invalid_argument&) {
        throw;
    } catch (const fs::filesystem_error& ex) {
        spdlog::error("Filesystem error for '{}': {}", filePath.string(),
                      ex.what());
        throw std::runtime_error("Filesystem error accessing file info for '" +
                                 filePath.string() + "': " + ex.what());
    } catch (const std::exception& ex) {
        spdlog::error("Exception in getFileInfo for '{}': {}",
                      filePath.string(), ex.what());
        throw std::runtime_error("getFileInfo failed for '" +
                                 filePath.string() + "': " + ex.what());
    } catch (...) {
        spdlog::error("Unknown error in getFileInfo for '{}'",
                      filePath.string());
        throw std::runtime_error("getFileInfo failed for '" +
                                 filePath.string() +
                                 "' due to an unknown error");
    }
}

}  // namespace atom::io
