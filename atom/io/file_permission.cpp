#include "file_permission.hpp"

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <string_view>

#ifdef ATOM_USE_BOOST
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
namespace fs = boost::filesystem;
#else
namespace fs = std::filesystem;
#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#endif

#include "atom/containers/high_performance.hpp"
#include "atom/log/loguru.hpp"

namespace atom::io {
// anonymous namespace removed as logError is no longer needed

#ifdef ATOM_USE_BOOST
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        LOG_F(ERROR, "Empty file path provided");
        return {};
    }

    try {
        boost::system::error_code ec;
        fs::path pPath(filePath);  // Use fs::path for consistency
        fs::file_status status = fs::status(pPath, ec);
        if (ec) {
            LOG_F(ERROR, "Error getting status for '{}': {}", filePath,
                  ec.message());
            return {};
        }
        fs::perms p = status.permissions();

        std::array<char, 9> permissions{};
        permissions[0] =
            ((p & fs::perms::owner_read) != fs::perms::none) ? 'r' : '-';
        permissions[1] =
            ((p & fs::perms::owner_write) != fs::perms::none) ? 'w' : '-';
        permissions[2] =
            ((p & fs::perms::owner_exec) != fs::perms::none) ? 'x' : '-';
        permissions[3] =
            ((p & fs::perms::group_read) != fs::perms::none) ? 'r' : '-';
        permissions[4] =
            ((p & fs::perms::group_write) != fs::perms::none) ? 'w' : '-';
        permissions[5] =
            ((p & fs::perms::group_exec) != fs::perms::none) ? 'x' : '-';
        permissions[6] =
            ((p & fs::perms::others_read) != fs::perms::none) ? 'r' : '-';
        permissions[7] =
            ((p & fs::perms::others_write) != fs::perms::none) ? 'w' : '-';
        permissions[8] =
            ((p & fs::perms::others_exec) != fs::perms::none) ? 'x' : '-';

        return {permissions.begin(), permissions.end()};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getFilePermissions for '{}': {}", filePath,
              e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getFilePermissions for '{}'",
              filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        boost::system::error_code ec;
        // Try to get executable path first if available
#ifdef _WIN32
        std::array<char, MAX_PATH> path{};
        if (GetModuleFileNameA(nullptr, path.data(),
                               static_cast<DWORD>(path.size())) != 0) {
            return getFilePermissions(path.data());
        } else {
            LOG_F(ERROR, "GetModuleFileNameA failed: {}", GetLastError());
        }
#else
        std::array<char, 1024> path{};
        ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (len > 0) {
            path[len] = '\0';
            return getFilePermissions(path.data());
        } else {
            LOG_F(ERROR, "readlink /proc/self/exe failed: {}", strerror(errno));
        }
#endif
        // Fallback to current path
        fs::path selfPath = fs::current_path(ec);
        if (ec) {
            LOG_F(ERROR, "Error getting current path: {}", ec.message());
            return {};
        }
        return getFilePermissions(selfPath.string());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getSelfPermissions");
        return {};
    }
}

#else  // Standard C++ implementation

#ifdef _WIN32
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        LOG_F(ERROR, "Empty file path provided");
        return {};
    }

    try {
        DWORD dwRtnCode = 0;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        std::array<char, 9> permissions;
        permissions.fill('-');  // Initialize with '-'

        // Use smart pointer with custom deleter for security descriptor
        auto psdDeleter = [](PSECURITY_DESCRIPTOR psd) {
            if (psd)
                LocalFree(static_cast<HLOCAL>(psd));
        };
        std::unique_ptr<void, decltype(psdDeleter)> psdPtr(nullptr, psdDeleter);

        PACL pDACL = nullptr;
        // Use GetNamedSecurityInfoW for better Unicode support
        std::wstring wFilePath(filePath.begin(), filePath.end());
        dwRtnCode = GetNamedSecurityInfoW(wFilePath.c_str(), SE_FILE_OBJECT,
                                          DACL_SECURITY_INFORMATION, nullptr,
                                          nullptr, &pDACL, nullptr, &pSD);

        if (dwRtnCode != ERROR_SUCCESS) {
            LOG_F(ERROR, "GetNamedSecurityInfoW error for '{}': {}", filePath,
                  dwRtnCode);
            return {};
        }

        // Update smart pointer to manage the security descriptor
        psdPtr.reset(pSD);

        if (pDACL != nullptr) {
            // Simplified permission check (may not be fully accurate for
            // complex ACLs) A more robust implementation would iterate through
            // ACEs and check SIDs For simplicity, we check if *any* ACE grants
            // these permissions.
            bool canRead = false, canWrite = false, canExec = false;

            for (DWORD i = 0; i < pDACL->AceCount; ++i) {
                ACCESS_ALLOWED_ACE* ace = nullptr;
                if (GetAce(pDACL, i, reinterpret_cast<LPVOID*>(&ace))) {
                    // Check for ACCESS_ALLOWED_ACE_TYPE, could also check DENY
                    // types
                    if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) {
                        // Check against common file access rights
                        if ((ace->Mask & FILE_GENERIC_READ) ==
                            FILE_GENERIC_READ)
                            canRead = true;
                        if ((ace->Mask & FILE_GENERIC_WRITE) ==
                            FILE_GENERIC_WRITE)
                            canWrite = true;
                        if ((ace->Mask & FILE_GENERIC_EXECUTE) ==
                            FILE_GENERIC_EXECUTE)
                            canExec = true;
                    }
                }
            }
            // Apply simplified permissions (assuming owner/group/other have
            // same basic rights)
            if (canRead)
                permissions[0] = permissions[3] = permissions[6] = 'r';
            if (canWrite)
                permissions[1] = permissions[4] = permissions[7] = 'w';
            if (canExec)
                permissions[2] = permissions[5] = permissions[8] = 'x';

        } else {
            LOG_F(WARNING,
                  "No DACL found for '{}', cannot determine permissions.",
                  filePath);
            // Return default '-' permissions or an empty string based on
            // desired behavior
            return {};  // Or return std::string(permissions.begin(),
                        // permissions.end());
        }

        return {permissions.begin(), permissions.end()};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getFilePermissions for '{}': {}", filePath,
              e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getFilePermissions for '{}'",
              filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<wchar_t, MAX_PATH> wPath{};  // Use wide characters
        if (GetModuleFileNameW(nullptr, wPath.data(),
                               static_cast<DWORD>(wPath.size())) == 0) {
            const auto error = GetLastError();
            LOG_F(ERROR, "GetModuleFileNameW error: {}", error);
            return {};
        }
        // Convert wide string path to narrow string for getFilePermissions
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wPath.data(), -1,
                                              NULL, 0, NULL, NULL);
        std::string path(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wPath.data(), -1, &path[0], size_needed,
                            NULL, NULL);
        path.pop_back();  // Remove null terminator added by WideCharToMultiByte

        return getFilePermissions(path);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getSelfPermissions");
        return {};
    }
}
#else   // POSIX systems
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        LOG_F(ERROR, "Empty file path provided");
        return {};
    }

    try {
        struct stat fileStat{};
        if (stat(filePath.data(), &fileStat) < 0) {
            // Use {} for strerror result
            LOG_F(ERROR, "stat error for '{}': {}", filePath, strerror(errno));
            return {};
        }

        std::array<char, 9> permissions;
        permissions[0] = ((fileStat.st_mode & S_IRUSR) != 0U) ? 'r' : '-';
        permissions[1] = ((fileStat.st_mode & S_IWUSR) != 0U) ? 'w' : '-';
        permissions[2] = ((fileStat.st_mode & S_IXUSR) != 0U) ? 'x' : '-';
        permissions[3] = ((fileStat.st_mode & S_IRGRP) != 0U) ? 'r' : '-';
        permissions[4] = ((fileStat.st_mode & S_IWGRP) != 0U) ? 'w' : '-';
        permissions[5] = ((fileStat.st_mode & S_IXGRP) != 0U) ? 'x' : '-';
        permissions[6] = ((fileStat.st_mode & S_IROTH) != 0U) ? 'r' : '-';
        permissions[7] = ((fileStat.st_mode & S_IWOTH) != 0U) ? 'w' : '-';
        permissions[8] = ((fileStat.st_mode & S_IXOTH) != 0U) ? 'x' : '-';

        return {permissions.begin(), permissions.end()};
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getFilePermissions for '{}': {}", filePath,
              e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getFilePermissions for '{}'",
              filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<char, 1024> path{};

        // Try to get executable path using readlink
        ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (len < 0) {
            LOG_F(ERROR, "readlink /proc/self/exe error: {}", strerror(errno));

            // Fallback to current directory if available
            try {
                auto currentPath = fs::current_path();
                LOG_F(WARNING,
                      "Falling back to current directory permissions.");
                return getFilePermissions(currentPath.string());
            } catch (const fs::filesystem_error& e) {
                LOG_F(ERROR, "Failed to get current path: {}", e.what());
                return {};
            }
        }

        path[len] = '\0';  // Ensure null-terminated
        return getFilePermissions(path.data());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in getSelfPermissions");
        return {};
    }
}
#endif  // _WIN32
#endif  // ATOM_USE_BOOST

std::optional<bool> compareFileAndSelfPermissions(
    std::string_view filePath) noexcept {
    if (filePath.empty()) {
        LOG_F(ERROR, "Empty file path provided for comparison");
        return std::nullopt;
    }

    try {
        // Check if file exists using the correct namespace
        fs::path pPath(filePath);
        if (!fs::exists(pPath)) {
            LOG_F(ERROR, "File does not exist for comparison: '{}'", filePath);
            return std::nullopt;
        }

        std::string filePermissions = getFilePermissions(filePath);
        if (filePermissions.empty()) {
            LOG_F(WARNING,
                  "Could not get permissions for file '{}' during comparison.",
                  filePath);
            return std::nullopt;  // Already logged in getFilePermissions
        }

        std::string selfPermissions = getSelfPermissions();
        if (selfPermissions.empty()) {
            LOG_F(WARNING, "Could not get self permissions during comparison.");
            return std::nullopt;  // Already logged in getSelfPermissions
        }

        // Log the permissions being compared for debugging
        VLOG_F(1, "Comparing file ('{}': {}) and self ({}) permissions.",
               filePath, filePermissions, selfPermissions);

        return filePermissions == selfPermissions;
    }
#ifdef ATOM_USE_BOOST
    catch (const boost::system::system_error&
               e) {  // Catch boost filesystem errors
        LOG_F(ERROR,
              "Boost Filesystem error in compareFileAndSelfPermissions for "
              "'{}': {}",
              filePath, e.what());
        return std::nullopt;
    }
#else
    catch (const fs::filesystem_error& e) {  // Catch std filesystem errors
        LOG_F(ERROR,
              "Filesystem error in compareFileAndSelfPermissions for '{}': {}",
              filePath, e.what());
        return std::nullopt;
    }
#endif
    catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in compareFileAndSelfPermissions for '{}': {}",
              filePath, e.what());
        return std::nullopt;
    } catch (...) {
        LOG_F(ERROR,
              "Unknown exception in compareFileAndSelfPermissions for '{}'",
              filePath);
        return std::nullopt;
    }
}

// Implementation for changing file permissions
// Ensure atom::containers::String is available
using atom::containers::String;
void changeFilePermissions(const fs::path& filePath,
                           const String& permissions) {
    // Check if filePath is empty using fs::path's empty() method
    if (filePath.empty()) {
        LOG_F(ERROR, "Empty file path provided to changeFilePermissions.");
        throw std::invalid_argument("Empty file path provided.");
    }

    try {
        if (!fs::exists(filePath)) {
            LOG_F(ERROR, "File does not exist: '{}'", filePath.string());
            throw std::runtime_error("File does not exist: " +
                                     filePath.string());
        }

        fs::perms newPerms = fs::perms::none;

        // Parse the permission string in the format "rwxrwxrwx"
        // Assuming String has length() and operator[] similar to std::string
        if (permissions.length() != 9) {
            LOG_F(ERROR,
                  "Invalid permission format: '{}'. Expected 'rwxrwxrwx'.",
                  permissions);
            throw std::invalid_argument(
                "Invalid permission format. Expected format: 'rwxrwxrwx'");
        }

        // Owner permissions
        if (permissions[0] == 'r')
            newPerms |= fs::perms::owner_read;
        if (permissions[1] == 'w')
            newPerms |= fs::perms::owner_write;
        if (permissions[2] == 'x')
            newPerms |= fs::perms::owner_exec;

        // Group permissions
        if (permissions[3] == 'r')
            newPerms |= fs::perms::group_read;
        if (permissions[4] == 'w')
            newPerms |= fs::perms::group_write;
        if (permissions[5] == 'x')
            newPerms |= fs::perms::group_exec;

        // Others permissions
        if (permissions[6] == 'r')
            newPerms |= fs::perms::others_read;
        if (permissions[7] == 'w')
            newPerms |= fs::perms::others_write;
        if (permissions[8] == 'x')
            newPerms |= fs::perms::others_exec;

        // Apply the permissions
        VLOG_F(1, "Setting permissions for '{}' to %03o", filePath.string(),
               static_cast<int>(newPerms));
        fs::permissions(filePath, newPerms, fs::perm_options::replace);
        LOG_F(INFO, "Successfully changed permissions for '{}'",
              filePath.string());

    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to change permissions for '{}': {}",
              filePath.string(), e.what());
        // Re-throw as a runtime_error for consistent exception handling upwards
        throw std::runtime_error("Failed to change permissions for '" +
                                 filePath.string() + "': " + e.what());
    } catch (const std::invalid_argument& e) {
        // Log invalid argument errors specifically if they weren't already
        // logged LOG_F(ERROR, "Invalid argument in changeFilePermissions: {}",
        // e.what()); // Already logged above
        throw;  // Re-throw original exception
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error changing file permissions for '{}': {}",
              filePath.string(), e.what());
        throw std::runtime_error("Error changing file permissions: " +
                                 std::string(e.what()));
    } catch (...) {
        LOG_F(ERROR, "Unknown error changing file permissions for '{}'",
              filePath.string());
        throw std::runtime_error(
            "Unknown error changing file permissions for '" +
            filePath.string() + "'");
    }
}

}  // namespace atom::io