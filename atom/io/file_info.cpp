#include "file_info.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>

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

auto getFileInfo(const fs::path& filePath, const FileInfoOptions& options) -> FileInfo {
    try {
        if (filePath.empty()) {
            spdlog::error("Empty file path provided");
            throw std::invalid_argument("Empty file path provided");
        }

        spdlog::debug("Getting file info for: {}", filePath.string());

        FileInfo info;

        if (!fs::exists(filePath)) {
            spdlog::error("File does not exist: {}", filePath.string());
            THROW_FAIL_TO_READ_FILE("File does not exist: " +
                                    filePath.string());
        }

        info.filePath = String(fs::absolute(filePath).string());
        info.fileName = String(filePath.filename().string());
        info.extension = String(filePath.extension().string());
        info.fileSize =
            fs::is_regular_file(filePath) ? fs::file_size(filePath) : 0;

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
                info.owner = "Owner retrieval not implemented";
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

void printFileInfo(const FileInfo& info) {
    try {
        spdlog::debug("Printing file info for: {}", info.filePath);

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
        spdlog::error("printFileInfo encountered an error: {}", ex.what());
        std::cerr << "printFileInfo encountered an error: " << ex.what()
                  << std::endl;
    }
}

// Enhanced FileInfo methods implementation
String FileInfo::getFormattedSize() const {
    return utils::formatFileSize(fileSize);
}

std::chrono::seconds FileInfo::getAge() const {
    auto now = std::chrono::system_clock::now();
    auto lastModTime = std::chrono::system_clock::from_time_t(0); // Placeholder - would need proper parsing
    return std::chrono::duration_cast<std::chrono::seconds>(now - lastModTime);
}

bool FileInfo::hasPermission(char permission, int position) const {
    if (position < 0 || position >= static_cast<int>(permissions.size())) {
        return false;
    }
    return permissions[position] == permission;
}

// Enhanced function implementations
void getFileInfoAsync(const fs::path& filePath,
                     FileInfoCallback callback,
                     FileInfoErrorCallback errorCallback,
                     const FileInfoOptions& options) {
    std::thread([=]() {
        try {
            auto info = getFileInfo(filePath, options);
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

Vector<FileInfo> getMultipleFileInfo(const Vector<fs::path>& filePaths,
                                    const FileInfoOptions& options) {
    Vector<FileInfo> results;
    results.reserve(filePaths.size());

    for (const auto& path : filePaths) {
        try {
            results.push_back(getFileInfo(path, options));
        } catch (const std::exception& e) {
            spdlog::warn("Failed to get info for {}: {}", path.string(), e.what());
            // Continue with other files
        }
    }

    return results;
}

std::future<Vector<FileInfo>> getMultipleFileInfoAsync(
    const Vector<fs::path>& filePaths,
    FileInfoCallback callback,
    ProgressCallback progressCallback,
    const FileInfoOptions& options) {

    return std::async(std::launch::async, [=]() {
        Vector<FileInfo> results;
        results.reserve(filePaths.size());

        for (size_t i = 0; i < filePaths.size(); ++i) {
            try {
                auto info = getFileInfo(filePaths[i], options);
                results.push_back(info);

                if (callback) {
                    callback(info);
                }

                if (progressCallback) {
                    double percentage = static_cast<double>(i + 1) / filePaths.size() * 100.0;
                    progressCallback(i + 1, filePaths.size(), percentage);
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to get info for {}: {}", filePaths[i].string(), e.what());
            }
        }

        return results;
    });
}

// FileInfoCache implementation
FileInfoCache& FileInfoCache::getInstance() {
    static FileInfoCache instance;
    return instance;
}

std::optional<FileInfo> FileInfoCache::get(const fs::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(String(path.string()));
    if (it != cache_.end() && it->second.isValid()) {
        hit_count_++;
        return it->second;
    }

    miss_count_++;
    return std::nullopt;
}

void FileInfoCache::put(const fs::path& path, const FileInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[String(path.string())] = info;
}

void FileInfoCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

void FileInfoCache::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (!it->second.isValid()) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t FileInfoCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t FileInfoCache::getHitCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hit_count_;
}

size_t FileInfoCache::getMissCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return miss_count_;
}

void FileInfoCache::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    hit_count_ = 0;
    miss_count_ = 0;
}

// FileInfoFormatter implementation
String FileInfoFormatter::format(const FileInfo& info, Format format) {
    switch (format) {
        case Format::CONSOLE: return formatConsole(info);
        case Format::JSON: return formatJSON(info);
        case Format::XML: return formatXML(info);
        case Format::CSV: return formatCSV(info);
        case Format::MARKDOWN: return formatMarkdown(info);
        default: return formatConsole(info);
    }
}

String FileInfoFormatter::formatMultiple(const Vector<FileInfo>& infos, Format format) {
    String result;

    if (format == Format::CSV) {
        result += "FilePath,FileName,Extension,FileSize,FileType,LastModified,Permissions\n";
    } else if (format == Format::JSON) {
        result += "[\n";
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        if (format == Format::JSON && i > 0) {
            result += ",\n";
        }
        result += format == Format::JSON ? formatJSON(infos[i]) : FileInfoFormatter::format(infos[i], format);
        if (format != Format::JSON && format != Format::CSV) {
            result += "\n---\n";
        }
    }

    if (format == Format::JSON) {
        result += "\n]";
    }

    return result;
}

String FileInfoFormatter::formatConsole(const FileInfo& info) {
    std::ostringstream oss;
    oss << "File Path: " << info.filePath << "\n";
    oss << "File Name: " << info.fileName << "\n";
    oss << "Extension: " << info.extension << "\n";
    oss << "File Size: " << info.getFormattedSize() << "\n";
    oss << "File Type: " << info.fileType << "\n";
    oss << "Last Modified: " << info.lastModifiedTime << "\n";
    oss << "Permissions: " << info.permissions << "\n";
    oss << "Is Hidden: " << (info.isHidden ? "Yes" : "No") << "\n";
    return String(oss.str());
}

String FileInfoFormatter::formatJSON(const FileInfo& info) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"filePath\": \"" << info.filePath << "\",\n";
    oss << "  \"fileName\": \"" << info.fileName << "\",\n";
    oss << "  \"extension\": \"" << info.extension << "\",\n";
    oss << "  \"fileSize\": " << info.fileSize << ",\n";
    oss << "  \"fileType\": \"" << info.fileType << "\",\n";
    oss << "  \"lastModifiedTime\": \"" << info.lastModifiedTime << "\",\n";
    oss << "  \"permissions\": \"" << info.permissions << "\",\n";
    oss << "  \"isHidden\": " << (info.isHidden ? "true" : "false") << "\n";
    oss << "}";
    return String(oss.str());
}

String FileInfoFormatter::formatXML(const FileInfo& info) {
    std::ostringstream oss;
    oss << "<fileInfo>\n";
    oss << "  <filePath>" << info.filePath << "</filePath>\n";
    oss << "  <fileName>" << info.fileName << "</fileName>\n";
    oss << "  <extension>" << info.extension << "</extension>\n";
    oss << "  <fileSize>" << info.fileSize << "</fileSize>\n";
    oss << "  <fileType>" << info.fileType << "</fileType>\n";
    oss << "  <lastModifiedTime>" << info.lastModifiedTime << "</lastModifiedTime>\n";
    oss << "  <permissions>" << info.permissions << "</permissions>\n";
    oss << "  <isHidden>" << (info.isHidden ? "true" : "false") << "</isHidden>\n";
    oss << "</fileInfo>";
    return String(oss.str());
}

String FileInfoFormatter::formatCSV(const FileInfo& info) {
    std::ostringstream oss;
    oss << "\"" << info.filePath << "\",";
    oss << "\"" << info.fileName << "\",";
    oss << "\"" << info.extension << "\",";
    oss << info.fileSize << ",";
    oss << "\"" << info.fileType << "\",";
    oss << "\"" << info.lastModifiedTime << "\",";
    oss << "\"" << info.permissions << "\"";
    return String(oss.str());
}

String FileInfoFormatter::formatMarkdown(const FileInfo& info) {
    std::ostringstream oss;
    oss << "## " << info.fileName << "\n\n";
    oss << "| Property | Value |\n";
    oss << "|----------|-------|\n";
    oss << "| Path | `" << info.filePath << "` |\n";
    oss << "| Size | " << info.getFormattedSize() << " |\n";
    oss << "| Type | " << info.fileType << " |\n";
    oss << "| Modified | " << info.lastModifiedTime << " |\n";
    oss << "| Permissions | `" << info.permissions << "` |\n";
    oss << "| Hidden | " << (info.isHidden ? "Yes" : "No") << " |\n";
    return String(oss.str());
}

// Utility functions implementation
namespace utils {

String formatFileSize(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return String(oss.str());
}

String formatPermissions(const fs::perms& permissions) {
    std::string result;
    result.reserve(9);

    result += (permissions & fs::perms::owner_read) != fs::perms::none ? 'r' : '-';
    result += (permissions & fs::perms::owner_write) != fs::perms::none ? 'w' : '-';
    result += (permissions & fs::perms::owner_exec) != fs::perms::none ? 'x' : '-';
    result += (permissions & fs::perms::group_read) != fs::perms::none ? 'r' : '-';
    result += (permissions & fs::perms::group_write) != fs::perms::none ? 'w' : '-';
    result += (permissions & fs::perms::group_exec) != fs::perms::none ? 'x' : '-';
    result += (permissions & fs::perms::others_read) != fs::perms::none ? 'r' : '-';
    result += (permissions & fs::perms::others_write) != fs::perms::none ? 'w' : '-';
    result += (permissions & fs::perms::others_exec) != fs::perms::none ? 'x' : '-';

    return String(result);
}

String getFileTypeDescription(const fs::path& filePath) {
    if (fs::is_directory(filePath)) {
        return "Directory";
    } else if (fs::is_regular_file(filePath)) {
        return "Regular file";
    } else if (fs::is_symlink(filePath)) {
        return "Symbolic link";
    } else if (fs::is_block_file(filePath)) {
        return "Block device";
    } else if (fs::is_character_file(filePath)) {
        return "Character device";
    } else if (fs::is_fifo(filePath)) {
        return "FIFO/pipe";
    } else if (fs::is_socket(filePath)) {
        return "Socket";
    } else {
        return "Other";
    }
}

bool isTextFile(const fs::path& filePath) {
    if (!fs::is_regular_file(filePath)) {
        return false;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read first 512 bytes to check for binary content
    char buffer[512];
    file.read(buffer, sizeof(buffer));
    auto bytesRead = file.gcount();

    // Check for null bytes (common in binary files)
    for (std::streamsize i = 0; i < bytesRead; ++i) {
        if (buffer[i] == '\0') {
            return false;
        }
    }

    return true;
}

bool isBinaryFile(const fs::path& filePath) {
    return !isTextFile(filePath);
}

FileInfoOptions getOptimalOptions(const String& useCase) {
    if (useCase == "fast") {
        return FileInfoOptions::createFastOptions();
    } else if (useCase == "detailed") {
        return FileInfoOptions::createDetailedOptions();
    } else {
        // Balanced default
        FileInfoOptions options;
        options.includeChecksum = false;
        options.includeMimeType = true;
        options.includeExtendedAttributes = false;
        options.enableCaching = true;
        return options;
    }
}

} // namespace utils

}  // namespace atom::io
