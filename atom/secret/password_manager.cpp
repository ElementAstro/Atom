#include "password_manager.hpp"

#include <spdlog/spdlog.h>
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

PasswordManager::PasswordManager(std::string_view appName)
    : storage_(SecureStorage::create(appName)), settings_{}, appName_(appName) {
    spdlog::info("PasswordManager initialized for app: {}", appName);
}

Result<void> PasswordManager::addEntry(const PasswordEntry& entry,
                                       std::string_view masterPassword) {
    std::unique_lock lock(mutex_);
    if (entry.title.empty()) {
        return Result<void>::Error("Entry title cannot be empty.");
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
        return Result<void>::Error("Encryption failed: " +
                                   encrypted_result.error());
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

    if (!storage_->store(entry.title, storable_data)) {
        return Result<void>::Error("Failed to store entry.");
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
    std::string storable_data = storage_->retrieve(title);
    if (storable_data.empty()) {
        return Result<PasswordEntry>::Error("Entry not found.");
    }

    if (storable_data.length() < 44) {  // 16 salt + 12 iv + 16 aad
        return Result<PasswordEntry>::Error("Invalid stored data: too short.");
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
        return Result<PasswordEntry>::Error("Decryption failed: " +
                                            decrypted_result.error());
    }

    try {
        nlohmann::json j = nlohmann::json::parse(decrypted_result.value());
        PasswordEntry entry = j.get<PasswordEntry>();
        return Result<PasswordEntry>(std::move(entry));
    } catch (const nlohmann::json::exception& e) {
        return Result<PasswordEntry>::Error("JSON parsing failed: " +
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
    if (storage_->remove(title)) {
        return Result<void>();
    }
    return Result<void>::Error("Failed to remove entry.");
}

Result<std::vector<PasswordEntry>> PasswordManager::getAllEntries(
    std::string_view masterPassword) const {
    std::shared_lock lock(mutex_);
    auto keys = storage_->getAllKeys();
    std::vector<PasswordEntry> entries;
    entries.reserve(keys.size());
    for (const auto& key : keys) {
        // Call the non-locking version to avoid recursive lock issues.
        auto entry_result = getEntry_nolock(key, masterPassword);
        if (entry_result.isSuccess()) {
            entries.push_back(std::move(entry_result.value()));
        } else {
            spdlog::warn("Failed to decrypt entry with key '{}': {}", key,
                         entry_result.error());
        }
    }
    return Result(std::move(entries));
}

}  // namespace atom::secret
