#include "file_permission.hpp"

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
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

namespace atom::io {
namespace {
// Error logging helper that works with both Boost and standard library
template <typename... Args>
void logError(Args&&... args) noexcept {
    try {
#ifdef ATOM_USE_BOOST
        boost::log::trivial::error << std::forward<Args>(args)...;
#else
        std::cerr << "ERROR: ";
        (std::cerr << ... << std::forward<Args>(args)) << std::endl;
#endif
    } catch (...) {
        // Last resort if logging fails
        std::cerr << "Error occurred during logging" << std::endl;
    }
}
}  // anonymous namespace

#ifdef ATOM_USE_BOOST
std::string getFilePermissions(const std::string& filePath) noexcept {
    if (filePath.empty()) {
        logError("Empty file path provided");
        return {};
    }

    try {
        boost::system::error_code ec;
        fs::perms p = fs::status(filePath, ec).permissions();
        if (ec) {
            logError("Error getting permissions for ", filePath, ": ",
                     ec.message());
            return {};
        }

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
        logError("Exception in getFilePermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getFilePermissions");
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        boost::system::error_code ec;
        fs::path selfPath = fs::current_path(ec);
        if (ec) {
            logError("Error getting self path: ", ec.message());
            return {};
        }

// Try to get executable path first if available
#ifdef _WIN32
        char path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH) != 0) {
            return getFilePermissions(path);
        }
#else
        char path[1024];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len > 0) {
            path[len] = '\0';
            return getFilePermissions(path);
        }
#endif

        return getFilePermissions(selfPath.string());
    } catch (const std::exception& e) {
        logError("Exception in getSelfPermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getSelfPermissions");
        return {};
    }
}

#else  // Standard C++ implementation

#ifdef _WIN32
std::string getFilePermissions(const std::string& filePath) noexcept {
    if (filePath.empty()) {
        logError("Empty file path provided");
        return {};
    }

    try {
        DWORD dwRtnCode = 0;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        std::array<char, 9> permissions;
        permissions.fill('-');

        // Use smart pointer with custom deleter for security descriptor
        auto psdDeleter = [](PSECURITY_DESCRIPTOR psd) {
            if (psd)
                LocalFree(static_cast<HLOCAL>(psd));
        };
        std::unique_ptr<void, decltype(psdDeleter)> psdPtr(nullptr, psdDeleter);

        PACL pDACL = nullptr;
        dwRtnCode = GetNamedSecurityInfoA(filePath.c_str(), SE_FILE_OBJECT,
                                          DACL_SECURITY_INFORMATION, nullptr,
                                          nullptr, &pDACL, nullptr, &pSD);

        if (dwRtnCode != ERROR_SUCCESS) {
            logError("GetNamedSecurityInfoA error: ", dwRtnCode);
            return {};
        }

        // Update smart pointer to manage the security descriptor
        psdPtr.reset(pSD);

        if (pDACL != nullptr) {
            // Process ACLs in a more robust way
            for (DWORD i = 0; i < pDACL->AceCount && i < 10;
                 i++) {  // Boundary check
                ACE_HEADER* aceHeader = nullptr;
                if (GetAce(pDACL, i, reinterpret_cast<LPVOID*>(&aceHeader))) {
                    if (aceHeader &&
                        aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE) {
                        ACCESS_ALLOWED_ACE* ace =
                            reinterpret_cast<ACCESS_ALLOWED_ACE*>(aceHeader);

                        // Parse permissions
                        if (ace->Mask & GENERIC_READ)
                            permissions[0] = permissions[3] = permissions[6] =
                                'r';
                        if (ace->Mask & GENERIC_WRITE)
                            permissions[1] = permissions[4] = permissions[7] =
                                'w';
                        if (ace->Mask & GENERIC_EXECUTE)
                            permissions[2] = permissions[5] = permissions[8] =
                                'x';
                    }
                }
            }
        }

        return {permissions.begin(), permissions.end()};
    } catch (const std::exception& e) {
        logError("Exception in getFilePermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getFilePermissions");
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<char, MAX_PATH> path{};
        if (GetModuleFileNameA(nullptr, path.data(),
                               static_cast<DWORD>(path.size())) == 0) {
            const auto error = GetLastError();
            logError("GetModuleFileNameA error: ", error);
            return {};
        }
        return getFilePermissions(path.data());
    } catch (const std::exception& e) {
        logError("Exception in getSelfPermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getSelfPermissions");
        return {};
    }
}
#else   // POSIX systems
std::string getFilePermissions(const std::string& filePath) noexcept {
    if (filePath.empty()) {
        logError("Empty file path provided");
        return {};
    }

    try {
        struct stat fileStat {};
        if (stat(filePath.c_str(), &fileStat) < 0) {
            logError("stat error for ", filePath, ": ", strerror(errno));
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
        logError("Exception in getFilePermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getFilePermissions");
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<char, 1024> path{};

        // Try to get executable path using readlink
        ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (len < 0) {
            logError("readlink error: ", strerror(errno));

            // Fallback to current directory if available
            try {
                auto currentPath = fs::current_path();
                return getFilePermissions(currentPath.string());
            } catch (const fs::filesystem_error& e) {
                logError("Failed to get current path: ", e.what());
                return {};
            }
        }

        path[len] = '\0';  // Ensure null-terminated
        return getFilePermissions(path.data());
    } catch (const std::exception& e) {
        logError("Exception in getSelfPermissions: ", e.what());
        return {};
    } catch (...) {
        logError("Unknown exception in getSelfPermissions");
        return {};
    }
}
#endif  // _WIN32
#endif  // ATOM_USE_BOOST

std::optional<bool> compareFileAndSelfPermissions(
    const std::string& filePath) noexcept {
    if (filePath.empty()) {
        logError("Empty file path provided");
        return std::nullopt;
    }

    try {
        // Check if file exists
        if (!fs::exists(filePath)) {
            logError("File does not exist: ", filePath);
            return std::nullopt;
        }

        std::string filePermissions = getFilePermissions(filePath);
        if (filePermissions.empty()) {
            return std::nullopt;
        }

        std::string selfPermissions = getSelfPermissions();
        if (selfPermissions.empty()) {
            return std::nullopt;
        }

        // Use ranges and algorithms for comparison when more complex logic is
        // needed
        return filePermissions == selfPermissions;
    } catch (const fs::filesystem_error& e) {
        logError("Filesystem error in compareFileAndSelfPermissions: ",
                 e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        logError("Exception in compareFileAndSelfPermissions: ", e.what());
        return std::nullopt;
    } catch (...) {
        logError("Unknown exception in compareFileAndSelfPermissions");
        return std::nullopt;
    }
}

}  // namespace atom::io