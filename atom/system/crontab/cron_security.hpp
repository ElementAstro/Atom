#ifndef CRON_SECURITY_HPP
#define CRON_SECURITY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>

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
        : user_id(std::move(uid)), session_id(std::move(sid)),
          created_at(std::chrono::system_clock::now()),
          expires_at(std::chrono::system_clock::now() + std::chrono::hours(8)) {}
};

/**
 * @brief Resource limits for job execution
 */
struct ResourceLimits {
    size_t max_memory_mb{1024};        // Maximum memory usage in MB
    std::chrono::seconds max_cpu_time{300}; // Maximum CPU time
    std::chrono::seconds max_wall_time{600}; // Maximum wall clock time
    size_t max_file_size_mb{100};      // Maximum file size for output
    size_t max_processes{10};          // Maximum number of processes
    size_t max_open_files{100};        // Maximum number of open files
    double max_cpu_percent{50.0};      // Maximum CPU percentage
    
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
        // Default safe paths
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
    
    SecurityEvent(std::string uid, std::string act, std::string res, bool succ, std::string rsn)
        : user_id(std::move(uid)), action(std::move(act)), resource(std::move(res)),
          success(succ), reason(std::move(rsn)), timestamp(std::chrono::system_clock::now()) {}
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
        : user_id(std::move(uid)), username(std::move(uname)),
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
        : role_id(std::move(rid)), name(std::move(n)), description(std::move(desc)) {}
};

/**
 * @brief Comprehensive security and resource management system
 */
class CronSecurity {
public:
    /**
     * @brief Constructs a new CronSecurity manager
     */
    CronSecurity();
    
    /**
     * @brief Destructor
     */
    ~CronSecurity();
    
    // Disable copy operations
    CronSecurity(const CronSecurity&) = delete;
    CronSecurity& operator=(const CronSecurity&) = delete;
    
    // Enable move operations
    CronSecurity(CronSecurity&&) noexcept = default;
    CronSecurity& operator=(CronSecurity&&) noexcept = default;
    
    /**
     * @brief Initializes the security system
     * @return True if initialized successfully
     */
    auto initialize() -> bool;
    
    /**
     * @brief Shuts down the security system
     */
    void shutdown();
    
    // User and role management
    /**
     * @brief Creates a new user account
     * @param username Username
     * @param email Email address
     * @param roles Initial roles
     * @return User ID if created successfully
     */
    auto createUser(const std::string& username, const std::string& email,
                   const std::vector<std::string>& roles = {}) -> std::optional<std::string>;
    
    /**
     * @brief Deletes a user account
     * @param user_id User identifier
     * @return True if deleted successfully
     */
    auto deleteUser(const std::string& user_id) -> bool;
    
    /**
     * @brief Creates a new role
     * @param name Role name
     * @param description Role description
     * @param permissions Role permissions
     * @return Role ID if created successfully
     */
    auto createRole(const std::string& name, const std::string& description,
                   const std::unordered_map<std::string, PermissionLevel>& permissions = {})
        -> std::optional<std::string>;
    
    /**
     * @brief Assigns a role to a user
     * @param user_id User identifier
     * @param role_id Role identifier
     * @return True if assigned successfully
     */
    auto assignRole(const std::string& user_id, const std::string& role_id) -> bool;
    
    /**
     * @brief Removes a role from a user
     * @param user_id User identifier
     * @param role_id Role identifier
     * @return True if removed successfully
     */
    auto removeRole(const std::string& user_id, const std::string& role_id) -> bool;
    
    // Authentication and authorization
    /**
     * @brief Authenticates a user and creates a security context
     * @param username Username
     * @param password Password (or token)
     * @return Security context if authentication successful
     */
    auto authenticate(const std::string& username, const std::string& password)
        -> std::optional<SecurityContext>;
    
    /**
     * @brief Validates a security context
     * @param context Security context to validate
     * @return True if context is valid and not expired
     */
    auto validateContext(const SecurityContext& context) -> bool;
    
    /**
     * @brief Checks if a user has permission for a specific action on a resource
     * @param context Security context
     * @param action Action to perform
     * @param resource Resource identifier
     * @return True if permission granted
     */
    auto checkPermission(const SecurityContext& context, const std::string& action,
                        const std::string& resource) -> bool;
    
    /**
     * @brief Checks if a user can execute a specific command
     * @param context Security context
     * @param command Command to execute
     * @return True if command is allowed
     */
    auto checkCommandPermission(const SecurityContext& context, const std::string& command) -> bool;
    
    // Resource management
    /**
     * @brief Sets resource limits for a user or job
     * @param identifier User ID or job ID
     * @param limits Resource limits
     */
    void setResourceLimits(const std::string& identifier, const ResourceLimits& limits);
    
    /**
     * @brief Gets resource limits for a user or job
     * @param identifier User ID or job ID
     * @return Resource limits if found
     */
    auto getResourceLimits(const std::string& identifier) -> std::optional<ResourceLimits>;
    
    /**
     * @brief Monitors resource usage for a process
     * @param process_id Process identifier
     * @param limits Resource limits to enforce
     * @return True if within limits, false if exceeded
     */
    auto monitorResourceUsage(int process_id, const ResourceLimits& limits) -> bool;
    
    // Sandboxing
    /**
     * @brief Sets sandbox configuration for a user or job
     * @param identifier User ID or job ID
     * @param config Sandbox configuration
     */
    void setSandboxConfig(const std::string& identifier, const SandboxConfig& config);
    
    /**
     * @brief Gets sandbox configuration for a user or job
     * @param identifier User ID or job ID
     * @return Sandbox configuration if found
     */
    auto getSandboxConfig(const std::string& identifier) -> std::optional<SandboxConfig>;
    
    /**
     * @brief Creates a sandboxed execution environment
     * @param config Sandbox configuration
     * @return Sandbox ID if created successfully
     */
    auto createSandbox(const SandboxConfig& config) -> std::optional<std::string>;
    
    /**
     * @brief Destroys a sandbox environment
     * @param sandbox_id Sandbox identifier
     * @return True if destroyed successfully
     */
    auto destroySandbox(const std::string& sandbox_id) -> bool;
    
    // Security validation
    /**
     * @brief Validates a command for security issues
     * @param command Command to validate
     * @param context Security context
     * @return True if command is safe to execute
     */
    auto validateCommand(const std::string& command, const SecurityContext& context) -> bool;
    
    /**
     * @brief Validates a file path for security issues
     * @param path File path to validate
     * @param context Security context
     * @return True if path is safe to access
     */
    auto validatePath(const std::string& path, const SecurityContext& context) -> bool;
    
    /**
     * @brief Scans a command for potential security vulnerabilities
     * @param command Command to scan
     * @return Vector of security issues found
     */
    auto scanCommand(const std::string& command) -> std::vector<std::string>;
    
    // Audit logging
    /**
     * @brief Logs a security event
     * @param event Security event to log
     */
    void logSecurityEvent(const SecurityEvent& event);
    
    /**
     * @brief Gets security audit log
     * @param start_time Start of time range
     * @param end_time End of time range
     * @param user_id Optional user filter
     * @return Vector of security events
     */
    auto getAuditLog(std::chrono::system_clock::time_point start_time,
                    std::chrono::system_clock::time_point end_time,
                    const std::string& user_id = "") -> std::vector<SecurityEvent>;
    
    // Configuration
    /**
     * @brief Enables or disables security features
     * @param feature Feature name
     * @param enabled Whether to enable the feature
     */
    void setSecurityFeature(const std::string& feature, bool enabled);
    
    /**
     * @brief Gets the status of a security feature
     * @param feature Feature name
     * @return True if feature is enabled
     */
    auto isSecurityFeatureEnabled(const std::string& feature) -> bool;

private:
    mutable std::mutex security_mutex_;
    std::atomic<bool> initialized_{false};
    
    // User and role storage
    std::unordered_map<std::string, UserAccount> users_;
    std::unordered_map<std::string, std::string> username_to_id_;
    std::unordered_map<std::string, Role> roles_;
    
    // Active sessions
    std::unordered_map<std::string, SecurityContext> active_sessions_;
    
    // Resource management
    std::unordered_map<std::string, ResourceLimits> resource_limits_;
    std::unordered_map<std::string, SandboxConfig> sandbox_configs_;
    std::unordered_map<std::string, std::string> active_sandboxes_;
    
    // Security configuration
    std::unordered_map<std::string, bool> security_features_;
    
    // Audit log
    std::vector<SecurityEvent> audit_log_;
    static constexpr size_t MAX_AUDIT_LOG_SIZE = 10000;
    
    // Internal methods
    auto generateUserId() -> std::string;
    auto generateRoleId() -> std::string;
    auto generateSessionId() -> std::string;
    auto generateEventId() -> std::string;
    auto hashPassword(const std::string& password) -> std::string;
    auto verifyPassword(const std::string& password, const std::string& hash) -> bool;
    auto calculateEffectivePermissions(const UserAccount& user) -> std::unordered_map<std::string, PermissionLevel>;
    void cleanupExpiredSessions();
    void initializeDefaultRoles();
};

#endif // CRON_SECURITY_HPP
