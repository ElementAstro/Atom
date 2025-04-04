/*
 * stat.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: Python like stat for Windows & Linux

**************************************************/

#ifndef ATOM_SYSTEM_STAT_HPP
#define ATOM_SYSTEM_STAT_HPP

#include <ctime>
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace atom::system {

/**
 * @brief Enumeration for file permission flags
 */
enum class FilePermission { Read = 0x04, Write = 0x02, Execute = 0x01 };

/**
 * @brief Class representing file statistics.
 *
 * This class provides methods to retrieve various attributes of a file,
 * such as its type, size, access time, modification time, and so on.
 * It caches file information for better performance.
 */
class Stat {
public:
    /**
     * @brief Constructs a `Stat` object for the specified file path.
     *
     * @param path The path to the file whose statistics are to be retrieved.
     * @param followSymlinks Whether to follow symlinks (default: true)
     * @throw std::system_error if the file cannot be accessed
     */
    explicit Stat(const fs::path& path, bool followSymlinks = true);

    /**
     * @brief Updates the file statistics.
     *
     * This method refreshes the statistics for the file specified in the
     * constructor.
     *
     * @throw std::system_error if there's an error accessing the file
     */
    void update();

    /**
     * @brief Checks if the file exists.
     *
     * @return true if the file exists, false otherwise.
     */
    [[nodiscard]] bool exists() const;

    /**
     * @brief Gets the type of the file.
     *
     * @return The type of the file as an `fs::file_type` enum value.
     */
    [[nodiscard]] fs::file_type type();

    /**
     * @brief Gets the size of the file.
     *
     * @return The size of the file in bytes.
     * @throw std::system_error if the file size cannot be determined
     */
    [[nodiscard]] std::uintmax_t size();

    /**
     * @brief Gets the last access time of the file.
     *
     * @return The last access time of the file as a `std::time_t` value.
     * @throw std::system_error if the access time cannot be determined
     */
    [[nodiscard]] std::time_t atime() const;

    /**
     * @brief Gets the last modification time of the file.
     *
     * @return The last modification time of the file as a `std::time_t` value.
     * @throw std::system_error if the modification time cannot be determined
     */
    [[nodiscard]] std::time_t mtime() const;

    /**
     * @brief Gets the creation time of the file.
     *
     * @return The creation time of the file as a `std::time_t` value.
     * @throw std::system_error if the creation time cannot be determined
     */
    [[nodiscard]] std::time_t ctime() const;

    /**
     * @brief Gets the file mode/permissions.
     *
     * @return The file mode/permissions as an integer value.
     * @throw std::system_error if the file permissions cannot be determined
     */
    [[nodiscard]] int mode() const;

    /**
     * @brief Gets the user ID of the file owner.
     *
     * @return The user ID of the file owner as an integer value.
     * @throw std::system_error if the user ID cannot be determined
     */
    [[nodiscard]] int uid() const;

    /**
     * @brief Gets the group ID of the file owner.
     *
     * @return The group ID of the file owner as an integer value.
     * @throw std::system_error if the group ID cannot be determined
     */
    [[nodiscard]] int gid() const;

    /**
     * @brief Gets the path of the file.
     *
     * @return The path of the file as an `fs::path` object.
     */
    [[nodiscard]] fs::path path() const;

    /**
     * @brief Gets the number of hard links to the file.
     *
     * @return The number of hard links to the file.
     * @throw std::system_error if the hard link count cannot be determined
     */
    [[nodiscard]] std::uintmax_t hardLinkCount() const;

    /**
     * @brief Gets the device ID of the file.
     *
     * @return The device ID of the file.
     * @throw std::system_error if the device ID cannot be determined
     */
    [[nodiscard]] std::uintmax_t deviceId() const;

    /**
     * @brief Gets the inode number of the file.
     *
     * @return The inode number of the file.
     * @throw std::system_error if the inode number cannot be determined
     */
    [[nodiscard]] std::uintmax_t inodeNumber() const;

    /**
     * @brief Gets the block size for the file system.
     *
     * @return The block size for the file system.
     * @throw std::system_error if the block size cannot be determined
     */
    [[nodiscard]] std::uintmax_t blockSize() const;

    /**
     * @brief Gets the username of the file owner.
     *
     * @return The username of the file owner.
     * @throw std::system_error if the username cannot be determined
     */
    [[nodiscard]] std::string ownerName() const;

    /**
     * @brief Gets the group name of the file.
     *
     * @return The group name of the file.
     * @throw std::system_error if the group name cannot be determined
     */
    [[nodiscard]] std::string groupName() const;

    /**
     * @brief Checks if the file is a symbolic link.
     *
     * @return true if the file is a symbolic link, false otherwise.
     */
    [[nodiscard]] bool isSymlink() const;

    /**
     * @brief Checks if the file is a directory.
     *
     * @return true if the file is a directory, false otherwise.
     */
    [[nodiscard]] bool isDirectory() const;

    /**
     * @brief Checks if the file is a regular file.
     *
     * @return true if the file is a regular file, false otherwise.
     */
    [[nodiscard]] bool isRegularFile() const;

    /**
     * @brief Checks if the file is readable by the current user.
     *
     * @return true if the file is readable, false otherwise.
     */
    [[nodiscard]] bool isReadable() const;

    /**
     * @brief Checks if the file is writable by the current user.
     *
     * @return true if the file is writable, false otherwise.
     */
    [[nodiscard]] bool isWritable() const;

    /**
     * @brief Checks if the file is executable by the current user.
     *
     * @return true if the file is executable, false otherwise.
     */
    [[nodiscard]] bool isExecutable() const;

    /**
     * @brief Checks if the file has specific permission.
     *
     * @param user Check for user permissions
     * @param group Check for group permissions
     * @param others Check for others permissions
     * @param permission The permission to check (Read, Write, or Execute)
     * @return true if the permission is granted, false otherwise.
     */
    [[nodiscard]] bool hasPermission(bool user, bool group, bool others,
                                     FilePermission permission) const;

    /**
     * @brief Gets the target path if the file is a symbolic link.
     *
     * @return The target path of the symbolic link. Empty if not a symlink.
     */
    [[nodiscard]] fs::path symlinkTarget();

    /**
     * @brief Formats the file time (atime, mtime, ctime) as a string.
     *
     * @param time The time to format.
     * @param format The format string (default: "%Y-%m-%d %H:%M:%S").
     * @return The formatted time string.
     */
    [[nodiscard]] static std::string formatTime(
        std::time_t time, const std::string& format = "%Y-%m-%d %H:%M:%S");

private:
    fs::path path_;        ///< The path to the file.
    bool followSymlinks_;  ///< Whether to follow symlinks
    std::error_code
        ec_;  ///< The error code for handling errors during file operations.

    // Cache structure to store stat info
    class StatInfo;
    std::unique_ptr<StatInfo> statInfo_;  ///< Cached stat information

    // Helper methods
    void initStatInfo();
    void checkFileExists() const;
    void ensureStatInfoCached() const;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_STAT_HPP