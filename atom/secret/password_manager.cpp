#include "password_manager.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <regex>

#include "atom/type/json.hpp"
#include "encryption.hpp"
#include "storage.hpp"

namespace atom::secret {

// JSON serialization for PasswordEntry
void to_json(nlohmann::json& j, const PasswordEntry& p) {
    j = nlohmann::json{
        {"password", p.password},
        {"username", p.username},
        {"url", p.url},
        {"notes", p.notes},
        {"title", p.title},
        {"category", p.category},
        {"tags", p.tags},
        {"created", std::chrono::system_clock::to_time_t(p.created)},
        {"modified", std::chrono::system_clock::to_time_t(p.modified)},
        {"expires", std::chrono::system_clock::to_time_t(p.expires)},
        {"previousPasswords", p.previousPasswords}};
}

void from_json(const nlohmann::json& j, PasswordEntry& p) {
    j.at("password").get_to(p.password);
    j.at("username").get_to(p.username);
    j.at("url").get_to(p.url);
    j.at("notes").get_to(p.notes);
    j.at("title").get_to(p.title);
    j.at("category").get_to(p.category);
    j.at("tags").get_to(p.tags);
    p.created = std::chrono::system_clock::from_time_t(
        j.at("created").get<std::time_t>());
    p.modified = std::chrono::system_clock::from_time_t(
        j.at("modified").get<std::time_t>());
    p.expires = std::chrono::system_clock::from_time_t(
        j.at("expires").get<std::time_t>());
    j.at("previousPasswords").get_to(p.previousPasswords);
}

PasswordManager::PasswordManager(std::string_view appName,
                               const PasswordManagerSettings& settings)
    : storage_(SecureStorage::create(appName)), settings_(settings), appName_(appName) {
    spdlog::info("PasswordManager initialized for app: {}", appName);
}

Result<void> PasswordManager::addEntry(const PasswordEntry& entry,
                                       std::string_view masterPassword) {
    std::unique_lock lock(mutex_);
    if (entry.title.empty()) {
        return Result<void>::Failure("Entry title cannot be empty.");
    }

    nlohmann::json j = entry;
    std::string plaintext = j.dump();

    auto salt = Encryption::random_bytes(16);
    auto key = Encryption::derive_key(
        masterPassword,
        std::string_view(reinterpret_cast<const char*>(salt.data()),
                         salt.size()));
    auto iv = Encryption::random_bytes(12);
    auto aad = Encryption::random_bytes(16);

    auto encrypted_result = Encryption::encrypt(plaintext, key, iv, aad);
    if (encrypted_result.isError()) {
        return Result<void>::Failure("Encryption failed: " +
                                   encrypted_result.errorMessage());
    }

    auto ciphertext_with_tag = encrypted_result.value();

    // Combine salt, iv, aad, and ciphertext with tag
    std::string storable_data;
    storable_data.reserve(salt.size() + iv.size() + aad.size() +
                          ciphertext_with_tag.size());
    storable_data.append(reinterpret_cast<const char*>(salt.data()),
                         salt.size());
    storable_data.append(reinterpret_cast<const char*>(iv.data()), iv.size());
    storable_data.append(reinterpret_cast<const char*>(aad.data()), aad.size());
    storable_data.append(
        reinterpret_cast<const char*>(ciphertext_with_tag.data()),
        ciphertext_with_tag.size());

    auto store_result = storage_->store(entry.title, storable_data);
    if (store_result.isError()) {
        return Result<void>::Failure("Failed to store entry: " + store_result.errorMessage());
    }

    // Update metrics
    {
        std::unique_lock metrics_lock(mutex_);
        metrics_.totalOperations++;
        metrics_.successfulOperations++;
        metrics_.totalEntries++;
        metrics_.entriesByCategory[entry.category]++;
        metrics_.lastOperation = std::chrono::system_clock::now();
    }

    return Result<void>();
}

Result<PasswordEntry> PasswordManager::getEntry(
    std::string_view title, std::string_view masterPassword) const {
    std::shared_lock lock(mutex_);
    return getEntry_nolock(title, masterPassword);
}

Result<PasswordEntry> PasswordManager::getEntry_nolock(
    std::string_view title, std::string_view masterPassword) const {
    auto retrieve_result = storage_->retrieve(title);
    if (retrieve_result.isError()) {
        return Result<PasswordEntry>::Failure("Entry not found: " + retrieve_result.errorMessage());
    }

    const std::string& storable_data = retrieve_result.value();
    if (storable_data.empty()) {
        return Result<PasswordEntry>::Failure("Entry not found.");
    }

    if (storable_data.length() < 44) {  // 16 salt + 12 iv + 16 aad
        return Result<PasswordEntry>::Failure("Invalid stored data: too short.");
    }

    std::string_view data_view(storable_data);
    auto salt_sv = data_view.substr(0, 16);
    auto iv_sv = data_view.substr(16, 12);
    auto aad_sv = data_view.substr(28, 16);

    std::vector<unsigned char> salt(salt_sv.begin(), salt_sv.end());
    std::vector<unsigned char> iv(iv_sv.begin(), iv_sv.end());
    std::vector<unsigned char> aad(aad_sv.begin(), aad_sv.end());
    std::vector<unsigned char> ciphertext_with_tag(data_view.begin() + 44,
                                                   data_view.end());

    auto key = Encryption::derive_key(
        masterPassword,
        std::string_view(reinterpret_cast<const char*>(salt.data()),
                         salt.size()));

    auto decrypted_result =
        Encryption::decrypt(ciphertext_with_tag, key, iv, aad);

    if (decrypted_result.isError()) {
        return Result<PasswordEntry>::Failure("Decryption failed: " +
                                            decrypted_result.errorMessage());
    }

    try {
        nlohmann::json j = nlohmann::json::parse(decrypted_result.value());
        PasswordEntry entry = j.get<PasswordEntry>();
        return Result<PasswordEntry>(std::move(entry));
    } catch (const nlohmann::json::exception& e) {
        return Result<PasswordEntry>::Failure("JSON parsing failed: " +
                                            std::string(e.what()));
    }
}

Result<void> PasswordManager::updateEntry(const PasswordEntry& entry,
                                          std::string_view masterPassword) {
    // This will overwrite the existing entry, which is the desired behavior for
    // an update.
    return addEntry(entry, masterPassword);
}

Result<void> PasswordManager::removeEntry(std::string_view title) {
    std::unique_lock lock(mutex_);
    auto remove_result = storage_->remove(title);
    if (remove_result.isError()) {
        return Result<void>::Failure("Failed to remove entry: " + remove_result.errorMessage());
    }

    // Update metrics
    {
        std::unique_lock metrics_lock(mutex_);
        metrics_.totalOperations++;
        metrics_.successfulOperations++;
        metrics_.lastOperation = std::chrono::system_clock::now();
    }

    return Result<void>();
}

Result<std::vector<PasswordEntry>> PasswordManager::getAllEntries(
    std::string_view masterPassword) const {
    std::shared_lock lock(mutex_);
    auto keys_result = storage_->getAllKeys();
    if (keys_result.isError()) {
        return Result<std::vector<PasswordEntry>>::Failure("Failed to get keys: " + keys_result.errorMessage());
    }

    const auto& keys = keys_result.value();
    std::vector<PasswordEntry> entries;
    entries.reserve(keys.size());

    for (const auto& key : keys) {
        // Call the non-locking version to avoid recursive lock issues.
        auto entry_result = getEntry_nolock(key, masterPassword);
        if (entry_result.isSuccess()) {
            entries.push_back(std::move(entry_result.value()));
        } else {
            spdlog::warn("Failed to decrypt entry with key '{}': {}", key,
                         entry_result.errorMessage());
        }
    }
    return Result(std::move(entries));
}

// Enhanced search and filtering methods
Result<std::vector<PasswordEntry>> PasswordManager::searchEntries(
    const SearchOptions& options,
    std::string_view masterPassword) const {

    auto start = std::chrono::steady_clock::now();

    auto all_entries_result = getAllEntries(masterPassword);
    if (all_entries_result.isError()) {
        return all_entries_result;
    }

    const auto& all_entries = all_entries_result.value();
    std::vector<PasswordEntry> matching_entries;

    for (const auto& entry : all_entries) {
        if (matchesSearchCriteria(entry, options)) {
            matching_entries.push_back(entry);
        }
    }

    // Update metrics
    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    {
        std::unique_lock lock(mutex_);
        metrics_.searchOperations++;
        metrics_.totalOperations++;
        metrics_.successfulOperations++;
        metrics_.totalLatency += latency;
        metrics_.lastOperation = std::chrono::system_clock::now();
    }

    return Result(std::move(matching_entries));
}

Result<std::vector<PasswordEntry>> PasswordManager::getEntriesByCategory(
    PasswordCategory category,
    std::string_view masterPassword) const {

    SearchOptions options;
    options.category = category;
    return searchEntries(options, masterPassword);
}

Result<std::vector<PasswordEntry>> PasswordManager::getEntriesByTags(
    const std::vector<std::string>& tags,
    bool matchAll,
    std::string_view masterPassword) const {

    auto all_entries_result = getAllEntries(masterPassword);
    if (all_entries_result.isError()) {
        return all_entries_result;
    }

    const auto& all_entries = all_entries_result.value();
    std::vector<PasswordEntry> matching_entries;

    for (const auto& entry : all_entries) {
        bool matches = false;

        if (matchAll) {
            // Entry must have all specified tags
            matches = std::all_of(tags.begin(), tags.end(),
                [&entry](const std::string& tag) {
                    return std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end();
                });
        } else {
            // Entry must have at least one of the specified tags
            matches = std::any_of(tags.begin(), tags.end(),
                [&entry](const std::string& tag) {
                    return std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end();
                });
        }

        if (matches) {
            matching_entries.push_back(entry);
        }
    }

    return Result(std::move(matching_entries));
}

Result<std::vector<PasswordEntry>> PasswordManager::getExpiringEntries(
    int daysAhead,
    std::string_view masterPassword) const {

    auto all_entries_result = getAllEntries(masterPassword);
    if (all_entries_result.isError()) {
        return all_entries_result;
    }

    const auto& all_entries = all_entries_result.value();
    std::vector<PasswordEntry> expiring_entries;

    auto now = std::chrono::system_clock::now();
    auto threshold = now + std::chrono::hours(24 * daysAhead);

    for (const auto& entry : all_entries) {
        if (entry.expires <= threshold && entry.expires > now) {
            expiring_entries.push_back(entry);
        }
    }

    return Result(std::move(expiring_entries));
}

// Bulk operations
Result<std::vector<Result<void>>> PasswordManager::bulkOperation(
    const std::vector<BulkPasswordOperation>& operations,
    std::string_view masterPassword) {

    std::vector<Result<void>> results;
    results.reserve(operations.size());

    for (const auto& op : operations) {
        switch (op.operation) {
            case BulkPasswordOperation::Type::Add:
                results.push_back(addEntry(op.entry, masterPassword));
                break;
            case BulkPasswordOperation::Type::Update:
                results.push_back(updateEntry(op.entry, masterPassword));
                break;
            case BulkPasswordOperation::Type::Remove:
                results.push_back(removeEntry(op.title));
                break;
        }
    }

    return Result(std::move(results));
}

Result<std::vector<Result<void>>> PasswordManager::bulkAddEntries(
    const std::vector<PasswordEntry>& entries,
    std::string_view masterPassword) {

    std::vector<Result<void>> results;
    results.reserve(entries.size());

    for (const auto& entry : entries) {
        results.push_back(addEntry(entry, masterPassword));
    }

    return Result(std::move(results));
}

Result<std::vector<Result<void>>> PasswordManager::bulkRemoveEntries(
    const std::vector<std::string>& titles) {

    std::vector<Result<void>> results;
    results.reserve(titles.size());

    for (const auto& title : titles) {
        results.push_back(removeEntry(title));
    }

    return Result(std::move(results));
}

// Password utilities
Result<std::string> PasswordManager::generatePassword(
    const PasswordGenerationOptions& options) const {

    try {
        std::string password = utils::generatePassword(options);
        if (password.empty()) {
            return Result<std::string>::Failure(ErrorCode::InvalidArgument,
                std::string("Failed to generate password with given options"));
        }
        return Result<std::string>(std::move(password));
    } catch (const std::exception& e) {
        return Result<std::string>::Failure(ErrorCode::InvalidState,
            std::string("Password generation failed: ") + e.what());
    }
}

PasswordStrength PasswordManager::analyzePasswordStrength(std::string_view password) const {
    return utils::analyzePasswordStrength(password);
}

Result<void> PasswordManager::validatePassword(std::string_view password) const {
    std::shared_lock lock(mutex_);

    if (!utils::validatePassword(password, settings_)) {
        return Result<void>::Failure(ErrorCode::InvalidArgument,
            std::string("Password does not meet security requirements"));
    }

    return Result<void>();
}

Result<std::unordered_map<std::string, std::vector<std::string>>>
PasswordManager::findDuplicatePasswords(std::string_view masterPassword) const {

    auto all_entries_result = getAllEntries(masterPassword);
    if (all_entries_result.isError()) {
        return Result<std::unordered_map<std::string, std::vector<std::string>>>::Failure(
            all_entries_result.errorCode(), all_entries_result.errorMessage());
    }

    const auto& all_entries = all_entries_result.value();
    std::unordered_map<std::string, std::vector<std::string>> password_map;

    for (const auto& entry : all_entries) {
        password_map[entry.password].push_back(entry.title);
    }

    // Remove entries with only one occurrence
    auto it = password_map.begin();
    while (it != password_map.end()) {
        if (it->second.size() <= 1) {
            it = password_map.erase(it);
        } else {
            ++it;
        }
    }

    return Result(std::move(password_map));
}

Result<std::vector<PasswordEntry>> PasswordManager::findWeakPasswords(
    std::string_view masterPassword) const {

    auto all_entries_result = getAllEntries(masterPassword);
    if (all_entries_result.isError()) {
        return all_entries_result;
    }

    const auto& all_entries = all_entries_result.value();
    std::vector<PasswordEntry> weak_entries;

    for (const auto& entry : all_entries) {
        PasswordStrength strength = analyzePasswordStrength(entry.password);
        if (strength == PasswordStrength::VeryWeak || strength == PasswordStrength::Weak) {
            weak_entries.push_back(entry);
        }
    }

    return Result(std::move(weak_entries));
}

// Settings and metrics methods
void PasswordManager::updateSettings(const PasswordManagerSettings& newSettings) {
    std::unique_lock lock(mutex_);
    settings_ = newSettings;
    spdlog::info("Password manager settings updated");
}

PasswordManagerSettings PasswordManager::getSettings() const {
    std::shared_lock lock(mutex_);
    return settings_;
}

PasswordManagerMetrics PasswordManager::getMetrics() const {
    std::shared_lock lock(mutex_);
    return metrics_;
}

void PasswordManager::clearCache() {
    storage_->clearCache();
    std::unique_lock lock(mutex_);
    metrics_ = PasswordManagerMetrics{};
    spdlog::info("Password manager cache and metrics cleared");
}

// Helper methods
bool PasswordManager::matchesSearchCriteria(const PasswordEntry& entry, const SearchOptions& options) const {
    // Check category filter
    if (options.category != PasswordCategory::General && entry.category != options.category) {
        return false;
    }

    // Check date filters
    if (options.createdAfter != std::chrono::system_clock::time_point{} &&
        entry.created < options.createdAfter) {
        return false;
    }

    if (options.createdBefore != std::chrono::system_clock::time_point{} &&
        entry.created > options.createdBefore) {
        return false;
    }

    if (options.expiresAfter != std::chrono::system_clock::time_point{} &&
        entry.expires < options.expiresAfter) {
        return false;
    }

    if (options.expiresBefore != std::chrono::system_clock::time_point{} &&
        entry.expires > options.expiresBefore) {
        return false;
    }

    // Check expiration filter
    if (!options.includeExpired) {
        auto now = std::chrono::system_clock::now();
        if (entry.expires <= now) {
            return false;
        }
    }

    // Check text search
    if (!options.query.empty()) {
        std::string query = options.query;
        if (!options.caseSensitive) {
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        }

        auto checkField = [&](const std::string& field) -> bool {
            std::string fieldCopy = field;
            if (!options.caseSensitive) {
                std::transform(fieldCopy.begin(), fieldCopy.end(), fieldCopy.begin(), ::tolower);
            }

            if (options.useRegex) {
                try {
                    std::regex pattern(query, options.caseSensitive ? std::regex::ECMAScript : std::regex::icase);
                    return std::regex_search(fieldCopy, pattern);
                } catch (const std::regex_error&) {
                    return false;
                }
            } else {
                return fieldCopy.find(query) != std::string::npos;
            }
        };

        bool matches = false;
        if (options.searchInTitle && checkField(entry.title)) matches = true;
        if (options.searchInUsername && checkField(entry.username)) matches = true;
        if (options.searchInUrl && checkField(entry.url)) matches = true;
        if (options.searchInNotes && checkField(entry.notes)) matches = true;

        if (options.searchInTags) {
            for (const auto& tag : entry.tags) {
                if (checkField(tag)) {
                    matches = true;
                    break;
                }
            }
        }

        if (!matches) {
            return false;
        }
    }

    return true;
}

void PasswordManager::updateMetrics(bool /*success*/, std::chrono::milliseconds /*latency*/) {
    // This method is now inlined where needed to handle const correctness
}

}  // namespace atom::secret
