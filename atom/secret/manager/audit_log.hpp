#ifndef ATOM_SECRET_MANAGER_AUDIT_LOG_HPP
#define ATOM_SECRET_MANAGER_AUDIT_LOG_HPP

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Audit log action types.
 */
enum class AuditAction {
    // Session actions
    SessionUnlock,
    SessionLock,
    SessionTimeout,
    MasterPasswordChange,

    // Entry actions
    EntryCreate,
    EntryRead,
    EntryUpdate,
    EntryDelete,
    PasswordCopied,
    PasswordViewed,

    // Import/Export
    DataExport,
    DataImport,
    BackupCreate,
    BackupRestore,

    // Settings
    SettingsChange,

    // Security
    FailedUnlock,
    SuspiciousActivity
};

/**
 * @brief Audit log entry.
 */
struct AuditEntry {
    int64_t id;                                       ///< Entry ID
    std::chrono::system_clock::time_point timestamp;  ///< When it happened
    AuditAction action;                               ///< What action
    std::string entryId;     ///< Related entry ID (if any)
    std::string entryTitle;  ///< Related entry title (if any)
    std::string details;     ///< Additional details
    std::string ipAddress;   ///< IP address (if applicable)
    std::string deviceInfo;  ///< Device information

    /**
     * @brief Gets action as string.
     * @return Action name.
     */
    std::string getActionName() const;
};

/**
 * @brief Audit log query options.
 */
struct AuditQueryOptions {
    std::chrono::system_clock::time_point startTime;  ///< Start of time range
    std::chrono::system_clock::time_point endTime;    ///< End of time range
    std::vector<AuditAction> actions;                 ///< Filter by actions
    std::string entryId;                              ///< Filter by entry ID
    int limit = 100;  ///< Maximum entries to return
    int offset = 0;   ///< Offset for pagination

    static AuditQueryOptions defaults() { return AuditQueryOptions{}; }
};

/**
 * @brief Audit logging for security and compliance.
 */
class AuditLog {
public:
    /**
     * @brief Constructs an AuditLog.
     * @param logPath Path to log file.
     * @param maxEntries Maximum entries to keep (0 = unlimited).
     */
    explicit AuditLog(const std::filesystem::path& logPath,
                      size_t maxEntries = 10000);

    ~AuditLog();

    /**
     * @brief Logs an action.
     * @param action The action type.
     * @param entryId Related entry ID (optional).
     * @param entryTitle Related entry title (optional).
     * @param details Additional details (optional).
     * @return Result indicating success or error.
     */
    Result<void> log(AuditAction action, const std::string& entryId = "",
                     const std::string& entryTitle = "",
                     const std::string& details = "");

    /**
     * @brief Queries the audit log.
     * @param options Query options.
     * @return Result containing matching entries or error.
     */
    Result<std::vector<AuditEntry>> query(
        const AuditQueryOptions& options = AuditQueryOptions::defaults());

    /**
     * @brief Gets the most recent entries.
     * @param count Number of entries to get.
     * @return Result containing entries or error.
     */
    Result<std::vector<AuditEntry>> getRecent(int count = 50);

    /**
     * @brief Gets entries for a specific password entry.
     * @param entryId Entry ID.
     * @param limit Maximum entries.
     * @return Result containing entries or error.
     */
    Result<std::vector<AuditEntry>> getForEntry(const std::string& entryId,
                                                int limit = 50);

    /**
     * @brief Clears old entries.
     * @param olderThan Delete entries older than this.
     * @return Result containing number of deleted entries or error.
     */
    Result<int> clearOldEntries(
        std::chrono::system_clock::time_point olderThan);

    /**
     * @brief Exports the audit log.
     * @param outputPath Output file path.
     * @param options Query options for filtering.
     * @return Result indicating success or error.
     */
    Result<void> exportLog(
        const std::filesystem::path& outputPath,
        const AuditQueryOptions& options = AuditQueryOptions::defaults());

    /**
     * @brief Gets the total number of entries.
     * @return Entry count.
     */
    size_t getEntryCount() const;

    /**
     * @brief Enables or disables logging.
     * @param enabled Whether logging is enabled.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Checks if logging is enabled.
     * @return True if enabled.
     */
    bool isEnabled() const;

private:
    void loadFromFile();
    void saveToFile();
    void trimEntries();

    std::filesystem::path logPath_;
    size_t maxEntries_;
    bool enabled_;
    std::vector<AuditEntry> entries_;
    int64_t nextId_;
    mutable std::mutex mutex_;
};

/**
 * @brief Converts AuditAction to string.
 * @param action Action to convert.
 * @return String representation.
 */
inline std::string auditActionToString(AuditAction action) {
    switch (action) {
        case AuditAction::SessionUnlock:
            return "Session Unlock";
        case AuditAction::SessionLock:
            return "Session Lock";
        case AuditAction::SessionTimeout:
            return "Session Timeout";
        case AuditAction::MasterPasswordChange:
            return "Master Password Change";
        case AuditAction::EntryCreate:
            return "Entry Create";
        case AuditAction::EntryRead:
            return "Entry Read";
        case AuditAction::EntryUpdate:
            return "Entry Update";
        case AuditAction::EntryDelete:
            return "Entry Delete";
        case AuditAction::PasswordCopied:
            return "Password Copied";
        case AuditAction::PasswordViewed:
            return "Password Viewed";
        case AuditAction::DataExport:
            return "Data Export";
        case AuditAction::DataImport:
            return "Data Import";
        case AuditAction::BackupCreate:
            return "Backup Create";
        case AuditAction::BackupRestore:
            return "Backup Restore";
        case AuditAction::SettingsChange:
            return "Settings Change";
        case AuditAction::FailedUnlock:
            return "Failed Unlock";
        case AuditAction::SuspiciousActivity:
            return "Suspicious Activity";
        default:
            return "Unknown";
    }
}

}  // namespace atom::secret

#endif  // ATOM_SECRET_MANAGER_AUDIT_LOG_HPP
