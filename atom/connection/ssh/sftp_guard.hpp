/*
 * sftp_guard.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-29

Description: RAII guards for SFTP resources

*************************************************/

#ifndef ATOM_CONNECTION_SFTP_GUARD_HPP
#define ATOM_CONNECTION_SFTP_GUARD_HPP

#include <cstdio>

#if __has_include(<libssh/libssh.h>)
#include <libssh/libssh.h>
#include <libssh/sftp.h>

namespace atom::connection {

/**
 * @brief RAII guard for sftp_file resources.
 *
 * Automatically closes the SFTP file handle when the guard goes out of scope.
 */
class SftpFileGuard {
public:
    explicit SftpFileGuard(sftp_file file = nullptr) noexcept : file_(file) {}

    ~SftpFileGuard() { reset(); }

    SftpFileGuard(const SftpFileGuard&) = delete;
    auto operator=(const SftpFileGuard&) -> SftpFileGuard& = delete;

    SftpFileGuard(SftpFileGuard&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }

    auto operator=(SftpFileGuard&& other) noexcept -> SftpFileGuard& {
        if (this != &other) {
            reset();
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> sftp_file { return file_; }

    [[nodiscard]] explicit operator bool() const noexcept {
        return file_ != nullptr;
    }

    auto release() noexcept -> sftp_file {
        sftp_file tmp = file_;
        file_ = nullptr;
        return tmp;
    }

    void reset(sftp_file file = nullptr) noexcept {
        if (file_ != nullptr) {
            sftp_close(file_);
        }
        file_ = file;
    }

private:
    sftp_file file_;
};

/**
 * @brief RAII guard for sftp_dir resources.
 *
 * Automatically closes the SFTP directory handle when the guard goes out of
 * scope.
 */
class SftpDirGuard {
public:
    explicit SftpDirGuard(sftp_dir dir = nullptr) noexcept : dir_(dir) {}

    ~SftpDirGuard() { reset(); }

    SftpDirGuard(const SftpDirGuard&) = delete;
    auto operator=(const SftpDirGuard&) -> SftpDirGuard& = delete;

    SftpDirGuard(SftpDirGuard&& other) noexcept : dir_(other.dir_) {
        other.dir_ = nullptr;
    }

    auto operator=(SftpDirGuard&& other) noexcept -> SftpDirGuard& {
        if (this != &other) {
            reset();
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> sftp_dir { return dir_; }

    [[nodiscard]] explicit operator bool() const noexcept {
        return dir_ != nullptr;
    }

    auto release() noexcept -> sftp_dir {
        sftp_dir tmp = dir_;
        dir_ = nullptr;
        return tmp;
    }

    void reset(sftp_dir dir = nullptr) noexcept {
        if (dir_ != nullptr) {
            sftp_closedir(dir_);
        }
        dir_ = dir;
    }

private:
    sftp_dir dir_;
};

/**
 * @brief RAII guard for sftp_attributes resources.
 *
 * Automatically frees the SFTP attributes when the guard goes out of scope.
 */
class SftpAttributesGuard {
public:
    explicit SftpAttributesGuard(sftp_attributes attrs = nullptr) noexcept
        : attrs_(attrs) {}

    ~SftpAttributesGuard() { reset(); }

    SftpAttributesGuard(const SftpAttributesGuard&) = delete;
    auto operator=(const SftpAttributesGuard&) -> SftpAttributesGuard& = delete;

    SftpAttributesGuard(SftpAttributesGuard&& other) noexcept
        : attrs_(other.attrs_) {
        other.attrs_ = nullptr;
    }

    auto operator=(SftpAttributesGuard&& other) noexcept
        -> SftpAttributesGuard& {
        if (this != &other) {
            reset();
            attrs_ = other.attrs_;
            other.attrs_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> sftp_attributes {
        return attrs_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return attrs_ != nullptr;
    }

    auto release() noexcept -> sftp_attributes {
        sftp_attributes tmp = attrs_;
        attrs_ = nullptr;
        return tmp;
    }

    void reset(sftp_attributes attrs = nullptr) noexcept {
        if (attrs_ != nullptr) {
            sftp_attributes_free(attrs_);
        }
        attrs_ = attrs;
    }

private:
    sftp_attributes attrs_;
};

/**
 * @brief RAII guard for local FILE* resources.
 *
 * Automatically closes the file handle when the guard goes out of scope.
 */
class LocalFileGuard {
public:
    explicit LocalFileGuard(FILE* fp = nullptr) noexcept : fp_(fp) {}

    ~LocalFileGuard() { reset(); }

    LocalFileGuard(const LocalFileGuard&) = delete;
    auto operator=(const LocalFileGuard&) -> LocalFileGuard& = delete;

    LocalFileGuard(LocalFileGuard&& other) noexcept : fp_(other.fp_) {
        other.fp_ = nullptr;
    }

    auto operator=(LocalFileGuard&& other) noexcept -> LocalFileGuard& {
        if (this != &other) {
            reset();
            fp_ = other.fp_;
            other.fp_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> FILE* { return fp_; }

    [[nodiscard]] explicit operator bool() const noexcept {
        return fp_ != nullptr;
    }

    auto release() noexcept -> FILE* {
        FILE* tmp = fp_;
        fp_ = nullptr;
        return tmp;
    }

    void reset(FILE* fp = nullptr) noexcept {
        if (fp_ != nullptr) {
            std::fclose(fp_);
        }
        fp_ = fp;
    }

private:
    FILE* fp_;
};

/**
 * @brief RAII guard for ssh_channel resources.
 *
 * Automatically sends EOF, closes and frees the SSH channel when the guard
 * goes out of scope.
 */
class SshChannelGuard {
public:
    explicit SshChannelGuard(ssh_channel channel = nullptr) noexcept
        : channel_(channel) {}

    ~SshChannelGuard() { reset(); }

    SshChannelGuard(const SshChannelGuard&) = delete;
    auto operator=(const SshChannelGuard&) -> SshChannelGuard& = delete;

    SshChannelGuard(SshChannelGuard&& other) noexcept
        : channel_(other.channel_) {
        other.channel_ = nullptr;
    }

    auto operator=(SshChannelGuard&& other) noexcept -> SshChannelGuard& {
        if (this != &other) {
            reset();
            channel_ = other.channel_;
            other.channel_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] auto get() const noexcept -> ssh_channel { return channel_; }

    [[nodiscard]] explicit operator bool() const noexcept {
        return channel_ != nullptr;
    }

    auto release() noexcept -> ssh_channel {
        ssh_channel tmp = channel_;
        channel_ = nullptr;
        return tmp;
    }

    void reset(ssh_channel channel = nullptr) noexcept {
        if (channel_ != nullptr) {
            ssh_channel_send_eof(channel_);
            ssh_channel_close(channel_);
            ssh_channel_free(channel_);
        }
        channel_ = channel;
    }

private:
    ssh_channel channel_;
};

}  // namespace atom::connection

#endif  // __has_include(<libssh/libssh.h>)

#endif  // ATOM_CONNECTION_SFTP_GUARD_HPP
