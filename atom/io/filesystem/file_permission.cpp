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

#include <spdlog/spdlog.h>
#include "atom/containers/high_performance.hpp"

namespace atom::io {

#ifdef ATOM_USE_BOOST
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided");
        return {};
    }

    try {
        boost::system::error_code ec;
        fs::path pPath(filePath);
        fs::file_status status = fs::status(pPath, ec);
        if (ec) {
            spdlog::error("Failed to get status for '{}': {}", filePath,
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
        spdlog::error("Exception in getFilePermissions for '{}': {}", filePath,
                      e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getFilePermissions for '{}'",
                      filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        boost::system::error_code ec;
#ifdef _WIN32
        std::array<char, MAX_PATH> path{};
        if (GetModuleFileNameA(nullptr, path.data(),
                               static_cast<DWORD>(path.size())) != 0) {
            return getFilePermissions(path.data());
        } else {
            spdlog::error("GetModuleFileNameA failed with error: {}",
                          GetLastError());
        }
#else
        std::array<char, 1024> path{};
        ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (len > 0) {
            path[len] = '\0';
            return getFilePermissions(path.data());
        } else {
            spdlog::error("readlink /proc/self/exe failed: {}",
                          strerror(errno));
        }
#endif
        fs::path selfPath = fs::current_path(ec);
        if (ec) {
            spdlog::error("Failed to get current path: {}", ec.message());
            return {};
        }
        return getFilePermissions(selfPath.string());

    } catch (const std::exception& e) {
        spdlog::error("Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getSelfPermissions");
        return {};
    }
}

#else

#ifdef _WIN32
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided");
        return {};
    }

    try {
        DWORD dwRtnCode = 0;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        std::array<char, 9> permissions;
        permissions.fill('-');

        auto psdDeleter = [](PSECURITY_DESCRIPTOR psd) {
            if (psd)
                LocalFree(static_cast<HLOCAL>(psd));
        };
        std::unique_ptr<void, decltype(psdDeleter)> psdPtr(nullptr, psdDeleter);

        PACL pDACL = nullptr;
        std::wstring wFilePath(filePath.begin(), filePath.end());
        dwRtnCode = GetNamedSecurityInfoW(wFilePath.c_str(), SE_FILE_OBJECT,
                                          DACL_SECURITY_INFORMATION, nullptr,
                                          nullptr, &pDACL, nullptr, &pSD);

        if (dwRtnCode != ERROR_SUCCESS) {
            spdlog::error(
                "GetNamedSecurityInfoW failed for '{}' with error code: {}",
                filePath, dwRtnCode);
            return {};
        }

        psdPtr.reset(pSD);

        if (pDACL != nullptr) {
            bool canRead = false, canWrite = false, canExec = false;

            for (DWORD i = 0; i < pDACL->AceCount; ++i) {
                ACCESS_ALLOWED_ACE* ace = nullptr;
                if (GetAce(pDACL, i, reinterpret_cast<LPVOID*>(&ace))) {
                    if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) {
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

            if (canRead)
                permissions[0] = permissions[3] = permissions[6] = 'r';
            if (canWrite)
                permissions[1] = permissions[4] = permissions[7] = 'w';
            if (canExec)
                permissions[2] = permissions[5] = permissions[8] = 'x';

        } else {
            spdlog::warn("No DACL found for '{}', cannot determine permissions",
                         filePath);
            return {};
        }

        return {permissions.begin(), permissions.end()};
    } catch (const std::exception& e) {
        spdlog::error("Exception in getFilePermissions for '{}': {}", filePath,
                      e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getFilePermissions for '{}'",
                      filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<wchar_t, MAX_PATH> wPath{};
        if (GetModuleFileNameW(nullptr, wPath.data(),
                               static_cast<DWORD>(wPath.size())) == 0) {
            const auto error = GetLastError();
            spdlog::error("GetModuleFileNameW failed with error: {}", error);
            return {};
        }

        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wPath.data(), -1,
                                              NULL, 0, NULL, NULL);
        std::string path(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wPath.data(), -1, &path[0], size_needed,
                            NULL, NULL);
        path.pop_back();

        return getFilePermissions(path);
    } catch (const std::exception& e) {
        spdlog::error("Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getSelfPermissions");
        return {};
    }
}
#else
std::string getFilePermissions(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided");
        return {};
    }

    try {
        struct stat fileStat{};
        if (stat(filePath.data(), &fileStat) < 0) {
            spdlog::error("stat failed for '{}': {}", filePath,
                          strerror(errno));
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
        spdlog::error("Exception in getFilePermissions for '{}': {}", filePath,
                      e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getFilePermissions for '{}'",
                      filePath);
        return {};
    }
}

std::string getSelfPermissions() noexcept {
    try {
        std::array<char, 1024> path{};

        ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (len < 0) {
            spdlog::error("readlink /proc/self/exe failed: {}",
                          strerror(errno));

            try {
                auto currentPath = fs::current_path();
                spdlog::warn("Falling back to current directory permissions");
                return getFilePermissions(currentPath.string());
            } catch (const fs::filesystem_error& e) {
                spdlog::error("Failed to get current path: {}", e.what());
                return {};
            }
        }

        path[len] = '\0';
        return getFilePermissions(path.data());
    } catch (const std::exception& e) {
        spdlog::error("Exception in getSelfPermissions: {}", e.what());
        return {};
    } catch (...) {
        spdlog::error("Unknown exception in getSelfPermissions");
        return {};
    }
}
#endif
#endif

std::optional<bool> compareFileAndSelfPermissions(
    std::string_view filePath) noexcept {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided for comparison");
        return std::nullopt;
    }

    try {
        fs::path pPath(filePath);
        if (!fs::exists(pPath)) {
            spdlog::error("File does not exist for comparison: '{}'", filePath);
            return std::nullopt;
        }

        std::string filePermissions = getFilePermissions(filePath);
        if (filePermissions.empty()) {
            spdlog::warn(
                "Could not get permissions for file '{}' during comparison",
                filePath);
            return std::nullopt;
        }

        std::string selfPermissions = getSelfPermissions();
        if (selfPermissions.empty()) {
            spdlog::warn("Could not get self permissions during comparison");
            return std::nullopt;
        }

        spdlog::debug("Comparing file ('{}': {}) and self ({}) permissions",
                      filePath, filePermissions, selfPermissions);

        return filePermissions == selfPermissions;
    }
#ifdef ATOM_USE_BOOST
    catch (const boost::system::system_error& e) {
        spdlog::error(
            "Boost filesystem error in compareFileAndSelfPermissions for '{}': "
            "{}",
            filePath, e.what());
        return std::nullopt;
    }
#else
    catch (const fs::filesystem_error& e) {
        spdlog::error(
            "Filesystem error in compareFileAndSelfPermissions for '{}': {}",
            filePath, e.what());
        return std::nullopt;
    }
#endif
    catch (const std::exception& e) {
        spdlog::error("Exception in compareFileAndSelfPermissions for '{}': {}",
                      filePath, e.what());
        return std::nullopt;
    } catch (...) {
        spdlog::error(
            "Unknown exception in compareFileAndSelfPermissions for '{}'",
            filePath);
        return std::nullopt;
    }
}

void changeFilePermissions(const fs::path& filePath,
                           const atom::containers::String& permissions) {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided to changeFilePermissions");
        throw std::invalid_argument("Empty file path provided");
    }

    try {
        if (!fs::exists(filePath)) {
            spdlog::error("File does not exist: '{}'", filePath.string());
            throw std::runtime_error("File does not exist: " +
                                     filePath.string());
        }

        fs::perms newPerms = fs::perms::none;

        if (permissions.length() != 9) {
            spdlog::error(
                "Invalid permission format: '{}'. Expected 'rwxrwxrwx'",
                permissions);
            throw std::invalid_argument(
                "Invalid permission format. Expected format: 'rwxrwxrwx'");
        }

        if (permissions[0] == 'r')
            newPerms |= fs::perms::owner_read;
        if (permissions[1] == 'w')
            newPerms |= fs::perms::owner_write;
        if (permissions[2] == 'x')
            newPerms |= fs::perms::owner_exec;
        if (permissions[3] == 'r')
            newPerms |= fs::perms::group_read;
        if (permissions[4] == 'w')
            newPerms |= fs::perms::group_write;
        if (permissions[5] == 'x')
            newPerms |= fs::perms::group_exec;
        if (permissions[6] == 'r')
            newPerms |= fs::perms::others_read;
        if (permissions[7] == 'w')
            newPerms |= fs::perms::others_write;
        if (permissions[8] == 'x')
            newPerms |= fs::perms::others_exec;

        spdlog::debug("Setting permissions for '{}' to {:#o}",
                      filePath.string(), static_cast<int>(newPerms));
        fs::permissions(filePath, newPerms, fs::perm_options::replace);
        spdlog::info("Successfully changed permissions for '{}'",
                     filePath.string());

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to change permissions for '{}': {}",
                      filePath.string(), e.what());
        throw std::runtime_error("Failed to change permissions for '" +
                                 filePath.string() + "': " + e.what());
    } catch (const std::invalid_argument& e) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error changing file permissions for '{}': {}",
                      filePath.string(), e.what());
        throw std::runtime_error("Error changing file permissions: " +
                                 std::string(e.what()));
    } catch (...) {
        spdlog::error("Unknown error changing file permissions for '{}'",
                      filePath.string());
        throw std::runtime_error(
            "Unknown error changing file permissions for '" +
            filePath.string() + "'");
    }
}

}  // namespace atom::io