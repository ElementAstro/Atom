/*
 * json_example.cpp
 *
 * Demonstrates JSON serialization in the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>

#include "atom/secret/core/utils.hpp"
#include "atom/secret/password/entry.hpp"
#include "atom/secret/serialization/json.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret JSON Serialization Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Create a Password Entry
    // ========================================================================
    std::cout << "--- Create Password Entry ---" << std::endl;

    PasswordEntry entry = PasswordEntry::create();
    entry.title = "Example Account";
    entry.username = "user@example.com";
    entry.password = "SecurePassword123!";
    entry.url = "https://example.com";
    entry.email = "user@example.com";
    entry.notes = "This is a test account for demonstration.";
    entry.category = PasswordCategory::Work;
    entry.isFavorite = true;
    entry.addTag("work");
    entry.addTag("demo");
    entry.addCustomField(CustomField("API Key", "sk-1234567890", true, false));
    entry.addCustomField(
        CustomField("Notes", "Multi-line\nnotes\nhere", false, true));

    std::cout << "Created entry: " << entry.title << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Serialize Single Entry
    // ========================================================================
    std::cout << "--- Serialize Single Entry ---" << std::endl;

    auto serializeResult = JsonSerializer::serializeEntry(entry);
    if (serializeResult.isSuccess()) {
        std::cout << "Serialized (compact):" << std::endl;
        std::cout << serializeResult.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Serialize with Pretty Print
    // ========================================================================
    std::cout << "--- Serialize with Pretty Print ---" << std::endl;

    auto prettyResult = JsonSerializer::serializeEntry(entry, true);
    if (prettyResult.isSuccess()) {
        std::cout << "Serialized (pretty):" << std::endl;
        std::cout << prettyResult.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Deserialize Entry
    // ========================================================================
    std::cout << "--- Deserialize Entry ---" << std::endl;

    if (serializeResult.isSuccess()) {
        auto deserializeResult =
            JsonSerializer::deserializeEntry(serializeResult.value());
        if (deserializeResult.isSuccess()) {
            const auto& deserialized = deserializeResult.value();
            std::cout << "Deserialized entry:" << std::endl;
            std::cout << "  Title: " << deserialized.title << std::endl;
            std::cout << "  Username: " << deserialized.username << std::endl;
            std::cout << "  Password: "
                      << Utils::mask(deserialized.password, 2, 2) << std::endl;
            std::cout << "  Category: "
                      << categoryToString(deserialized.category) << std::endl;
            std::cout << "  Tags: ";
            for (const auto& tag : deserialized.tags) {
                std::cout << tag << " ";
            }
            std::cout << std::endl;
            std::cout << "  Custom fields: " << deserialized.customFields.size()
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Serialize Multiple Entries
    // ========================================================================
    std::cout << "--- Serialize Multiple Entries ---" << std::endl;

    std::vector<PasswordEntry> entries;

    PasswordEntry entry1 = PasswordEntry::create();
    entry1.title = "GitHub";
    entry1.username = "developer";
    entry1.password = "gh_pass_123";
    entries.push_back(entry1);

    PasswordEntry entry2 = PasswordEntry::create();
    entry2.title = "Gmail";
    entry2.username = "user@gmail.com";
    entry2.password = "gmail_pass_456";
    entries.push_back(entry2);

    PasswordEntry entry3 = PasswordEntry::create();
    entry3.title = "AWS";
    entry3.username = "admin";
    entry3.password = "aws_pass_789";
    entries.push_back(entry3);

    auto multiResult = JsonSerializer::serializeEntries(entries, true);
    if (multiResult.isSuccess()) {
        std::cout << "Serialized " << entries.size()
                  << " entries:" << std::endl;
        std::cout << multiResult.value().substr(0, 500) << "..." << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Deserialize Multiple Entries
    // ========================================================================
    std::cout << "--- Deserialize Multiple Entries ---" << std::endl;

    if (multiResult.isSuccess()) {
        auto multiDeserialize =
            JsonSerializer::deserializeEntries(multiResult.value());
        if (multiDeserialize.isSuccess()) {
            std::cout << "Deserialized " << multiDeserialize.value().size()
                      << " entries:" << std::endl;
            for (const auto& e : multiDeserialize.value()) {
                std::cout << "  - " << e.title << " (" << e.username << ")"
                          << std::endl;
            }
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Serialize with Keys
    // ========================================================================
    std::cout << "--- Serialize with Keys ---" << std::endl;

    std::vector<std::pair<std::string, PasswordEntry>> keyedEntries;
    keyedEntries.push_back({"key_github", entry1});
    keyedEntries.push_back({"key_gmail", entry2});
    keyedEntries.push_back({"key_aws", entry3});

    auto keyedResult =
        JsonSerializer::serializeEntriesWithKeys(keyedEntries, true);
    if (keyedResult.isSuccess()) {
        std::cout << "Serialized with keys:" << std::endl;
        std::cout << keyedResult.value().substr(0, 300) << "..." << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Error Handling
    // ========================================================================
    std::cout << "--- Error Handling ---" << std::endl;

    auto invalidResult = JsonSerializer::deserializeEntry("{ invalid json }");
    if (invalidResult.isError()) {
        std::cout << "Invalid JSON error: " << invalidResult.errorMessage()
                  << std::endl;
    }

    auto emptyResult = JsonSerializer::deserializeEntry("");
    if (emptyResult.isError()) {
        std::cout << "Empty JSON error: " << emptyResult.errorMessage()
                  << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
