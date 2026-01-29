/*
 * password_manager_example.cpp
 *
 * Demonstrates the PasswordManager usage in the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <filesystem>
#include <iostream>

#include "atom/secret/core/utils.hpp"
#include "atom/secret/manager/password_manager.hpp"
#include "atom/secret/password/generator.hpp"
#include "atom/secret/storage/storage.hpp"

using namespace atom::secret;

void printEntry(const PasswordEntry& entry) {
    std::cout << "  ID: " << entry.id << std::endl;
    std::cout << "  Title: " << entry.title << std::endl;
    std::cout << "  Username: " << entry.username << std::endl;
    std::cout << "  Password: " << Utils::mask(entry.password, 2, 2)
              << std::endl;
    std::cout << "  URL: " << entry.url << std::endl;
    std::cout << "  Category: " << categoryToString(entry.category)
              << std::endl;
    std::cout << "  Favorite: " << (entry.isFavorite ? "Yes" : "No")
              << std::endl;
}

int main() {
    std::cout << "=== Atom Secret Password Manager Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Create Password Manager
    // ========================================================================
    std::cout << "--- Create Password Manager ---" << std::endl;

    auto storage = SecureStorage::create(StorageBackend::Memory);
    PasswordManager manager(std::move(storage));

    // Initialize with master password
    std::string masterPassword = "MySecureMasterPassword123!";
    auto initResult = manager.initialize(masterPassword);
    if (initResult.isError()) {
        std::cerr << "Failed to initialize: " << initResult.errorMessage()
                  << std::endl;
        return 1;
    }

    std::cout << "Password manager initialized successfully" << std::endl;
    std::cout << "Is unlocked: " << (manager.isUnlocked() ? "Yes" : "No")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Add Password Entries
    // ========================================================================
    std::cout << "--- Add Password Entries ---" << std::endl;

    // GitHub account
    PasswordEntry github;
    github.title = "GitHub";
    github.username = "developer";
    github.password = "gh_password_123!";
    github.url = "https://github.com";
    github.email = "dev@example.com";
    github.category = PasswordCategory::Development;
    github.isFavorite = true;
    github.addTag("work");
    github.addTag("code");

    auto addResult1 = manager.addEntry(github);
    if (addResult1.isSuccess()) {
        std::cout << "Added GitHub entry with ID: " << addResult1.value()
                  << std::endl;
    }

    // Gmail account
    PasswordEntry gmail;
    gmail.title = "Gmail";
    gmail.username = "user@gmail.com";
    gmail.password = "gmail_secure_pass!";
    gmail.url = "https://mail.google.com";
    gmail.category = PasswordCategory::Email;

    auto addResult2 = manager.addEntry(gmail);
    if (addResult2.isSuccess()) {
        std::cout << "Added Gmail entry with ID: " << addResult2.value()
                  << std::endl;
    }

    // Bank account
    PasswordEntry bank;
    bank.title = "Chase Bank";
    bank.username = "customer123";
    bank.password = "bank_super_secure!";
    bank.url = "https://chase.com";
    bank.category = PasswordCategory::Finance;
    bank.isFavorite = true;

    auto addResult3 = manager.addEntry(bank);
    if (addResult3.isSuccess()) {
        std::cout << "Added Bank entry with ID: " << addResult3.value()
                  << std::endl;
    }

    std::cout << "Total entries: " << manager.getEntryCount() << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Get Entry
    // ========================================================================
    std::cout << "--- Get Entry ---" << std::endl;

    if (addResult1.isSuccess()) {
        auto getResult = manager.getEntry(addResult1.value());
        if (getResult.isSuccess()) {
            std::cout << "Retrieved entry:" << std::endl;
            printEntry(getResult.value());
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Search Entries
    // ========================================================================
    std::cout << "--- Search Entries ---" << std::endl;

    // Search by query
    SearchFilter queryFilter;
    queryFilter.query = "gmail";
    auto searchResult1 = manager.search(queryFilter);
    if (searchResult1.isSuccess()) {
        std::cout << "Search 'gmail': " << searchResult1.value().size()
                  << " result(s)" << std::endl;
    }

    // Search by category
    SearchFilter categoryFilter;
    categoryFilter.categories = {PasswordCategory::Finance};
    auto searchResult2 = manager.search(categoryFilter);
    if (searchResult2.isSuccess()) {
        std::cout << "Search Finance category: " << searchResult2.value().size()
                  << " result(s)" << std::endl;
    }

    // Search favorites
    SearchFilter favFilter;
    favFilter.favoritesOnly = true;
    auto searchResult3 = manager.search(favFilter);
    if (searchResult3.isSuccess()) {
        std::cout << "Search favorites: " << searchResult3.value().size()
                  << " result(s)" << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Update Password
    // ========================================================================
    std::cout << "--- Update Password ---" << std::endl;

    if (addResult1.isSuccess()) {
        // Generate new password
        auto newPwResult = PasswordGenerator::generate();
        if (newPwResult.isSuccess()) {
            auto updateResult =
                manager.updatePassword(addResult1.value(), newPwResult.value());
            if (updateResult.isSuccess()) {
                std::cout << "Password updated for GitHub entry" << std::endl;

                auto pwResult = manager.getPassword(addResult1.value());
                if (pwResult.isSuccess()) {
                    std::cout << "New password: "
                              << Utils::mask(pwResult.value(), 4, 4)
                              << std::endl;
                }
            }
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Lock and Unlock
    // ========================================================================
    std::cout << "--- Lock and Unlock ---" << std::endl;

    manager.lock();
    std::cout << "Manager locked. Is unlocked: "
              << (manager.isUnlocked() ? "Yes" : "No") << std::endl;

    // Try to access while locked
    auto lockedResult = manager.getAllEntries();
    if (lockedResult.isError()) {
        std::cout << "Access denied while locked: "
                  << lockedResult.errorMessage() << std::endl;
    }

    // Unlock
    auto unlockResult = manager.unlock(masterPassword);
    if (unlockResult.isSuccess()) {
        std::cout << "Manager unlocked. Is unlocked: "
                  << (manager.isUnlocked() ? "Yes" : "No") << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Export to JSON
    // ========================================================================
    std::cout << "--- Export to JSON ---" << std::endl;

    auto exportResult = manager.exportToJson();
    if (exportResult.isSuccess()) {
        std::cout << "Exported JSON length: " << exportResult.value().length()
                  << " bytes" << std::endl;
        std::cout << "Preview: " << exportResult.value().substr(0, 100) << "..."
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Delete Entry
    // ========================================================================
    std::cout << "--- Delete Entry ---" << std::endl;

    if (addResult2.isSuccess()) {
        auto deleteResult = manager.deleteEntry(addResult2.value());
        if (deleteResult.isSuccess()) {
            std::cout << "Deleted Gmail entry" << std::endl;
            std::cout << "Remaining entries: " << manager.getEntryCount()
                      << std::endl;
        }
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
