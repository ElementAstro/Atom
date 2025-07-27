#include "file_permission.hpp"

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <string_view>
#include <sstream>
#include <thread>
#include <future>

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
#include <pwd.h>
#include <grp.h>
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

// Enhanced PermissionInfo methods implementation
uint32_t PermissionInfo::toOctal() const {
    return octalPermissions;
}

bool PermissionInfo::hasPermission(char permission, int position) const {
    if (position < 0 || position >= static_cast<int>(permissionString.size())) {
        return false;
    }
    return permissionString[position] == permission;
}

String PermissionInfo::getDescription() const {
    std::ostringstream oss;
    oss << "Permissions: " << permissionString << " (";
    oss << std::oct << octalPermissions << std::dec << ")";
    if (!owner.empty()) {
        oss << ", Owner: " << owner;
    }
    if (!group.empty()) {
        oss << ", Group: " << group;
    }
    return String(oss.str());
}

// Enhanced function implementations
PermissionInfo getPermissionInfo(const std::filesystem::path& filePath,
                                const PermissionOptions& options) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    if (options.enableCaching) {
        auto cached = PermissionCache::getInstance().get(filePath);
        if (cached && cached->isValid(options.cacheMaxAge)) {
            return *cached;
        }
    }

    PermissionInfo info;
    info.filePath = String(filePath.string());
    info.retrievalTime = start_time;

    try {
        // Get basic permissions
        info.permissionString = String(getFilePermissions(filePath.string()));
        if (info.permissionString.empty()) {
            throw std::runtime_error("Failed to get file permissions");
        }

        // Convert to octal
        info.octalPermissions = utils::stringToOctal(info.permissionString);

        // Set convenience flags
        info.isReadable = info.permissionString[0] == 'r';
        info.isWritable = info.permissionString[1] == 'w';
        info.isExecutable = info.permissionString[2] == 'x';

        // Get ownership information if requested
        if (options.includeOwnership) {
#ifndef _WIN32
            struct stat fileStat;
            if (stat(filePath.c_str(), &fileStat) == 0) {
                info.unixMode = fileStat.st_mode;
                info.uid = fileStat.st_uid;
                info.gid = fileStat.st_gid;

                // Get owner name
                struct passwd* pw = getpwuid(fileStat.st_uid);
                if (pw) {
                    info.owner = String(pw->pw_name);
                }

                // Get group name
                struct group* gr = getgrgid(fileStat.st_gid);
                if (gr) {
                    info.group = String(gr->gr_name);
                }
            }
#endif
        }

        auto end_time = std::chrono::steady_clock::now();
        info.retrievalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Cache the result
        if (options.enableCaching) {
            PermissionCache::getInstance().put(filePath, info);
        }

        return info;

    } catch (const std::exception& e) {
        spdlog::error("Failed to get permission info for {}: {}", filePath.string(), e.what());
        throw;
    }
}

void getPermissionInfoAsync(const std::filesystem::path& filePath,
                           PermissionCallback callback,
                           PermissionErrorCallback errorCallback,
                           const PermissionOptions& options) {
    std::thread([=]() {
        try {
            auto info = getPermissionInfo(filePath, options);
            if (callback) {
                callback(info);
            }
        } catch (const std::exception& e) {
            if (errorCallback) {
                errorCallback(String(e.what()));
            }
        }
    }).detach();
}

Vector<PermissionInfo> getMultiplePermissionInfo(const Vector<std::filesystem::path>& filePaths,
                                                 const PermissionOptions& options) {
    Vector<PermissionInfo> results;
    results.reserve(filePaths.size());

    for (const auto& path : filePaths) {
        try {
            results.push_back(getPermissionInfo(path, options));
        } catch (const std::exception& e) {
            spdlog::warn("Failed to get permission info for {}: {}", path.string(), e.what());
            // Continue with other files
        }
    }

    return results;
}

std::future<Vector<PermissionInfo>> getMultiplePermissionInfoAsync(
    const Vector<std::filesystem::path>& filePaths,
    PermissionCallback callback,
    ProgressCallback progressCallback,
    const PermissionOptions& options) {

    return std::async(std::launch::async, [=]() {
        Vector<PermissionInfo> results;
        results.reserve(filePaths.size());

        for (size_t i = 0; i < filePaths.size(); ++i) {
            try {
                auto info = getPermissionInfo(filePaths[i], options);
                results.push_back(info);

                if (callback) {
                    callback(info);
                }

                if (progressCallback) {
                    double percentage = static_cast<double>(i + 1) / filePaths.size() * 100.0;
                    progressCallback(i + 1, filePaths.size(), percentage);
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to get permission info for {}: {}", filePaths[i].string(), e.what());
            }
        }

        return results;
    });
}

void changeFilePermissionsEx(const std::filesystem::path& filePath,
                            const String& permissions,
                            const PermissionOptions& options) {
    try {
        // Validate input
        if (!utils::isValidPermissionString(permissions)) {
            throw std::invalid_argument("Invalid permission format: " + permissions);
        }

        // Use existing function for now
        changeFilePermissions(filePath, permissions);

        // Clear cache entry if caching is enabled
        if (options.enableCaching) {
            // Note: We'd need to implement cache invalidation
            PermissionCache::getInstance().clear(); // Simple approach for now
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to change permissions for {}: {}", filePath.string(), e.what());
        throw;
    }
}

// PermissionCache implementation
PermissionCache& PermissionCache::getInstance() {
    static PermissionCache instance;
    return instance;
}

std::optional<PermissionInfo> PermissionCache::get(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(String(path.string()));
    if (it != cache_.end() && it->second.isValid()) {
        hit_count_++;
        return it->second;
    }

    miss_count_++;
    return std::nullopt;
}

void PermissionCache::put(const std::filesystem::path& path, const PermissionInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[String(path.string())] = info;
}

void PermissionCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

void PermissionCache::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (!it->second.isValid()) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t PermissionCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t PermissionCache::getHitCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hit_count_;
}

size_t PermissionCache::getMissCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return miss_count_;
}

void PermissionCache::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    hit_count_ = 0;
    miss_count_ = 0;
}

// PermissionAnalyzer implementation
String PermissionAnalyzer::comparePermissions(const PermissionInfo& info1, const PermissionInfo& info2) {
    std::ostringstream oss;

    if (info1.permissionString == info2.permissionString) {
        oss << "Permissions are identical: " << info1.permissionString;
    } else {
        oss << "Permissions differ:\n";
        oss << "  File 1: " << info1.permissionString << " (" << std::oct << info1.octalPermissions << std::dec << ")\n";
        oss << "  File 2: " << info2.permissionString << " (" << std::oct << info2.octalPermissions << std::dec << ")";

        // Highlight differences
        for (size_t i = 0; i < std::min(info1.permissionString.size(), info2.permissionString.size()); ++i) {
            if (info1.permissionString[i] != info2.permissionString[i]) {
                oss << "\n  Difference at position " << i << ": '"
                    << info1.permissionString[i] << "' vs '" << info2.permissionString[i] << "'";
            }
        }
    }

    return String(oss.str());
}

String PermissionAnalyzer::suggestPermissions(const std::filesystem::path& filePath) {
    try {
        if (std::filesystem::is_directory(filePath)) {
            return "rwxr-xr-x"; // 755 for directories
        } else if (std::filesystem::is_regular_file(filePath)) {
            // Check if it's an executable
            auto extension = filePath.extension().string();
            if (extension == ".exe" || extension == ".sh" || extension == ".py" || extension.empty()) {
                // Check if file has execute permission or is a script
                auto current_perms = getFilePermissions(filePath.string());
                if (!current_perms.empty() && (current_perms[2] == 'x' || current_perms[5] == 'x' || current_perms[8] == 'x')) {
                    return "rwxr-xr-x"; // 755 for executables
                }
            }
            return "rw-r--r--"; // 644 for regular files
        } else {
            return "rw-r--r--"; // Default for other file types
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to suggest permissions for {}: {}", filePath.string(), e.what());
        return "rw-r--r--"; // Safe default
    }
}

bool PermissionAnalyzer::validatePermissionString(const String& permissions) {
    return utils::isValidPermissionString(permissions);
}

String PermissionAnalyzer::convertPermissionFormat(const String& input, const String& fromFormat, const String& toFormat) {
    try {
        if (fromFormat == "string" && toFormat == "octal") {
            uint32_t octal = utils::stringToOctal(input);
            std::ostringstream oss;
            oss << std::oct << octal;
            return String(oss.str());
        } else if (fromFormat == "octal" && toFormat == "string") {
            uint32_t octal = std::stoul(input, nullptr, 8);
            return utils::octalToString(octal);
        } else {
            return input; // No conversion needed or unsupported
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to convert permission format: {}", e.what());
        return input;
    }
}

bool PermissionAnalyzer::arePermissionsSecure(const PermissionInfo& info) {
    // Check for common security issues

    // World-writable files are generally insecure
    if (info.permissionString.size() >= 8 && info.permissionString[7] == 'w') {
        return false;
    }

    // World-writable directories without sticky bit are insecure
    if (info.permissionString.size() >= 8 && info.permissionString[7] == 'w' &&
        info.permissionString[8] == 'x') {
        // Check for sticky bit (would need more detailed analysis)
        return false;
    }

    // Files with no owner permissions are suspicious
    if (info.permissionString.size() >= 3 &&
        info.permissionString[0] == '-' && info.permissionString[1] == '-' && info.permissionString[2] == '-') {
        return false;
    }

    return true; // Passed basic security checks
}

// Utility functions implementation
namespace utils {

String octalToString(uint32_t octal) {
    std::array<char, 9> permissions;

    // Owner permissions
    permissions[0] = (octal & 0400) ? 'r' : '-';
    permissions[1] = (octal & 0200) ? 'w' : '-';
    permissions[2] = (octal & 0100) ? 'x' : '-';

    // Group permissions
    permissions[3] = (octal & 0040) ? 'r' : '-';
    permissions[4] = (octal & 0020) ? 'w' : '-';
    permissions[5] = (octal & 0010) ? 'x' : '-';

    // Other permissions
    permissions[6] = (octal & 0004) ? 'r' : '-';
    permissions[7] = (octal & 0002) ? 'w' : '-';
    permissions[8] = (octal & 0001) ? 'x' : '-';

    return String(permissions.begin(), permissions.end());
}

uint32_t stringToOctal(const String& permissions) {
    if (permissions.size() != 9) {
        throw std::invalid_argument("Invalid permission string length");
    }

    uint32_t octal = 0;

    // Owner permissions
    if (permissions[0] == 'r') octal |= 0400;
    if (permissions[1] == 'w') octal |= 0200;
    if (permissions[2] == 'x') octal |= 0100;

    // Group permissions
    if (permissions[3] == 'r') octal |= 0040;
    if (permissions[4] == 'w') octal |= 0020;
    if (permissions[5] == 'x') octal |= 0010;

    // Other permissions
    if (permissions[6] == 'r') octal |= 0004;
    if (permissions[7] == 'w') octal |= 0002;
    if (permissions[8] == 'x') octal |= 0001;

    return octal;
}

bool isValidPermissionString(const String& permissions) {
    if (permissions.size() != 9) {
        return false;
    }

    for (char c : permissions) {
        if (c != 'r' && c != 'w' && c != 'x' && c != '-') {
            return false;
        }
    }

    return true;
}

String getDefaultPermissions(const std::filesystem::path& filePath) {
    return PermissionAnalyzer::suggestPermissions(filePath);
}

String formatPermissions(const PermissionInfo& info, const String& format) {
    if (format == "octal") {
        std::ostringstream oss;
        oss << std::oct << info.octalPermissions;
        return String(oss.str());
    } else if (format == "detailed") {
        return info.getDescription();
    } else {
        return info.permissionString; // Default string format
    }
}

PermissionOptions getOptimalOptions(const String& useCase) {
    if (useCase == "fast") {
        return PermissionOptions::createFastOptions();
    } else if (useCase == "detailed") {
        return PermissionOptions::createDetailedOptions();
    } else {
        // Balanced default
        PermissionOptions options;
        options.includeOwnership = false;
        options.enableCaching = true;
        options.enableStatistics = true;
        return options;
    }
}

} // namespace utils

}  // namespace atom::io
