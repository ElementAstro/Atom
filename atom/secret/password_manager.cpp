#include "password_manager.hpp"

#include <algorithm>
#include <sstream>

#include "encryption.hpp"

namespace atom::secret {

// ============================================================================
// PasswordManager Implementation
// ============================================================================

PasswordManager::PasswordManager()
    : storage_(SecureStorage::create("atom-password-manager")),
      isLocked_(true),
      lastActivity_(std::chrono::system_clock::now()) {
}

PasswordManager::~PasswordManager() {
    lock();
}

PasswordManager::PasswordManager(PasswordManager&& other) noexcept
    : storage_(std::move(other.storage_)),
      settings_(std::move(other.settings_)),
      masterKey_(std::move(other.masterKey_)),
      masterSalt_(std::move(other.masterSalt_)),
      isLocked_(other.isLocked_),
      lastActivity_(other.lastActivity_) {
    
    other.isLocked_ = true;
    other.lastActivity_ = std::chrono::system_clock::now();
}

PasswordManager& PasswordManager::operator=(PasswordManager&& other) noexcept {
    if (this != &other) {
        lock(); // Clear current state
        
        storage_ = std::move(other.storage_);
        settings_ = std::move(other.settings_);
        masterKey_ = std::move(other.masterKey_);
        masterSalt_ = std::move(other.masterSalt_);
        isLocked_ = other.isLocked_;
        lastActivity_ = other.lastActivity_;
        
        other.isLocked_ = true;
        other.lastActivity_ = std::chrono::system_clock::now();
    }
    return *this;
}

bool PasswordManager::initialize(std::string_view masterPassword,
                                const PasswordManagerSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (masterPassword.empty()) {
        return false;
    }
    
    settings_ = settings;
    
    // Generate a new salt for the master key
    masterSalt_.resize(32);
    if (RAND_bytes(masterSalt_.data(), static_cast<int>(masterSalt_.size())) != 1) {
        return false;
    }
    
    // Derive the master key
    auto keyResult = deriveMasterKey(masterPassword);
    if (keyResult.isError()) {
        return false;
    }
    
    masterKey_ = std::move(keyResult.value());
    isLocked_ = false;
    updateLastActivity();
    
    // Store the salt in secure storage for future use
    std::string saltData(masterSalt_.begin(), masterSalt_.end());
    if (!storage_->store("__master_salt__", saltData)) {
        PasswordManager::lock();
        return false;
    }
    
    return true;
}

void PasswordManager::lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Securely clear sensitive data
    SecureMemory::secureClear(masterKey_);
    SecureMemory::secureClear(masterSalt_);
    
    isLocked_ = true;
}

bool PasswordManager::unlock(std::string_view masterPassword) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (masterPassword.empty()) {
        return false;
    }
    
    // Retrieve the master salt
    std::string saltData = storage_->retrieve("__master_salt__");
    if (saltData.empty()) {
        return false;
    }
    masterSalt_.assign(saltData.begin(), saltData.end());
    
    // Derive the master key
    auto keyResult = deriveMasterKey(masterPassword);
    if (keyResult.isError()) {
        return false;
    }
    
    masterKey_ = std::move(keyResult.value());
    isLocked_ = false;
    updateLastActivity();
    
    return true;
}

bool PasswordManager::isLocked() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return isLocked_;
}

bool PasswordManager::changeMasterPassword(std::string_view currentPassword,
                                          std::string_view newPassword) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!ensureUnlocked() || currentPassword.empty() || newPassword.empty()) {
        return false;
    }
    
    // Verify current password by deriving key
    auto currentKeyResult = deriveMasterKey(currentPassword);
    if (currentKeyResult.isError()) {
        return false;
    }
    
    // Check if current key matches stored key
    if (!SecureComparison::constantTimeEquals(
            currentKeyResult.value().data(), masterKey_.data(), masterKey_.size())) {
        return false;
    }
    
    // Get all stored entries to re-encrypt with new key
    auto allKeys = getAllKeys();
    std::vector<std::pair<std::string, PasswordEntry>> allEntries;
    
    for (const auto& key : allKeys) {
        if (key == "__master_salt__") continue;
        auto entry = retrievePassword(key);
        if (!entry.password.empty()) {
            allEntries.emplace_back(key, std::move(entry));
        }
    }
    
    // Generate new salt and derive new key
    if (RAND_bytes(masterSalt_.data(), static_cast<int>(masterSalt_.size())) != 1) {
        return false;
    }
    
    auto newKeyResult = deriveMasterKey(newPassword);
    if (newKeyResult.isError()) {
        return false;
    }
    
    // Clear old key and set new key
    SecureMemory::secureClear(masterKey_);
    masterKey_ = std::move(newKeyResult.value());
    
    // Store new salt
    std::string saltData(masterSalt_.begin(), masterSalt_.end());
    if (!storage_->store("__master_salt__", saltData)) {
        return false;
    }
    
    // Re-encrypt and store all entries with new key
    for (const auto& [key, entry] : allEntries) {
        if (!storePassword(key, entry)) {
            // If re-encryption fails, we're in a bad state
            return false;
        }
    }
    
    updateLastActivity();
    return true;
}

bool PasswordManager::storePassword(std::string_view key, const PasswordEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!ensureUnlocked() || key.empty()) {
        return false;
    }
    
    // Serialize the entry to JSON
    auto jsonResult = JsonSerializer::serializeEntry(entry);
    if (jsonResult.isError()) {
        return false;
    }
    
    // Encrypt the JSON data
    auto encryptedResult = encryptData(jsonResult.value());
    if (encryptedResult.isError()) {
        return false;
    }
    
    // Serialize the encrypted data for storage
    std::ostringstream oss;
    const auto& encrypted = encryptedResult.value();
    
    // Store as binary data: method(1) + salt_len(4) + salt + iv_len(4) + iv + tag_len(4) + tag + data
    oss.write(reinterpret_cast<const char*>(&encrypted.method), sizeof(encrypted.method));
    
    uint32_t saltLen = static_cast<uint32_t>(encrypted.salt.size());
    oss.write(reinterpret_cast<const char*>(&saltLen), sizeof(saltLen));
    oss.write(reinterpret_cast<const char*>(encrypted.salt.data()), saltLen);
    
    uint32_t ivLen = static_cast<uint32_t>(encrypted.iv.size());
    oss.write(reinterpret_cast<const char*>(&ivLen), sizeof(ivLen));
    oss.write(reinterpret_cast<const char*>(encrypted.iv.data()), ivLen);
    
    uint32_t tagLen = static_cast<uint32_t>(encrypted.tag.size());
    oss.write(reinterpret_cast<const char*>(&tagLen), sizeof(tagLen));
    oss.write(reinterpret_cast<const char*>(encrypted.tag.data()), tagLen);
    
    oss.write(reinterpret_cast<const char*>(encrypted.ciphertext.data()), encrypted.ciphertext.size());
    
    updateLastActivity();
    return storage_->store(std::string(key), oss.str());
}

PasswordEntry PasswordManager::retrievePassword(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!ensureUnlocked() || key.empty()) {
        return PasswordEntry{};
    }
    
    // Retrieve encrypted data from storage
    std::string data = storage_->retrieve(std::string(key));
    if (data.empty()) {
        return PasswordEntry{};
    }
    std::istringstream iss(data);
    
    // Deserialize encrypted data
    EncryptedData encrypted;
    
    iss.read(reinterpret_cast<char*>(&encrypted.method), sizeof(encrypted.method));
    
    uint32_t saltLen, ivLen, tagLen;
    iss.read(reinterpret_cast<char*>(&saltLen), sizeof(saltLen));
    encrypted.salt.resize(saltLen);
    iss.read(reinterpret_cast<char*>(encrypted.salt.data()), saltLen);
    
    iss.read(reinterpret_cast<char*>(&ivLen), sizeof(ivLen));
    encrypted.iv.resize(ivLen);
    iss.read(reinterpret_cast<char*>(encrypted.iv.data()), ivLen);
    
    iss.read(reinterpret_cast<char*>(&tagLen), sizeof(tagLen));
    encrypted.tag.resize(tagLen);
    iss.read(reinterpret_cast<char*>(encrypted.tag.data()), tagLen);
    
    size_t dataLen = data.size() - iss.tellg();
    encrypted.ciphertext.resize(dataLen);
    iss.read(reinterpret_cast<char*>(encrypted.ciphertext.data()), dataLen);
    
    // Decrypt the data
    auto decryptedResult = decryptData(encrypted);
    if (decryptedResult.isError()) {
        return PasswordEntry{};
    }
    
    // Deserialize the JSON to PasswordEntry
    auto entryResult = JsonSerializer::deserializeEntry(decryptedResult.value());
    if (entryResult.isError()) {
        return PasswordEntry{};
    }
    
    updateLastActivity();
    return entryResult.value();
}

bool PasswordManager::removePassword(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked() || key.empty()) {
        return false;
    }

    updateLastActivity();
    return storage_->remove(std::string(key));
}

std::vector<std::string> PasswordManager::getAllKeys() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return {};
    }

    auto allKeys = storage_->getAllKeys();

    // Filter out internal keys
    allKeys.erase(
        std::remove_if(allKeys.begin(), allKeys.end(),
                      [](const std::string& key) {
                          return key.starts_with("__") && key.ends_with("__");
                      }),
        allKeys.end());

    updateLastActivity();
    return allKeys;
}

std::vector<std::pair<std::string, PasswordEntry>> PasswordManager::searchPasswords(
    std::string_view query,
    bool searchInTitle,
    bool searchInUsername,
    bool searchInUrl,
    bool searchInNotes,
    bool searchInTags) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked() || query.empty()) {
        return {};
    }

    std::vector<std::pair<std::string, PasswordEntry>> results;
    auto allKeys = getAllKeys();

    std::string lowerQuery;
    lowerQuery.reserve(query.length());
    for (char c : query) {
        lowerQuery += std::tolower(c);
    }

    for (const auto& key : allKeys) {
        auto entry = retrievePassword(key);
        if (entry.password.empty()) continue;

        bool matches = false;

        // Helper lambda to check if text contains query (case-insensitive)
        auto contains = [&lowerQuery](const std::string& text) {
            std::string lowerText;
            lowerText.reserve(text.length());
            for (char c : text) {
                lowerText += std::tolower(c);
            }
            return lowerText.find(lowerQuery) != std::string::npos;
        };

        if (searchInTitle && contains(entry.title)) matches = true;
        if (searchInUsername && contains(entry.username)) matches = true;
        if (searchInUrl && contains(entry.url)) matches = true;
        if (searchInNotes && contains(entry.notes)) matches = true;

        if (searchInTags) {
            for (const auto& tag : entry.tags) {
                if (contains(tag)) {
                    matches = true;
                    break;
                }
            }
        }

        if (matches) {
            results.emplace_back(key, std::move(entry));
        }
    }

    updateLastActivity();
    return results;
}

std::vector<std::pair<std::string, PasswordEntry>> PasswordManager::filterByCategory(
    PasswordCategory category) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return {};
    }

    std::vector<std::pair<std::string, PasswordEntry>> results;
    auto allKeys = getAllKeys();

    for (const auto& key : allKeys) {
        auto entry = retrievePassword(key);
        if (entry.password.empty()) continue;

        if (entry.category == category) {
            results.emplace_back(key, std::move(entry));
        }
    }

    updateLastActivity();
    return results;
}

std::vector<std::pair<std::string, PasswordEntry>> PasswordManager::getExpiringPasswords(
    int daysAhead) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return {};
    }

    std::vector<std::pair<std::string, PasswordEntry>> results;
    auto allKeys = getAllKeys();
    auto now = std::chrono::system_clock::now();
    auto futureTime = now + std::chrono::hours(24 * daysAhead);

    for (const auto& key : allKeys) {
        auto entry = retrievePassword(key);
        if (entry.password.empty()) continue;

        if (entry.expires != std::chrono::system_clock::time_point{} &&
            entry.expires <= futureTime) {
            results.emplace_back(key, std::move(entry));
        }
    }

    updateLastActivity();
    return results;
}

std::string PasswordManager::generatePassword(int length,
                                             bool includeUppercase,
                                             bool includeNumbers,
                                             bool includeSpecial) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return "";
    }

    PasswordGenerator::GenerationOptions options;
    options.length = (length > 0) ? length : settings_.minPasswordLength;
    options.includeUppercase = includeUppercase;
    options.includeDigits = includeNumbers;
    options.includeSpecial = includeSpecial;

    auto result = PasswordGenerator::generatePassword(options);
    updateLastActivity();

    return result.isError() ? "" : result.value();
}

PasswordValidator::AnalysisResult PasswordManager::analyzePassword(std::string_view password) {
    std::lock_guard<std::mutex> lock(mutex_);
    updateLastActivity();
    return PasswordValidator::analyzePassword(password);
}

Result<std::string> PasswordManager::exportToJson() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return Result<std::string>("Password manager is locked");
    }

    std::vector<PasswordEntry> entries;
    auto allKeys = getAllKeys();

    for (const auto& key : allKeys) {
        auto entry = retrievePassword(key);
        if (!entry.password.empty()) {
            entries.push_back(std::move(entry));
        }
    }

    updateLastActivity();
    return JsonSerializer::serializeEntries(entries);
}

Result<int> PasswordManager::importFromJson(const std::string& json, bool overwriteExisting) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!ensureUnlocked()) {
        return Result<int>("Password manager is locked");
    }

    auto entriesResult = JsonSerializer::deserializeEntries(json);
    if (entriesResult.isError()) {
        return Result<int>("Failed to parse JSON: " + entriesResult.error());
    }

    const auto& entries = entriesResult.value();
    int importedCount = 0;

    for (const auto& entry : entries) {
        // Generate a key from title and username
        std::string key = entry.title;
        if (!entry.username.empty()) {
            key += "_" + entry.username;
        }

        // Check if entry already exists
        if (!overwriteExisting) {
            auto existingEntry = retrievePassword(key);
            if (!existingEntry.password.empty()) {
                continue; // Skip existing entry
            }
        }

        if (storePassword(key, entry)) {
            importedCount++;
        }
    }

    updateLastActivity();
    return Result<int>(importedCount);
}

const PasswordManagerSettings& PasswordManager::getSettings() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

bool PasswordManager::updateSettings(const PasswordManagerSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    updateLastActivity();
    return true;
}

PasswordManager::Statistics PasswordManager::getStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats;

    if (!ensureUnlocked()) {
        return stats;
    }

    auto allKeys = getAllKeys();
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> passwords;

    for (const auto& key : allKeys) {
        auto entry = retrievePassword(key);
        if (entry.password.empty()) continue;

        stats.totalEntries++;

        // Check if expired
        if (entry.expires != std::chrono::system_clock::time_point{} &&
            entry.expires <= now) {
            stats.expiredEntries++;
        }

        // Check password strength
        auto analysis = PasswordValidator::analyzePassword(entry.password);
        if (analysis.strength == PasswordStrength::Weak ||
            analysis.strength == PasswordStrength::VeryWeak) {
            stats.weakPasswords++;
        }

        // Check for duplicates
        if (std::find(passwords.begin(), passwords.end(), entry.password) != passwords.end()) {
            stats.duplicatePasswords++;
        } else {
            passwords.push_back(entry.password);
        }

        // Update last modified time
        if (entry.modified > stats.lastModified) {
            stats.lastModified = entry.modified;
        }
    }

    updateLastActivity();
    return stats;
}

// ============================================================================
// Private Methods
// ============================================================================

Result<std::vector<uint8_t>> PasswordManager::deriveMasterKey(std::string_view masterPassword) {
    return KeyDerivation::deriveKey(
        masterPassword,
        masterSalt_,
        settings_.encryptionOptions.keyIterations,
        32  // 256-bit key
    );
}

Result<EncryptedData> PasswordManager::encryptData(const std::string& data) {
    return Encryption::encryptWithKey(data, masterKey_, settings_.encryptionOptions);
}

Result<std::string> PasswordManager::decryptData(const EncryptedData& encryptedData) {
    return Encryption::decryptWithKey(encryptedData, masterKey_);
}

void PasswordManager::checkAutoLock() {
    if (settings_.autoLockTimeoutSeconds <= 0) {
        return; // Auto-lock disabled
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity_);

    if (elapsed.count() >= settings_.autoLockTimeoutSeconds) {
        PasswordManager::lock();
    }
}

void PasswordManager::updateLastActivity() {
    lastActivity_ = std::chrono::system_clock::now();
}

bool PasswordManager::ensureUnlocked() {
    checkAutoLock();
    return !isLocked_;
}

}  // namespace atom::secret
