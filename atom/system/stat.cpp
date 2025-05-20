/*
 * stat.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Python like stat for Windows & Linux

**************************************************/

#include "stat.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "atom/error/exception.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <Lmcons.h>
#include <io.h>
#include <shlwapi.h>
// clang-format on
#ifdef _MSC_VER
#pragma comment(lib, "shlwapi.lib")
#endif
#else
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif

#include "atom/utils/string.hpp"

namespace atom::system {

Stat::Stat(const fs::path& path, bool followSymlinks)
    : path_(path),
      followSymlinks_(followSymlinks),
      statInfo_(std::make_unique<StatInfo>()) {
    update();
}

void Stat::initStatInfo() {
    if (!statInfo_) {
        statInfo_ = std::make_unique<StatInfo>();
    } else {
        statInfo_->clear();
    }
}

void Stat::update() {
    initStatInfo();

    ec_.clear();

    // Basic file status check
    if (followSymlinks_) {
        statInfo_->status = fs::status(path_, ec_);
    } else {
        statInfo_->status = fs::symlink_status(path_, ec_);
    }

    if (ec_) {
        THROW_FAIL_TO_OPEN_FILE("Failed to get file status", path_.string(),
                                ec_);
    }

    // Check if it's a symlink
    statInfo_->isSymbolicLink =
        (fs::symlink_status(path_, ec_).type() == fs::file_type::symlink);
    if (!ec_ && statInfo_->isSymbolicLink.value()) {
        statInfo_->symTarget = fs::read_symlink(path_, ec_);
    }
}

bool Stat::exists() const {
    try {
        return fs::exists(path_);
    } catch (const std::exception&) {
        return false;
    }
}

void Stat::checkFileExists() const {
    if (!exists()) {
        std::string msg = "File does not exist: " + path_.string();
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory), msg);
    }
}

void Stat::ensureStatInfoCached() const {
    if (!statInfo_) {
        // Create statInfo_ if it doesn't exist (should never happen)
        const_cast<Stat*>(this)->statInfo_ = std::make_unique<StatInfo>();
        const_cast<Stat*>(this)->update();
    }
}

fs::file_type Stat::type() {
    ensureStatInfoCached();

    if (!statInfo_->status.has_value()) {
        ec_.clear();
        fs::file_status status;
        if (followSymlinks_) {
            status = fs::status(path_, ec_);
        } else {
            status = fs::symlink_status(path_, ec_);
        }

        if (ec_) {
            throw std::system_error(
                ec_, "Failed to get file type for: " + path_.string());
        }
        statInfo_->status = status;
    }

    return statInfo_->status.value().type();
}

std::uintmax_t Stat::size() {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->fileSize.has_value()) {
        ec_.clear();
        std::uintmax_t sz = fs::file_size(path_, ec_);
        if (ec_) {
            throw std::system_error(
                ec_, "Failed to get file size for: " + path_.string());
        }
        statInfo_->fileSize = sz;
    }

    return statInfo_->fileSize.value();
}

std::time_t Stat::atime() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->accessTime.has_value()) {
#ifdef _WIN32
        struct _stat64 fileStat;
        if (_wstat64(atom::utils::stringToWString(path_.string()).c_str(),
                     &fileStat) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get access time for: " + path_.string());
        }
        statInfo_->accessTime = fileStat.st_atime;
#else
        struct stat fileStat;
        if (stat(path_.c_str(), &fileStat) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get access time for: " + path_.string());
        }
        statInfo_->accessTime = fileStat.st_atime;
#endif
    }

    return statInfo_->accessTime.value();
}

std::time_t Stat::mtime() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->modifyTime.has_value()) {
        try {
            auto fileTime = fs::last_write_time(path_);
            auto duration = fileTime.time_since_epoch();
            auto systemTime = std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    duration));
            statInfo_->modifyTime =
                std::chrono::system_clock::to_time_t(systemTime);
        } catch (const std::exception& e) {
            throw std::system_error(std::make_error_code(std::errc::io_error),
                                    "Failed to get modification time for: " +
                                        path_.string() + " - " + e.what());
        }
    }

    return statInfo_->modifyTime.value();
}

std::time_t Stat::ctime() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->createTime.has_value()) {
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExW(
                atom::utils::stringToWString(path_.string()).c_str(),
                GetFileExInfoStandard, &attr) == 0) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get creation time for: " + path_.string());
        }

        ULARGE_INTEGER ull;
        ull.LowPart = attr.ftCreationTime.dwLowDateTime;
        ull.HighPart = attr.ftCreationTime.dwHighDateTime;

        // Convert from 100-nanosecond intervals since January 1, 1601 (UTC)
        // to seconds since the Unix epoch (January 1, 1970 UTC)
        const uint64_t WINDOWS_TICK = 10000000;
        const uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;

        time_t result = static_cast<time_t>(ull.QuadPart / WINDOWS_TICK -
                                            SEC_TO_UNIX_EPOCH);
        statInfo_->createTime = result;
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get creation time for: " + path_.string());
        }
        statInfo_->createTime = attr.st_ctime;
#endif
    }

    return statInfo_->createTime.value();
}

int Stat::mode() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->fileMode.has_value()) {
#ifdef _WIN32
        // Windows doesn't have a direct equivalent to Unix file mode
        // We'll approximate with a simplified mode
        DWORD attributes = GetFileAttributesW(
            atom::utils::stringToWString(path_.string()).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get file attributes for: " + path_.string());
        }

        int mode = 0;
        // Set read permission for all
        mode |= 0444;

        // If not read-only, set write permission
        if (!(attributes & FILE_ATTRIBUTE_READONLY)) {
            mode |= 0222;
        }

        // If it's an executable format, set execute permission
        if (path_.extension() == ".exe" || path_.extension() == ".bat" ||
            path_.extension() == ".cmd" || path_.extension() == ".com") {
            mode |= 0111;
        }

        // If it's a directory, set execute bits to allow traversal
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
            mode |= 0111;
        }

        statInfo_->fileMode = mode;
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get file mode for: " + path_.string());
        }
        statInfo_->fileMode = attr.st_mode;
#endif
    }

    return statInfo_->fileMode.value();
}

int Stat::uid() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->userId.has_value()) {
#ifdef _WIN32
        // Windows doesn't use UID in the same way as Unix-like systems
        // Return 0 as a placeholder
        statInfo_->userId = 0;
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get file UID for: " + path_.string());
        }
        statInfo_->userId = attr.st_uid;
#endif
    }

    return statInfo_->userId.value();
}

int Stat::gid() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->groupId.has_value()) {
#ifdef _WIN32
        // Windows doesn't use GID in the same way as Unix-like systems
        // Return 0 as a placeholder
        statInfo_->groupId = 0;
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get file GID for: " + path_.string());
        }
        statInfo_->groupId = attr.st_gid;
#endif
    }

    return statInfo_->groupId.value();
}

fs::path Stat::path() const { return path_; }

std::uintmax_t Stat::hardLinkCount() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->linkCount.has_value()) {
#ifdef _WIN32
        // Get the hard link count using FindFirstFileNameW in newer Windows
        HANDLE fileHandle =
            CreateFileW(atom::utils::stringToWString(path_.string()).c_str(),
                        FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,  // Needed for directories
                        NULL);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to open file for link count: " + path_.string());
        }

        BY_HANDLE_FILE_INFORMATION fileInfo;
        BOOL result = GetFileInformationByHandle(fileHandle, &fileInfo);
        CloseHandle(fileHandle);

        if (!result) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get file information for link count: " +
                    path_.string());
        }

        statInfo_->linkCount =
            static_cast<std::uintmax_t>(fileInfo.nNumberOfLinks);
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get hard link count for: " + path_.string());
        }
        statInfo_->linkCount = static_cast<std::uintmax_t>(attr.st_nlink);
#endif
    }

    return statInfo_->linkCount.value();
}

std::uintmax_t Stat::deviceId() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->devId.has_value()) {
#ifdef _WIN32
        WCHAR volumePath[MAX_PATH];
        if (!GetVolumePathNameW(
                atom::utils::stringToWString(path_.string()).c_str(),
                volumePath, MAX_PATH)) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get volume path for: " + path_.string());
        }

        DWORD serialNumber = 0;
        if (!GetVolumeInformationW(volumePath, NULL, 0, &serialNumber, NULL,
                                   NULL, NULL, 0)) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get volume information for: " + path_.string());
        }

        statInfo_->devId = static_cast<std::uintmax_t>(serialNumber);
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get device ID for: " + path_.string());
        }
        statInfo_->devId = static_cast<std::uintmax_t>(attr.st_dev);
#endif
    }

    return statInfo_->devId.value();
}

std::uintmax_t Stat::inodeNumber() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->inodeNum.has_value()) {
#ifdef _WIN32
        // Windows doesn't have inodes like Unix systems, but we can use file
        // index
        HANDLE fileHandle =
            CreateFileW(atom::utils::stringToWString(path_.string()).c_str(),
                        FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,  // Needed for directories
                        NULL);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to open file for inode: " + path_.string());
        }

        BY_HANDLE_FILE_INFORMATION fileInfo;
        BOOL result = GetFileInformationByHandle(fileHandle, &fileInfo);
        CloseHandle(fileHandle);

        if (!result) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get file information for inode: " + path_.string());
        }

        ULARGE_INTEGER fileIndex;
        fileIndex.HighPart = fileInfo.nFileIndexHigh;
        fileIndex.LowPart = fileInfo.nFileIndexLow;
        statInfo_->inodeNum = fileIndex.QuadPart;
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get inode number for: " + path_.string());
        }
        statInfo_->inodeNum = static_cast<std::uintmax_t>(attr.st_ino);
#endif
    }

    return statInfo_->inodeNum.value();
}

std::uintmax_t Stat::blockSize() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->blkSize.has_value()) {
#ifdef _WIN32
        // Windows doesn't expose block size directly
        // We'll use the allocation granularity as an approximation
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        statInfo_->blkSize =
            static_cast<std::uintmax_t>(sysInfo.dwAllocationGranularity);
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get block size for: " + path_.string());
        }
        statInfo_->blkSize = static_cast<std::uintmax_t>(attr.st_blksize);
#endif
    }

    return statInfo_->blkSize.value();
}

std::string Stat::ownerName() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->owner.has_value()) {
#ifdef _WIN32
        HANDLE fileHandle =
            CreateFileW(atom::utils::stringToWString(path_.string()).c_str(),
                        READ_CONTROL, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,  // Needed for directories
                        NULL);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to open file for owner: " + path_.string());
        }

        // Get size needed for security descriptor
        DWORD secDescSize = 0;
        GetFileSecurityW(atom::utils::stringToWString(path_.string()).c_str(),
                         OWNER_SECURITY_INFORMATION, NULL, 0, &secDescSize);

        if (secDescSize == 0) {
            CloseHandle(fileHandle);
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get security descriptor size: " + path_.string());
        }

        // Allocate buffer for security descriptor
        std::vector<BYTE> secDescBuffer(secDescSize);
        if (!GetFileSecurityW(
                atom::utils::stringToWString(path_.string()).c_str(),
                OWNER_SECURITY_INFORMATION,
                reinterpret_cast<PSECURITY_DESCRIPTOR>(&secDescBuffer[0]),
                secDescSize, &secDescSize)) {
            CloseHandle(fileHandle);
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get security descriptor: " + path_.string());
        }

        // Get owner SID
        PSID ownerSid = NULL;
        BOOL ownerDefaulted = FALSE;
        if (!GetSecurityDescriptorOwner(
                reinterpret_cast<PSECURITY_DESCRIPTOR>(&secDescBuffer[0]),
                &ownerSid, &ownerDefaulted)) {
            CloseHandle(fileHandle);
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to get owner SID: " + path_.string());
        }

        // Get account name from SID
        WCHAR userName[UNLEN + 1] = {0};
        DWORD userNameSize = UNLEN + 1;
        WCHAR domainName[DNLEN + 1] = {0};
        DWORD domainNameSize = DNLEN + 1;
        SID_NAME_USE sidType;

        if (!LookupAccountSidW(NULL,  // Local computer
                               ownerSid, userName, &userNameSize, domainName,
                               &domainNameSize, &sidType)) {
            CloseHandle(fileHandle);
            throw std::system_error(
                std::error_code(GetLastError(), std::system_category()),
                "Failed to lookup account name: " + path_.string());
        }

        CloseHandle(fileHandle);

        // Convert to UTF-8
        std::wstring wideUserName(userName);
        statInfo_->owner = atom::utils::wstringToString(wideUserName);
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get file owner for: " + path_.string());
        }

        struct passwd* pwd = getpwuid(attr.st_uid);
        if (pwd == nullptr) {
            // If unable to resolve user name, return UID as string
            statInfo_->owner = std::to_string(attr.st_uid);
        } else {
            statInfo_->owner = std::string(pwd->pw_name);
        }
#endif
    }

    return statInfo_->owner.value();
}

std::string Stat::groupName() const {
    checkFileExists();
    ensureStatInfoCached();

    if (!statInfo_->group.has_value()) {
#ifdef _WIN32
        // Getting group name on Windows is complex and not directly equivalent
        // to Unix Return a placeholder
        statInfo_->group = "None";
#else
        struct stat attr;
        if (stat(path_.c_str(), &attr) != 0) {
            throw std::system_error(
                std::error_code(errno, std::system_category()),
                "Failed to get file group for: " + path_.string());
        }

        struct group* grp = getgrgid(attr.st_gid);
        if (grp == nullptr) {
            // If unable to resolve group name, return GID as string
            statInfo_->group = std::to_string(attr.st_gid);
        } else {
            statInfo_->group = std::string(grp->gr_name);
        }
#endif
    }

    return statInfo_->group.value();
}

bool Stat::isSymlink() const {
    ensureStatInfoCached();

    if (!statInfo_->isSymbolicLink.has_value()) {
        try {
            statInfo_->isSymbolicLink =
                (fs::symlink_status(path_).type() == fs::file_type::symlink);
        } catch (const std::exception&) {
            statInfo_->isSymbolicLink = false;
        }
    }

    return statInfo_->isSymbolicLink.value();
}

bool Stat::isDirectory() const {
    try {
        return followSymlinks_ ? fs::is_directory(path_)
                               : fs::is_directory(fs::symlink_status(path_));
    } catch (const std::exception&) {
        return false;
    }
}

bool Stat::isRegularFile() const {
    try {
        if (followSymlinks_) {
            return fs::is_regular_file(path_);
        } else {
            return fs::is_regular_file(fs::symlink_status(path_));
        }
    } catch (const std::exception&) {
        return false;
    }
}

bool Stat::isReadable() const {
    try {
#ifdef _WIN32
        // Check for read access on Windows
        DWORD attributes = GetFileAttributesW(
            atom::utils::stringToWString(path_.string()).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        // Try to open the file for reading
        std::ifstream file(path_, std::ios::in | std::ios::binary);
        return file.good();
#else
        // Check for read access on Unix-like systems
        return access(path_.c_str(), R_OK) == 0;
#endif
    } catch (const std::exception&) {
        return false;
    }
}

bool Stat::isWritable() const {
    try {
#ifdef _WIN32
        // Check for write access on Windows
        DWORD attributes = GetFileAttributesW(
            atom::utils::stringToWString(path_.string()).c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        if (attributes & FILE_ATTRIBUTE_READONLY) {
            return false;
        }

        // Try to open the file for writing
        HANDLE fileHandle =
            CreateFileW(atom::utils::stringToWString(path_.string()).c_str(),
                        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            return false;
        }

        CloseHandle(fileHandle);
        return true;
#else
        // Check for write access on Unix-like systems
        return access(path_.c_str(), W_OK) == 0;
#endif
    } catch (const std::exception&) {
        return false;
    }
}

bool Stat::isExecutable() const {
    try {
#ifdef _WIN32
        // On Windows, check if the file has an executable extension
        std::string ext = path_.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com";
#else
        // Check for execute access on Unix-like systems
        return access(path_.c_str(), X_OK) == 0;
#endif
    } catch (const std::exception&) {
        return false;
    }
}

bool Stat::hasPermission(bool user, bool group, bool others,
                         FilePermission permission) const {
    checkFileExists();

    int modeBits = mode();
    int permBit = static_cast<int>(permission);

    // Check permissions for the requested user categories
    if (user && ((modeBits & (permBit << 6)) != 0)) {
        return true;
    }

    if (group && ((modeBits & (permBit << 3)) != 0)) {
        return true;
    }

    if (others && ((modeBits & permBit) != 0)) {
        return true;
    }

    return false;
}

fs::path Stat::symlinkTarget() {
    ensureStatInfoCached();

    if (!statInfo_->symTarget.has_value()) {
        if (isSymlink()) {
            ec_.clear();
            auto target = fs::read_symlink(path_, ec_);
            if (ec_) {
                throw std::system_error(
                    ec_,
                    "Failed to read symlink target for: " + path_.string());
            }
            statInfo_->symTarget = target;
        } else {
            statInfo_->symTarget = fs::path();
        }
    }

    return statInfo_->symTarget.value();
}

std::string Stat::formatTime(std::time_t time, const std::string& format) {
    std::tm tmTime;

#ifdef _WIN32
    if (localtime_s(&tmTime, &time) != 0) {
        throw std::runtime_error("Failed to convert time to local time");
    }
#else
    if (localtime_r(&time, &tmTime) == nullptr) {
        throw std::runtime_error("Failed to convert time to local time");
    }
#endif

    std::ostringstream oss;
    oss << std::put_time(&tmTime, format.c_str());
    return oss.str();
}

}  // namespace atom::system