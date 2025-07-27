#include "cron_security.hpp"

#include <algorithm>
#include <random>
#include <regex>
#include <fstream>
#include <filesystem>

#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

CronSecurity::CronSecurity() {
    // Initialize default security features
    security_features_["authentication"] = true;
    security_features_["authorization"] = true;
    security_features_["resource_limits"] = true;
    security_features_["sandboxing"] = true;
    security_features_["command_validation"] = true;
    security_features_["audit_logging"] = true;
    
    spdlog::info("CronSecurity initialized");
}

CronSecurity::~CronSecurity() {
    shutdown();
    spdlog::info("CronSecurity destroyed");
}

auto CronSecurity::initialize() -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    if (initialized_.load()) {
        spdlog::warn("CronSecurity is already initialized");
        return true;
    }
    
    try {
        // Initialize default roles
        initializeDefaultRoles();
        
        // Create default admin user
        auto admin_id = createUser("admin", "admin@localhost", {"admin"});
        if (!admin_id.has_value()) {
            spdlog::error("Failed to create default admin user");
            return false;
        }
        
        initialized_.store(true);
        spdlog::info("CronSecurity initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize CronSecurity: {}", e.what());
        return false;
    }
}

void CronSecurity::shutdown() {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    if (!initialized_.load()) {
        return;
    }
    
    // Clear all data
    users_.clear();
    username_to_id_.clear();
    roles_.clear();
    active_sessions_.clear();
    resource_limits_.clear();
    sandbox_configs_.clear();
    active_sandboxes_.clear();
    
    initialized_.store(false);
    spdlog::info("CronSecurity shutdown completed");
}

auto CronSecurity::createUser(const std::string& username, const std::string& email,
                             const std::vector<std::string>& roles) -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    // Check if username already exists
    if (username_to_id_.find(username) != username_to_id_.end()) {
        spdlog::warn("Username {} already exists", username);
        return std::nullopt;
    }
    
    std::string user_id = generateUserId();
    UserAccount user(user_id, username);
    user.email = email;
    user.roles = roles;
    
    // Validate roles exist
    for (const auto& role : roles) {
        if (roles_.find(role) == roles_.end()) {
            spdlog::warn("Role {} does not exist", role);
            return std::nullopt;
        }
    }
    
    users_[user_id] = user;
    username_to_id_[username] = user_id;
    
    logSecurityEvent(SecurityEvent(user_id, "create_user", username, true, "User created"));
    spdlog::info("Created user: {} ({})", username, user_id);
    
    return user_id;
}

auto CronSecurity::deleteUser(const std::string& user_id) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        spdlog::warn("User {} not found", user_id);
        return false;
    }
    
    std::string username = user_it->second.username;
    
    // Remove from all data structures
    users_.erase(user_it);
    username_to_id_.erase(username);
    
    // Remove active sessions
    auto session_it = active_sessions_.begin();
    while (session_it != active_sessions_.end()) {
        if (session_it->second.user_id == user_id) {
            session_it = active_sessions_.erase(session_it);
        } else {
            ++session_it;
        }
    }
    
    logSecurityEvent(SecurityEvent(user_id, "delete_user", username, true, "User deleted"));
    spdlog::info("Deleted user: {} ({})", username, user_id);
    
    return true;
}

auto CronSecurity::createRole(const std::string& name, const std::string& description,
                             const std::unordered_map<std::string, PermissionLevel>& permissions)
    -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    // Check if role name already exists
    for (const auto& [role_id, role] : roles_) {
        if (role.name == name) {
            spdlog::warn("Role name {} already exists", name);
            return std::nullopt;
        }
    }
    
    std::string role_id = generateRoleId();
    Role role(role_id, name, description);
    role.permissions = permissions;
    
    roles_[role_id] = role;
    
    logSecurityEvent(SecurityEvent("system", "create_role", name, true, "Role created"));
    spdlog::info("Created role: {} ({})", name, role_id);
    
    return role_id;
}

auto CronSecurity::assignRole(const std::string& user_id, const std::string& role_id) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        spdlog::warn("User {} not found", user_id);
        return false;
    }
    
    if (roles_.find(role_id) == roles_.end()) {
        spdlog::warn("Role {} not found", role_id);
        return false;
    }
    
    auto& user_roles = user_it->second.roles;
    if (std::find(user_roles.begin(), user_roles.end(), role_id) == user_roles.end()) {
        user_roles.push_back(role_id);
        
        logSecurityEvent(SecurityEvent(user_id, "assign_role", role_id, true, "Role assigned"));
        spdlog::info("Assigned role {} to user {}", role_id, user_id);
        return true;
    }
    
    spdlog::warn("User {} already has role {}", user_id, role_id);
    return false;
}

auto CronSecurity::removeRole(const std::string& user_id, const std::string& role_id) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    auto user_it = users_.find(user_id);
    if (user_it == users_.end()) {
        spdlog::warn("User {} not found", user_id);
        return false;
    }
    
    auto& user_roles = user_it->second.roles;
    auto role_it = std::find(user_roles.begin(), user_roles.end(), role_id);
    
    if (role_it != user_roles.end()) {
        user_roles.erase(role_it);
        
        logSecurityEvent(SecurityEvent(user_id, "remove_role", role_id, true, "Role removed"));
        spdlog::info("Removed role {} from user {}", role_id, user_id);
        return true;
    }
    
    spdlog::warn("User {} does not have role {}", user_id, role_id);
    return false;
}

auto CronSecurity::authenticate(const std::string& username, const std::string& password)
    -> std::optional<SecurityContext> {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    if (!isSecurityFeatureEnabled("authentication")) {
        // If authentication is disabled, create a default context
        SecurityContext context("anonymous", generateSessionId());
        context.roles.push_back("user");
        return context;
    }
    
    auto username_it = username_to_id_.find(username);
    if (username_it == username_to_id_.end()) {
        logSecurityEvent(SecurityEvent("unknown", "authenticate", username, false, "User not found"));
        spdlog::warn("Authentication failed: user {} not found", username);
        return std::nullopt;
    }
    
    std::string user_id = username_it->second;
    auto user_it = users_.find(user_id);
    
    if (!user_it->second.is_active || user_it->second.is_locked) {
        logSecurityEvent(SecurityEvent(user_id, "authenticate", username, false, "Account inactive or locked"));
        spdlog::warn("Authentication failed: account {} is inactive or locked", username);
        return std::nullopt;
    }
    
    // For now, simplified password check (in production, use proper hashing)
    if (password != "password") {  // Placeholder password check
        user_it->second.failed_login_attempts++;
        if (user_it->second.failed_login_attempts >= 5) {
            user_it->second.is_locked = true;
            logSecurityEvent(SecurityEvent(user_id, "account_locked", username, true, "Too many failed attempts"));
        }
        
        logSecurityEvent(SecurityEvent(user_id, "authenticate", username, false, "Invalid password"));
        spdlog::warn("Authentication failed: invalid password for user {}", username);
        return std::nullopt;
    }
    
    // Reset failed attempts on successful login
    user_it->second.failed_login_attempts = 0;
    user_it->second.last_login = std::chrono::system_clock::now();
    
    // Create security context
    SecurityContext context(user_id, generateSessionId());
    context.roles = user_it->second.roles;
    
    active_sessions_[context.session_id] = context;
    
    logSecurityEvent(SecurityEvent(user_id, "authenticate", username, true, "Authentication successful"));
    spdlog::info("User {} authenticated successfully", username);
    
    return context;
}

auto CronSecurity::validateContext(const SecurityContext& context) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    // Check if session exists
    auto session_it = active_sessions_.find(context.session_id);
    if (session_it == active_sessions_.end()) {
        return false;
    }
    
    // Check if session is expired
    auto now = std::chrono::system_clock::now();
    if (now > context.expires_at) {
        active_sessions_.erase(session_it);
        return false;
    }
    
    // Check if user still exists and is active
    auto user_it = users_.find(context.user_id);
    if (user_it == users_.end() || !user_it->second.is_active || user_it->second.is_locked) {
        active_sessions_.erase(session_it);
        return false;
    }
    
    return true;
}

auto CronSecurity::checkPermission(const SecurityContext& context, const std::string& action,
                                  const std::string& resource) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);
    
    if (!isSecurityFeatureEnabled("authorization")) {
        return true; // Authorization disabled
    }
    
    if (!validateContext(context)) {
        return false;
    }
    
    auto user_it = users_.find(context.user_id);
    if (user_it == users_.end()) {
        return false;
    }
    
    // Calculate effective permissions
    auto effective_permissions = calculateEffectivePermissions(user_it->second);
    
    // Check specific resource permission
    auto resource_perm_it = effective_permissions.find(resource);
    if (resource_perm_it != effective_permissions.end()) {
        PermissionLevel required_level = PermissionLevel::READ;
        if (action == "write" || action == "create" || action == "update") {
            required_level = PermissionLevel::WRITE;
        } else if (action == "execute" || action == "run") {
            required_level = PermissionLevel::EXECUTE;
        } else if (action == "admin" || action == "delete") {
            required_level = PermissionLevel::ADMIN;
        }
        
        return resource_perm_it->second >= required_level;
    }
    
    // Check default permission
    return user_it->second.default_permission >= PermissionLevel::READ;
}

auto CronSecurity::checkCommandPermission(const SecurityContext& context, const std::string& command) -> bool {
    if (!isSecurityFeatureEnabled("command_validation")) {
        return true;
    }

    return validateCommand(command, context);
}

void CronSecurity::setResourceLimits(const std::string& identifier, const ResourceLimits& limits) {
    std::lock_guard<std::mutex> lock(security_mutex_);
    resource_limits_[identifier] = limits;
    spdlog::info("Set resource limits for {}", identifier);
}

auto CronSecurity::getResourceLimits(const std::string& identifier) -> std::optional<ResourceLimits> {
    std::lock_guard<std::mutex> lock(security_mutex_);

    auto it = resource_limits_.find(identifier);
    if (it != resource_limits_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto CronSecurity::monitorResourceUsage([[maybe_unused]] int process_id, [[maybe_unused]] const ResourceLimits& limits) -> bool {
    if (!isSecurityFeatureEnabled("resource_limits")) {
        return true;
    }

    // Simplified resource monitoring (in production, use proper system APIs)
    // This is a placeholder implementation
    spdlog::debug("Monitoring resource usage for process {}", process_id);

    // TODO: Implement actual resource monitoring using:
    // - /proc/[pid]/stat for CPU and memory usage
    // - /proc/[pid]/limits for process limits
    // - cgroups for container-based limits

    return true; // Placeholder
}

void CronSecurity::setSandboxConfig(const std::string& identifier, const SandboxConfig& config) {
    std::lock_guard<std::mutex> lock(security_mutex_);
    sandbox_configs_[identifier] = config;
    spdlog::info("Set sandbox config for {}", identifier);
}

auto CronSecurity::getSandboxConfig(const std::string& identifier) -> std::optional<SandboxConfig> {
    std::lock_guard<std::mutex> lock(security_mutex_);

    auto it = sandbox_configs_.find(identifier);
    if (it != sandbox_configs_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto CronSecurity::createSandbox([[maybe_unused]] const SandboxConfig& config) -> std::optional<std::string> {
    if (!isSecurityFeatureEnabled("sandboxing")) {
        return "no-sandbox"; // Return dummy ID when sandboxing is disabled
    }

    std::lock_guard<std::mutex> lock(security_mutex_);

    try {
        std::string sandbox_id = "sandbox_" + std::to_string(std::time(nullptr));

        // TODO: Implement actual sandbox creation using:
        // - Linux namespaces (user, mount, network, pid)
        // - chroot or pivot_root for filesystem isolation
        // - seccomp for system call filtering
        // - cgroups for resource limits

        active_sandboxes_[sandbox_id] = "created";
        spdlog::info("Created sandbox: {}", sandbox_id);

        return sandbox_id;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create sandbox: {}", e.what());
        return std::nullopt;
    }
}

auto CronSecurity::destroySandbox(const std::string& sandbox_id) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);

    auto it = active_sandboxes_.find(sandbox_id);
    if (it != active_sandboxes_.end()) {
        // TODO: Implement actual sandbox cleanup
        active_sandboxes_.erase(it);
        spdlog::info("Destroyed sandbox: {}", sandbox_id);
        return true;
    }

    spdlog::warn("Sandbox {} not found", sandbox_id);
    return false;
}

auto CronSecurity::validateCommand(const std::string& command, const SecurityContext& context) -> bool {
    if (!isSecurityFeatureEnabled("command_validation")) {
        return true;
    }

    // Basic command validation
    std::vector<std::string> dangerous_commands = {
        "rm -rf", "dd if=", ":(){ :|:& };:", "chmod 777", "chown root",
        "sudo", "su -", "passwd", "useradd", "userdel", "mount", "umount"
    };

    for (const auto& dangerous : dangerous_commands) {
        if (command.find(dangerous) != std::string::npos) {
            logSecurityEvent(SecurityEvent(context.user_id, "validate_command", command,
                                         false, "Dangerous command detected"));
            spdlog::warn("Dangerous command blocked: {}", command);
            return false;
        }
    }

    // Check for shell injection patterns
    std::regex injection_patterns(R"([;&|`$(){}[\]\\])");
    if (std::regex_search(command, injection_patterns)) {
        logSecurityEvent(SecurityEvent(context.user_id, "validate_command", command,
                                     false, "Potential shell injection"));
        spdlog::warn("Potential shell injection blocked: {}", command);
        return false;
    }

    return true;
}

auto CronSecurity::validatePath(const std::string& path, const SecurityContext& context) -> bool {
    // Basic path validation
    std::vector<std::string> dangerous_paths = {
        "/etc/passwd", "/etc/shadow", "/root", "/boot", "/sys", "/proc/sys"
    };

    for (const auto& dangerous : dangerous_paths) {
        if (path.find(dangerous) == 0) {
            logSecurityEvent(SecurityEvent(context.user_id, "validate_path", path,
                                         false, "Dangerous path access"));
            spdlog::warn("Dangerous path blocked: {}", path);
            return false;
        }
    }

    // Check for path traversal
    if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos) {
        logSecurityEvent(SecurityEvent(context.user_id, "validate_path", path,
                                     false, "Path traversal attempt"));
        spdlog::warn("Path traversal blocked: {}", path);
        return false;
    }

    return true;
}

auto CronSecurity::scanCommand(const std::string& command) -> std::vector<std::string> {
    std::vector<std::string> issues;

    // Check for various security issues
    if (command.find("rm -rf") != std::string::npos) {
        issues.push_back("Potentially destructive file deletion");
    }

    if (command.find("sudo") != std::string::npos) {
        issues.push_back("Privilege escalation attempt");
    }

    if (command.find("curl") != std::string::npos || command.find("wget") != std::string::npos) {
        issues.push_back("Network access detected");
    }

    std::regex shell_chars(R"([;&|`$(){}])");
    if (std::regex_search(command, shell_chars)) {
        issues.push_back("Shell metacharacters detected");
    }

    return issues;
}

void CronSecurity::logSecurityEvent(const SecurityEvent& event) {
    if (!isSecurityFeatureEnabled("audit_logging")) {
        return;
    }

    std::lock_guard<std::mutex> lock(security_mutex_);

    SecurityEvent logged_event = event;
    logged_event.event_id = generateEventId();

    audit_log_.push_back(logged_event);

    // Limit audit log size
    if (audit_log_.size() > MAX_AUDIT_LOG_SIZE) {
        audit_log_.erase(audit_log_.begin(),
                        audit_log_.begin() + (audit_log_.size() - MAX_AUDIT_LOG_SIZE));
    }

    // Log to spdlog as well
    if (logged_event.success) {
        spdlog::info("Security event: {} performed {} on {} successfully",
                    logged_event.user_id, logged_event.action, logged_event.resource);
    } else {
        spdlog::warn("Security event: {} failed to perform {} on {} - {}",
                    logged_event.user_id, logged_event.action, logged_event.resource, logged_event.reason);
    }
}

auto CronSecurity::getAuditLog(std::chrono::system_clock::time_point start_time,
                              std::chrono::system_clock::time_point end_time,
                              const std::string& user_id) -> std::vector<SecurityEvent> {
    std::lock_guard<std::mutex> lock(security_mutex_);

    std::vector<SecurityEvent> result;

    for (const auto& event : audit_log_) {
        if (event.timestamp >= start_time && event.timestamp <= end_time) {
            if (user_id.empty() || event.user_id == user_id) {
                result.push_back(event);
            }
        }
    }

    return result;
}

void CronSecurity::setSecurityFeature(const std::string& feature, bool enabled) {
    std::lock_guard<std::mutex> lock(security_mutex_);
    security_features_[feature] = enabled;
    spdlog::info("Security feature {} {}", feature, enabled ? "enabled" : "disabled");
}

auto CronSecurity::isSecurityFeatureEnabled(const std::string& feature) -> bool {
    std::lock_guard<std::mutex> lock(security_mutex_);

    auto it = security_features_.find(feature);
    return it != security_features_.end() ? it->second : false;
}

// Private helper methods
auto CronSecurity::generateUserId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return "user_" + std::to_string(dis(gen));
}

auto CronSecurity::generateRoleId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return "role_" + std::to_string(dis(gen));
}

auto CronSecurity::generateSessionId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    return "session_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

auto CronSecurity::generateEventId() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    return "event_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

auto CronSecurity::calculateEffectivePermissions(const UserAccount& user)
    -> std::unordered_map<std::string, PermissionLevel> {
    std::unordered_map<std::string, PermissionLevel> effective_permissions;

    // Start with user's direct permissions
    effective_permissions = user.resource_permissions;

    // Add permissions from roles
    for (const auto& role_id : user.roles) {
        auto role_it = roles_.find(role_id);
        if (role_it != roles_.end()) {
            for (const auto& [resource, permission] : role_it->second.permissions) {
                // Take the highest permission level
                auto existing_it = effective_permissions.find(resource);
                if (existing_it == effective_permissions.end() ||
                    permission > existing_it->second) {
                    effective_permissions[resource] = permission;
                }
            }
        }
    }

    return effective_permissions;
}

void CronSecurity::initializeDefaultRoles() {
    // Create default roles
    auto admin_role_id = createRole("admin", "Administrator with full access");
    if (admin_role_id.has_value()) {
        auto& admin_role = roles_[admin_role_id.value()];
        admin_role.permissions["*"] = PermissionLevel::ADMIN;
    }

    auto user_role_id = createRole("user", "Regular user with limited access");
    if (user_role_id.has_value()) {
        auto& user_role = roles_[user_role_id.value()];
        user_role.permissions["jobs"] = PermissionLevel::WRITE;
        user_role.permissions["schedules"] = PermissionLevel::READ;
    }

    auto readonly_role_id = createRole("readonly", "Read-only access");
    if (readonly_role_id.has_value()) {
        auto& readonly_role = roles_[readonly_role_id.value()];
        readonly_role.permissions["*"] = PermissionLevel::READ;
    }
}
