/*
 * entry_example.cpp
 *
 * Demonstrates PasswordEntry usage in the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/utils.hpp"
#include "atom/secret/password/entry.hpp"

using namespace atom::secret;

void printEntry(const PasswordEntry& entry) {
    std::cout << "ID: " << entry.id << std::endl;
    std::cout << "Title: " << entry.title << std::endl;
    std::cout << "Username: " << entry.username << std::endl;
    std::cout << "Password: " << Utils::mask(entry.password, 2, 2) << std::endl;
    std::cout << "URL: " << entry.url << std::endl;
    std::cout << "Email: " << entry.email << std::endl;
    std::cout << "Category: " << categoryToString(entry.category) << std::endl;
    std::cout << "Favorite: " << (entry.isFavorite ? "Yes" : "No") << std::endl;
    std::cout << "Created: " << Utils::formatTimeIso8601(entry.created)
              << std::endl;
    std::cout << "Modified: " << Utils::formatTimeIso8601(entry.modified)
              << std::endl;
    std::cout << "Access Count: " << entry.accessCount << std::endl;

    if (!entry.tags.empty()) {
        std::cout << "Tags: ";
        for (size_t i = 0; i < entry.tags.size(); ++i) {
            std::cout << entry.tags[i];
            if (i < entry.tags.size() - 1)
                std::cout << ", ";
        }
        std::cout << std::endl;
    }

    if (!entry.customFields.empty()) {
        std::cout << "Custom Fields:" << std::endl;
        for (const auto& field : entry.customFields) {
            std::cout << "  " << field.name << ": "
                      << (field.isProtected ? Utils::mask(field.value)
                                            : field.value)
                      << std::endl;
        }
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Atom Secret Password Entry Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Creating a Password Entry
    // ========================================================================
    std::cout << "--- Creating Password Entry ---" << std::endl;

    PasswordEntry entry = PasswordEntry::create();
    entry.title = "GitHub Account";
    entry.username = "developer";
    entry.password = "MySecureGitHubPassword123!";
    entry.url = "https://github.com";
    entry.email = "developer@example.com";
    entry.notes = "Main development account";
    entry.category = PasswordCategory::Development;
    entry.isFavorite = true;

    printEntry(entry);

    // ========================================================================
    // Adding Tags
    // ========================================================================
    std::cout << "--- Adding Tags ---" << std::endl;

    entry.addTag("work");
    entry.addTag("development");
    entry.addTag("important");

    std::cout << "Tags after adding: ";
    for (const auto& tag : entry.tags) {
        std::cout << tag << " ";
    }
    std::cout << std::endl;

    // Try adding duplicate
    entry.addTag("work");
    std::cout << "Tags after adding duplicate 'work': " << entry.tags.size()
              << " tags" << std::endl;

    // Remove tag
    entry.removeTag("important");
    std::cout << "Tags after removing 'important': ";
    for (const auto& tag : entry.tags) {
        std::cout << tag << " ";
    }
    std::cout << std::endl;

    // Check tag
    std::cout << "Has 'work' tag: " << (entry.hasTag("work") ? "Yes" : "No")
              << std::endl;
    std::cout << "Has 'personal' tag: "
              << (entry.hasTag("personal") ? "Yes" : "No") << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Custom Fields
    // ========================================================================
    std::cout << "--- Custom Fields ---" << std::endl;

    entry.addCustomField(
        CustomField("API Token", "ghp_xxxxxxxxxxxx", true, false));
    entry.addCustomField(
        CustomField("SSH Key Path", "~/.ssh/id_ed25519", false, false));
    entry.addCustomField(
        CustomField("Notes", "Remember to rotate token monthly", false, true));

    std::cout << "Custom fields:" << std::endl;
    for (const auto& field : entry.customFields) {
        std::cout << "  " << field.name << ": "
                  << (field.isProtected ? "[PROTECTED]" : field.value)
                  << (field.isMultiline ? " (multiline)" : "") << std::endl;
    }

    auto* apiToken = entry.getCustomField("API Token");
    if (apiToken) {
        std::cout << "Retrieved API Token field: "
                  << Utils::mask(apiToken->value, 4, 0) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Password History
    // ========================================================================
    std::cout << "--- Password History ---" << std::endl;

    std::cout << "Current password: " << Utils::mask(entry.password, 2, 2)
              << std::endl;

    entry.updatePassword("NewSecurePassword456!");
    std::cout << "After update: " << Utils::mask(entry.password, 2, 2)
              << std::endl;
    std::cout << "History size: " << entry.passwordHistory.size() << std::endl;

    entry.updatePassword("AnotherPassword789!");
    std::cout << "After second update: " << Utils::mask(entry.password, 2, 2)
              << std::endl;
    std::cout << "History size: " << entry.passwordHistory.size() << std::endl;

    if (!entry.passwordHistory.empty()) {
        std::cout << "Previous passwords:" << std::endl;
        for (const auto& prev : entry.passwordHistory) {
            std::cout << "  " << Utils::mask(prev.password, 2, 2)
                      << " (changed: "
                      << Utils::formatTimeIso8601(prev.changedAt) << ")"
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Access Tracking
    // ========================================================================
    std::cout << "--- Access Tracking ---" << std::endl;

    std::cout << "Initial access count: " << entry.accessCount << std::endl;

    entry.markAccessed();
    entry.markAccessed();
    entry.markAccessed();

    std::cout << "After 3 accesses: " << entry.accessCount << std::endl;
    std::cout << "Last accessed: "
              << Utils::formatTimeIso8601(entry.lastAccessed) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Expiration
    // ========================================================================
    std::cout << "--- Expiration ---" << std::endl;

    std::cout << "Is expired (no expiration set): "
              << (entry.isExpired() ? "Yes" : "No") << std::endl;

    // Set expiration to 30 days from now
    entry.expires =
        std::chrono::system_clock::now() + std::chrono::hours(24 * 30);
    std::cout << "Expires: " << Utils::formatTimeIso8601(entry.expires)
              << std::endl;
    std::cout << "Is expired: " << (entry.isExpired() ? "Yes" : "No")
              << std::endl;
    std::cout << "Expiring within 60 days: "
              << (entry.isExpiringSoon(60) ? "Yes" : "No") << std::endl;
    std::cout << "Expiring within 7 days: "
              << (entry.isExpiringSoon(7) ? "Yes" : "No") << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Categories
    // ========================================================================
    std::cout << "--- Password Categories ---" << std::endl;

    std::cout << "Available categories:" << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::General)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Finance)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Social)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Work) << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Email) << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Shopping)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Development)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Entertainment)
              << std::endl;
    std::cout << "  " << categoryToString(PasswordCategory::Personal)
              << std::endl;

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
