/*
 * cron_security_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_SECURITY_TYPES_HPP
#define CRON_SECURITY_TYPES_HPP

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief User permission levels
 */
enum class PermissionLevel {
    NONE = 0,
    READ = 1,
    WRITE = 2,
    EXECUTE = 3,
    ADMIN = 4
};

/**
 * @brief Security context for operations
 */
struct SecurityContext {
    std::string user_id;
    std::string session_id;
    std::vector<std::string> roles;
    std::unordered_map<std::string, std::string> attributes;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;

    SecurityContext(std::string uid, std::string sid)
        : user_id(std::move(uid)),
          session_id(std::move(sid)),
          created_at(std::chrono::system_clock::now()),
          expires_at(std::chrono::system_clock::now() +
                     std::chrono::hours(8)) {}
};

/**
 * @brief Resource limits for job execution
 */
struct ResourceLimits {
    size_t max_memory_mb{1024};
    std::chrono::seconds max_cpu_time{300};
    std::chrono::seconds max_wall_time{600};
    size_t max_file_size_mb{100};
    size_t max_processes{10};
    size_t max_open_files{100};
    double max_cpu_percent{50.0};

    ResourceLimits() = default;
};

/**
 * @brief Sandbox configuration
 */
struct SandboxConfig {
    bool enable_filesystem_isolation{true};
    bool enable_network_isolation{false};
    bool enable_process_isolation{true};
    std::vector<std::string> allowed_paths;
    std::vector<std::string> blocked_paths;
    std::vector<std::string> allowed_commands;
    std::vector<std::string> blocked_commands;
    std::string chroot_directory;
    std::string user_namespace;

    SandboxConfig() {
        allowed_paths = {"/tmp", "/var/tmp", "/usr/bin", "/bin"};
        blocked_paths = {"/etc/passwd", "/etc/shadow", "/root", "/home"};
    }
};

/**
 * @brief Security audit event
 */
struct SecurityEvent {
    std::string event_id;
    std::string user_id;
    std::string action;
    std::string resource;
    bool success;
    std::string reason;
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    SecurityEvent(std::string uid, std::string act, std::string res, bool succ,
                  std::string rsn)
        : user_id(std::move(uid)),
          action(std::move(act)),
          resource(std::move(res)),
          success(succ),
          reason(std::move(rsn)),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief User account information
 */
struct UserAccount {
    std::string user_id;
    std::string username;
    std::string email;
    std::vector<std::string> roles;
    PermissionLevel default_permission{PermissionLevel::READ};
    std::unordered_map<std::string, PermissionLevel> resource_permissions;
    bool is_active{true};
    bool is_locked{false};
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    int failed_login_attempts{0};

    UserAccount(std::string uid, std::string uname)
        : user_id(std::move(uid)),
          username(std::move(uname)),
          created_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Role definition
 */
struct Role {
    std::string role_id;
    std::string name;
    std::string description;
    std::unordered_map<std::string, PermissionLevel> permissions;
    std::vector<std::string> inherited_roles;

    Role(std::string rid, std::string n, std::string desc)
        : role_id(std::move(rid)),
          name(std::move(n)),
          description(std::move(desc)) {}
};

#endif  // CRON_SECURITY_TYPES_HPP
