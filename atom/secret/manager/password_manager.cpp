#include "password_manager.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

#include "../serialization/json.hpp"

namespace atom::secret {

struct PasswordManager::Impl {
    std::unique_ptr<SecureStorage> storage;
    PasswordManagerSettings settings;
    SessionManager session;
    std::unique_ptr<AuditLog> auditLog;

    std::unordered_map<std::string, PasswordEntry> entries;
    std::vector<uint8_t> encryptedMasterKey;
    bool initialized = false;

    explicit Impl(std::unique_ptr<SecureStorage> stor,
                  const PasswordManagerSettings& set)
        : storage(std::move(stor)), settings(set), session(set.session) {
        if (settings.enableAuditLog && storage) {
            auto logPath =
                std::filesystem::path(storage->getLocation()) / "audit.log";
            auditLog = std::make_unique<AuditLog>(logPath);
        }
    }
};

PasswordManager::PasswordManager(std::unique_ptr<SecureStorage> storage,
                                 const PasswordManagerSettings& settings)
    : impl_(std::make_unique<Impl>(std::move(storage), settings)) {}

PasswordManager::~PasswordManager() { lock(); }

Result<void> PasswordManager::initialize(std::string_view masterPassword) {
    if (masterPassword.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "Master password cannot be empty");
    }

    if (masterPassword.length() < 8) {
        return Result<void>::error(
            ErrorCode::PasswordTooShort,
            "Master password must be at least 8 characters");
    }

    impl_->entries.clear();
    impl_->initialized = true;
    impl_->session.unlock();

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::SessionUnlock, "", "",
                             "Database initialized");
    }

    return Result<void>::success();
}

Result<void> PasswordManager::unlock(std::string_view masterPassword) {
    if (masterPassword.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "Master password cannot be empty");
    }

    // In a real implementation, we would:
    // 1. Load encrypted data from storage
    // 2. Derive key from master password
    // 3. Decrypt and verify data

    impl_->session.unlock();

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::SessionUnlock);
    }

    return Result<void>::success();
}

void PasswordManager::lock() {
    impl_->session.lock();

    // Clear sensitive data from memory
    impl_->entries.clear();

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::SessionLock);
    }
}

bool PasswordManager::isUnlocked() const { return impl_->session.isUnlocked(); }

Result<void> PasswordManager::changeMasterPassword(
    std::string_view currentPassword, std::string_view newPassword) {
    if (!isUnlocked()) {
        return Result<void>::error(ErrorCode::ManagerLocked,
                                   "Manager is locked");
    }

    if (newPassword.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "New password cannot be empty");
    }

    if (newPassword.length() < 8) {
        return Result<void>::error(
            ErrorCode::PasswordTooShort,
            "New password must be at least 8 characters");
    }

    // In a real implementation, we would re-encrypt all data with the new key

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::MasterPasswordChange);
    }

    return Result<void>::success();
}

Result<std::string> PasswordManager::addEntry(const PasswordEntry& entry) {
    if (!isUnlocked()) {
        return Result<std::string>::error(ErrorCode::ManagerLocked,
                                          "Manager is locked");
    }

    PasswordEntry newEntry = entry;

    // Generate ID if not set
    if (newEntry.id.empty()) {
        newEntry.generateId();
    }

    // Set timestamps
    if (newEntry.created == std::chrono::system_clock::time_point{}) {
        newEntry.created = std::chrono::system_clock::now();
    }
    newEntry.modified = std::chrono::system_clock::now();

    // Check for duplicate ID
    if (impl_->entries.find(newEntry.id) != impl_->entries.end()) {
        return Result<std::string>::error(ErrorCode::EntryExists,
                                          "Entry with this ID already exists");
    }

    impl_->entries[newEntry.id] = newEntry;

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::EntryCreate, newEntry.id,
                             newEntry.title);
    }

    impl_->session.recordActivity();
    return Result<std::string>::success(newEntry.id);
}

Result<void> PasswordManager::updateEntry(const PasswordEntry& entry) {
    if (!isUnlocked()) {
        return Result<void>::error(ErrorCode::ManagerLocked,
                                   "Manager is locked");
    }

    if (entry.id.empty()) {
        return Result<void>::error(ErrorCode::InvalidArgument,
                                   "Entry ID cannot be empty");
    }

    auto it = impl_->entries.find(entry.id);
    if (it == impl_->entries.end()) {
        return Result<void>::error(ErrorCode::EntryNotFound, "Entry not found");
    }

    PasswordEntry updatedEntry = entry;
    updatedEntry.modified = std::chrono::system_clock::now();

    impl_->entries[entry.id] = updatedEntry;

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::EntryUpdate, entry.id, entry.title);
    }

    impl_->session.recordActivity();
    return Result<void>::success();
}

Result<void> PasswordManager::deleteEntry(const std::string& entryId) {
    if (!isUnlocked()) {
        return Result<void>::error(ErrorCode::ManagerLocked,
                                   "Manager is locked");
    }

    auto it = impl_->entries.find(entryId);
    if (it == impl_->entries.end()) {
        return Result<void>::error(ErrorCode::EntryNotFound, "Entry not found");
    }

    std::string title = it->second.title;
    impl_->entries.erase(it);

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::EntryDelete, entryId, title);
    }

    impl_->session.recordActivity();
    return Result<void>::success();
}

Result<PasswordEntry> PasswordManager::getEntry(const std::string& entryId) {
    if (!isUnlocked()) {
        return Result<PasswordEntry>::error(ErrorCode::ManagerLocked,
                                            "Manager is locked");
    }

    auto it = impl_->entries.find(entryId);
    if (it == impl_->entries.end()) {
        return Result<PasswordEntry>::error(ErrorCode::EntryNotFound,
                                            "Entry not found");
    }

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::EntryRead, entryId, it->second.title);
    }

    impl_->session.recordActivity();
    return Result<PasswordEntry>::success(it->second);
}

Result<std::vector<PasswordEntry>> PasswordManager::getAllEntries() {
    if (!isUnlocked()) {
        return Result<std::vector<PasswordEntry>>::error(
            ErrorCode::ManagerLocked, "Manager is locked");
    }

    std::vector<PasswordEntry> result;
    result.reserve(impl_->entries.size());

    for (const auto& [id, entry] : impl_->entries) {
        result.push_back(entry);
    }

    impl_->session.recordActivity();
    return Result<std::vector<PasswordEntry>>::success(std::move(result));
}

Result<std::vector<PasswordEntry>> PasswordManager::search(
    const SearchFilter& filter) {
    if (!isUnlocked()) {
        return Result<std::vector<PasswordEntry>>::error(
            ErrorCode::ManagerLocked, "Manager is locked");
    }

    std::vector<PasswordEntry> results;

    auto matchesQuery = [&filter](const PasswordEntry& entry) {
        if (filter.query.empty())
            return true;

        std::string lowerQuery;
        for (char c : filter.query) {
            lowerQuery +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        auto containsIgnoreCase = [&lowerQuery](const std::string& str) {
            std::string lower;
            for (char c : str) {
                lower += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
            return lower.find(lowerQuery) != std::string::npos;
        };

        return containsIgnoreCase(entry.title) ||
               containsIgnoreCase(entry.username) ||
               containsIgnoreCase(entry.url) || containsIgnoreCase(entry.notes);
    };

    for (const auto& [id, entry] : impl_->entries) {
        // Apply filters
        if (filter.excludeArchived && entry.isArchived)
            continue;
        if (filter.favoritesOnly && !entry.isFavorite)
            continue;
        if (filter.expiredOnly && !entry.isExpired())
            continue;
        if (filter.expiringSoonDays > 0 &&
            !entry.isExpiringSoon(filter.expiringSoonDays))
            continue;

        // Category filter
        if (!filter.categories.empty()) {
            bool categoryMatch = false;
            for (const auto& cat : filter.categories) {
                if (entry.category == cat) {
                    categoryMatch = true;
                    break;
                }
            }
            if (!categoryMatch)
                continue;
        }

        // Tag filter
        if (!filter.tags.empty()) {
            bool tagMatch = false;
            for (const auto& tag : filter.tags) {
                if (entry.hasTag(tag)) {
                    tagMatch = true;
                    break;
                }
            }
            if (!tagMatch)
                continue;
        }

        // Query filter
        if (!matchesQuery(entry))
            continue;

        results.push_back(entry);
    }

    impl_->session.recordActivity();
    return Result<std::vector<PasswordEntry>>::success(std::move(results));
}

size_t PasswordManager::getEntryCount() const { return impl_->entries.size(); }

Result<std::string> PasswordManager::getPassword(const std::string& entryId) {
    if (!isUnlocked()) {
        return Result<std::string>::error(ErrorCode::ManagerLocked,
                                          "Manager is locked");
    }

    auto it = impl_->entries.find(entryId);
    if (it == impl_->entries.end()) {
        return Result<std::string>::error(ErrorCode::EntryNotFound,
                                          "Entry not found");
    }

    // Mark as accessed
    it->second.markAccessed();

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::PasswordCopied, entryId,
                             it->second.title);
    }

    impl_->session.recordActivity();
    return Result<std::string>::success(it->second.password);
}

Result<void> PasswordManager::updatePassword(const std::string& entryId,
                                             const std::string& newPassword) {
    if (!isUnlocked()) {
        return Result<void>::error(ErrorCode::ManagerLocked,
                                   "Manager is locked");
    }

    auto it = impl_->entries.find(entryId);
    if (it == impl_->entries.end()) {
        return Result<void>::error(ErrorCode::EntryNotFound, "Entry not found");
    }

    it->second.updatePassword(newPassword, impl_->settings.maxPasswordHistory);

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::EntryUpdate, entryId,
                             it->second.title, "Password updated");
    }

    impl_->session.recordActivity();
    return Result<void>::success();
}

Result<std::string> PasswordManager::exportToJson(const std::string& password) {
    if (!isUnlocked()) {
        return Result<std::string>::error(ErrorCode::ManagerLocked,
                                          "Manager is locked");
    }

    std::vector<PasswordEntry> entries;
    for (const auto& [id, entry] : impl_->entries) {
        entries.push_back(entry);
    }

    auto jsonResult = JsonSerializer::serializeEntries(entries, true);
    if (jsonResult.isError()) {
        return jsonResult;
    }

    if (impl_->auditLog) {
        impl_->auditLog->log(
            AuditAction::DataExport, "", "",
            "Exported " + std::to_string(entries.size()) + " entries");
    }

    // If password provided, encrypt the JSON
    if (!password.empty()) {
        auto encResult = Encryption::encrypt(jsonResult.value(), password);
        if (encResult.isError()) {
            return Result<std::string>::error(ErrorCode::EncryptionFailed,
                                              encResult.errorMessage());
        }

        auto serialized = encResult.value().serialize();
        return Result<std::string>::success(
            std::string(serialized.begin(), serialized.end()));
    }

    return jsonResult;
}

Result<int> PasswordManager::importFromJson(const std::string& json,
                                            const std::string& password,
                                            bool overwrite) {
    if (!isUnlocked()) {
        return Result<int>::error(ErrorCode::ManagerLocked,
                                  "Manager is locked");
    }

    std::string jsonData = json;

    // If password provided, decrypt first
    if (!password.empty()) {
        std::vector<uint8_t> data(json.begin(), json.end());
        auto encDataResult = EncryptedData::deserialize(data);
        if (encDataResult.isError()) {
            return Result<int>::error(ErrorCode::InvalidFormat,
                                      "Invalid encrypted data format");
        }

        auto decResult = Encryption::decrypt(encDataResult.value(), password);
        if (decResult.isError()) {
            return Result<int>::error(ErrorCode::DecryptionFailed,
                                      decResult.errorMessage());
        }
        jsonData = decResult.value();
    }

    auto entriesResult = JsonSerializer::deserializeEntries(jsonData);
    if (entriesResult.isError()) {
        return Result<int>::error(entriesResult.errorCode(),
                                  entriesResult.errorMessage());
    }

    int imported = 0;
    for (auto& entry : entriesResult.value()) {
        if (entry.id.empty()) {
            entry.generateId();
        }

        auto it = impl_->entries.find(entry.id);
        if (it != impl_->entries.end()) {
            if (overwrite) {
                impl_->entries[entry.id] = entry;
                ++imported;
            }
        } else {
            impl_->entries[entry.id] = entry;
            ++imported;
        }
    }

    if (impl_->auditLog) {
        impl_->auditLog->log(
            AuditAction::DataImport, "", "",
            "Imported " + std::to_string(imported) + " entries");
    }

    impl_->session.recordActivity();
    return Result<int>::success(imported);
}

const PasswordManagerSettings& PasswordManager::getSettings() const {
    return impl_->settings;
}

void PasswordManager::updateSettings(const PasswordManagerSettings& settings) {
    impl_->settings = settings;
    impl_->session.updateConfig(settings.session);

    if (impl_->auditLog) {
        impl_->auditLog->log(AuditAction::SettingsChange);
    }
}

SessionManager& PasswordManager::getSession() { return impl_->session; }

AuditLog* PasswordManager::getAuditLog() { return impl_->auditLog.get(); }

}  // namespace atom::secret
