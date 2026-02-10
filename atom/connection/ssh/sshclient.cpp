/*
 * sshclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: SSH Client

*************************************************/

#include "sshclient.hpp"

#include <fcntl.h>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <vector>

#include "atom/error/exception.hpp"
#include "sftp_guard.hpp"

namespace fs = std::filesystem;

namespace atom::connection {
SSHClient::SSHClient(const std::string &host, int port)
    : host_(host), port_(port), ssh_session_(nullptr), sftp_session_(nullptr) {}

SSHClient::SSHClient(SSHClient &&other) noexcept
    : host_(std::move(other.host_)),
      port_(other.port_),
      ssh_session_(other.ssh_session_),
      sftp_session_(other.sftp_session_) {
    other.ssh_session_ = nullptr;
    other.sftp_session_ = nullptr;
}

auto SSHClient::operator=(SSHClient &&other) noexcept -> SSHClient & {
    if (this == &other) {
        return *this;
    }

    cleanup();

    host_ = std::move(other.host_);
    port_ = other.port_;
    ssh_session_ = other.ssh_session_;
    sftp_session_ = other.sftp_session_;

    other.ssh_session_ = nullptr;
    other.sftp_session_ = nullptr;

    return *this;
}

SSHClient::~SSHClient() { cleanup(); }

void SSHClient::cleanup() noexcept {
    if (sftp_session_ != nullptr) {
        sftp_free(sftp_session_);
        sftp_session_ = nullptr;
    }
    if (ssh_session_ != nullptr) {
        ssh_disconnect(ssh_session_);
        ssh_free(ssh_session_);
        ssh_session_ = nullptr;
    }
}

void SSHClient::connect(const std::string &username,
                        const std::string &password, int timeout) {
    cleanup();

    ssh_session_ = ssh_new();
    if (ssh_session_ == nullptr) {
        THROW_RUNTIME_ERROR("Failed to create SSH session.");
    }

    try {
        ssh_options_set(ssh_session_, SSH_OPTIONS_HOST, host_.c_str());
        ssh_options_set(ssh_session_, SSH_OPTIONS_PORT, &port_);
        ssh_options_set(ssh_session_, SSH_OPTIONS_USER, username.c_str());
        ssh_options_set(ssh_session_, SSH_OPTIONS_TIMEOUT, &timeout);

        int rc = ssh_connect(ssh_session_);
        if (rc != SSH_OK) {
            THROW_RUNTIME_ERROR("Failed to connect to SSH server: " +
                                std::string(ssh_get_error(ssh_session_)));
        }

        verifyServerIdentity();

        rc = ssh_userauth_password(ssh_session_, nullptr, password.c_str());
        if (rc != SSH_AUTH_SUCCESS) {
            THROW_RUNTIME_ERROR("Failed to authenticate with SSH server: " +
                                std::string(ssh_get_error(ssh_session_)));
        }

        sftp_session_ = sftp_new(ssh_session_);
        if (sftp_session_ == nullptr) {
            THROW_RUNTIME_ERROR("Failed to create SFTP session.");
        }

        rc = sftp_init(sftp_session_);
        if (rc != SSH_OK) {
            THROW_RUNTIME_ERROR("Failed to initialize SFTP session: " +
                                std::string(ssh_get_error(ssh_session_)));
        }
    } catch (...) {
        cleanup();
        throw;
    }
}

bool SSHClient::isConnected() const {
    return (ssh_session_ != nullptr && sftp_session_ != nullptr);
}

void SSHClient::disconnect() { cleanup(); }

void SSHClient::executeCommand(const std::string &command,
                               std::vector<std::string> &output) {
    ensureConnected();

    SshChannelGuard channel(ssh_channel_new(ssh_session_));
    if (!channel) {
        THROW_RUNTIME_ERROR("Failed to create SSH channel.");
    }

    int rc = ssh_channel_open_session(channel.get());
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to open SSH channel: " +
                            std::string(ssh_get_error(ssh_session_)));
    }

    rc = ssh_channel_request_exec(channel.get(), command.c_str());
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to execute command: " +
                            std::string(ssh_get_error(ssh_session_)));
    }

    constexpr int COMMAND_BUFFER_SIZE = 4096;
    char buffer[COMMAND_BUFFER_SIZE];
    int nbytes = 0;
    while ((nbytes = ssh_channel_read(channel.get(), buffer, sizeof(buffer),
                                      0)) > 0) {
        output.emplace_back(buffer, nbytes);
    }

    if (nbytes < 0) {
        THROW_RUNTIME_ERROR("Failed to read command output: " +
                            std::string(ssh_get_error(ssh_session_)));
    }
}

void SSHClient::executeCommands(const std::vector<std::string> &commands,
                                std::vector<std::vector<std::string>> &output) {
    ensureConnected();

    output.clear();
    output.reserve(commands.size());

    for (const auto &cmd : commands) {
        std::vector<std::string> cmd_output;
        executeCommand(cmd, cmd_output);
        output.push_back(std::move(cmd_output));
    }
}

bool SSHClient::fileExists(const std::string &remote_path) const {
    ensureConnected();

    SftpAttributesGuard attrs(sftp_stat(sftp_session_, remote_path.c_str()));
    if (attrs) {
        return true;
    }

    const int err = sftp_get_error(sftp_session_);
    if (err == SSH_FX_NO_SUCH_FILE || err == SSH_FX_NO_SUCH_PATH) {
        return false;
    }

    THROW_RUNTIME_ERROR("Failed to stat remote path '" + remote_path +
                        "': error code " + std::to_string(err));
}

void SSHClient::createDirectory(const std::string &remote_path, int mode) {
    ensureConnected();

    const int rc = sftp_mkdir(sftp_session_, remote_path.c_str(), mode);
    if (rc == SSH_OK) {
        return;
    }

    const int err = sftp_get_error(sftp_session_);
    if (err == SSH_FX_FILE_ALREADY_EXISTS) {
        return;
    }

    THROW_RUNTIME_ERROR("Failed to create remote directory '" + remote_path +
                        "': error code " + std::to_string(err));
}

void SSHClient::removeFile(const std::string &remote_path) {
    ensureConnected();

    const int rc = sftp_unlink(sftp_session_, remote_path.c_str());
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to remove remote file '" + remote_path +
                            "': error code " +
                            std::to_string(sftp_get_error(sftp_session_)));
    }
}

void SSHClient::removeDirectory(const std::string &remote_path) {
    ensureConnected();

    const int rc = sftp_rmdir(sftp_session_, remote_path.c_str());
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to remove remote directory '" +
                            remote_path + "': error code " +
                            std::to_string(sftp_get_error(sftp_session_)));
    }
}

std::vector<std::string> SSHClient::listDirectory(
    const std::string &remote_path) const {
    std::vector<std::string> file_list;
    ensureConnected();

    SftpDirGuard dir(sftp_opendir(sftp_session_, remote_path.c_str()));
    if (!dir) {
        THROW_RUNTIME_ERROR("Failed to open remote directory '" + remote_path +
                            "': error code " +
                            std::to_string(sftp_get_error(sftp_session_)));
    }

    sftp_attributes attributes = nullptr;
    while ((attributes = sftp_readdir(sftp_session_, dir.get())) != nullptr) {
        SftpAttributesGuard attrsGuard(attributes);
        file_list.emplace_back(attrsGuard.get()->name);
    }

    return file_list;
}

void SSHClient::rename(const std::string &old_path,
                       const std::string &new_path) {
    ensureConnected();

    const int rc =
        sftp_rename(sftp_session_, old_path.c_str(), new_path.c_str());
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to rename '" + old_path + "' to '" +
                            new_path + "': error code " +
                            std::to_string(sftp_get_error(sftp_session_)));
    }
}

void SSHClient::getFileInfo(const std::string &remote_path,
                            sftp_attributes &attrs) {
    ensureConnected();

    if (attrs != nullptr) {
        sftp_attributes_free(attrs);
        attrs = nullptr;
    }

    attrs = sftp_stat(sftp_session_, remote_path.c_str());
    if (attrs == nullptr) {
        THROW_RUNTIME_ERROR("Failed to get file info for remote path '" +
                            remote_path + "': error code " +
                            std::to_string(sftp_get_error(sftp_session_)));
    }
}

void SSHClient::downloadFile(const std::string &remote_path,
                             const std::string &local_path) {
    ensureConnected();

    constexpr size_t TRANSFER_BUFFER_SIZE = 65536;  // 64KB for better perf

    SftpFileGuard remoteFile(
        sftp_open(sftp_session_, remote_path.c_str(), O_RDONLY, 0));
    if (!remoteFile) {
        THROW_RUNTIME_ERROR("Failed to open remote file for download: " +
                            remote_path);
    }

    LocalFileGuard localFile(std::fopen(local_path.c_str(), "wb"));
    if (!localFile) {
        THROW_RUNTIME_ERROR("Failed to open local file for download: " +
                            local_path);
    }

    std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
    int nbytes = 0;
    while ((nbytes = sftp_read(remoteFile.get(), buffer.data(),
                               buffer.size())) > 0) {
        const size_t written =
            std::fwrite(buffer.data(), 1, nbytes, localFile.get());
        if (written != static_cast<size_t>(nbytes)) {
            THROW_RUNTIME_ERROR("Failed to write to local file: " + local_path);
        }
    }

    if (nbytes < 0) {
        const int err = sftp_get_error(sftp_session_);
        THROW_RUNTIME_ERROR("Failed to download file '" + remote_path +
                            "': error code " + std::to_string(err));
    }
}

void SSHClient::uploadFile(const std::string &local_path,
                           const std::string &remote_path) {
    ensureConnected();

    constexpr int DEFAULT_FILE_PERMISSIONS = 0644;
    constexpr size_t TRANSFER_BUFFER_SIZE = 65536;  // 64KB for better perf

    SftpFileGuard remoteFile(sftp_open(sftp_session_, remote_path.c_str(),
                                       O_WRONLY | O_CREAT | O_TRUNC,
                                       DEFAULT_FILE_PERMISSIONS));
    if (!remoteFile) {
        THROW_RUNTIME_ERROR("Failed to open remote file for upload: " +
                            remote_path);
    }

    LocalFileGuard localFile(std::fopen(local_path.c_str(), "rb"));
    if (!localFile) {
        THROW_RUNTIME_ERROR("Failed to open local file for upload: " +
                            local_path);
    }

    std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
    size_t nbytes = 0;
    while ((nbytes = std::fread(buffer.data(), 1, buffer.size(),
                                localFile.get())) > 0) {
        size_t written_total = 0;
        while (written_total < nbytes) {
            const int written =
                sftp_write(remoteFile.get(), buffer.data() + written_total,
                           nbytes - written_total);
            if (written < 0) {
                const int err = sftp_get_error(sftp_session_);
                THROW_RUNTIME_ERROR("Failed to upload file '" + remote_path +
                                    "': error code " + std::to_string(err));
            }
            written_total += static_cast<size_t>(written);
        }
    }

    if (std::ferror(localFile.get()) != 0) {
        THROW_RUNTIME_ERROR("Failed to read from local file: " + local_path);
    }
}

void SSHClient::uploadDirectory(const std::string &local_path,
                                const std::string &remote_path) {
    ensureConnected();

    for (const auto &entry : fs::recursive_directory_iterator(local_path)) {
        const auto &path = entry.path();
        const auto relativePath = fs::relative(path, local_path);
        const auto remoteFilePath = remote_path + "/" + relativePath.string();

        if (entry.is_directory()) {
            createDirectory(remoteFilePath);
        } else if (entry.is_regular_file()) {
            uploadFile(path.string(), remoteFilePath);
        }
    }
}

void SSHClient::verifyServerIdentity() {
    ssh_key server_key = nullptr;
    unsigned char *hash = nullptr;
    size_t hash_len = 0;

    int rc = ssh_get_publickey(ssh_session_, &server_key);
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to obtain server public key: " +
                            std::string(ssh_get_error(ssh_session_)));
    }

    rc = ssh_get_publickey_hash(server_key, SSH_PUBLICKEY_HASH_SHA256, &hash,
                                &hash_len);
    ssh_key_free(server_key);
    if (rc != SSH_OK) {
        THROW_RUNTIME_ERROR("Failed to compute server key fingerprint: " +
                            std::string(ssh_get_error(ssh_session_)));
    }

    std::ostringstream fingerprint_stream;
    if (hash != nullptr && hash_len > 0U) {
        fingerprint_stream << std::hex << std::setfill('0');
        for (size_t i = 0; i < hash_len; ++i) {
            fingerprint_stream << std::setw(2) << static_cast<int>(hash[i]);
            if (i + 1 < hash_len) {
                fingerprint_stream << ':';
            }
        }
    }
    const std::string fingerprint = fingerprint_stream.str();

    const int state = ssh_is_server_known(ssh_session_);
    ssh_clean_pubkey_hash(&hash);

    switch (state) {
        case SSH_SERVER_KNOWN_OK:
            return;
        case SSH_SERVER_FILE_NOT_FOUND:
        case SSH_SERVER_NOT_KNOWN:
            THROW_RUNTIME_ERROR(
                "Server identity is unknown. Fingerprint: " + fingerprint +
                ". Verify the host key and add it to known_hosts.");
        case SSH_SERVER_KNOWN_CHANGED:
            THROW_RUNTIME_ERROR(
                "Server host key has changed. Possible MITM attack. "
                "Fingerprint: " +
                fingerprint);
        case SSH_SERVER_FOUND_OTHER:
            THROW_RUNTIME_ERROR(
                "A different host key type was found. Fingerprint: " +
                fingerprint);
        case SSH_SERVER_ERROR:
        default:
            THROW_RUNTIME_ERROR("Failed to verify server identity: " +
                                std::string(ssh_get_error(ssh_session_)));
    }
}

void SSHClient::ensureConnected() const {
    if (ssh_session_ == nullptr || sftp_session_ == nullptr) {
        THROW_RUNTIME_ERROR("SSH client is not connected.");
    }
}

}  // namespace atom::connection
