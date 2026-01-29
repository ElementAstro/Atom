#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <memory>

#include <vector>

#include "atom/secret/password_manager.hpp"
#include "atom/secret/storage.hpp"

using namespace atom::secret;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;

// Test fixture for PasswordManager tests
class PasswordManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test password manager with app name and simple settings to
        // avoid deadlocks
        PasswordManagerSettings settings;
        settings.autoLockTimeoutSeconds = 0;  // Disable auto-lock
        settings.minPasswordLength = 8;
        settings.requireSpecialChars = false;
        settings.encryptionOptions.keyIterations =
            1000;  // Reduce iterations for faster tests

        manager = std::make_unique<PasswordManager>("TestApp", settings);

        // Setup test values
        masterPassword = "TestMasterPassword123!";
        testTitle = "TestEntry";
        testEntry = PasswordEntry{};
        testEntry.title = testTitle;
        testEntry.password = "TestPassword123!";
        testEntry.username = "testuser@example.com";
        testEntry.url = "https://example.com/login";
        testEntry.notes = "Test account notes";
        testEntry.category = PasswordCategory::Personal;
        testEntry.created = std::chrono::system_clock::now();
        testEntry.modified = std::chrono::system_clock::now();
        testEntry.expires = std::chrono::system_clock::now() +
                            std::chrono::hours(24 * 90);  // 90 days from now
        testEntry.previousPasswords = {"OldPassword1!", "OldPassword2@"};

        // Define test export file path
        exportFilePath = "test_password_export.json";
    }

    void TearDown() override {
        // Clean up
        manager.reset();

        // Clean up any created export file
        std::remove(exportFilePath.c_str());
    }

    // Helper function to create a test entry with a given password
    PasswordEntry createTestEntry(const std::string& password,
                                  const std::string& title) {
        PasswordEntry entry = testEntry;
        entry.password = password;
        entry.title = title;
        return entry;
    }

    std::unique_ptr<PasswordManager> manager;
    std::string masterPassword;
    std::string testTitle;
    PasswordEntry testEntry;
    std::string exportFilePath;
};

// Test basic password manager functionality
TEST_F(PasswordManagerTest, BasicFunctionality) {
    // Test with custom settings
    PasswordManagerSettings settings;
    settings.autoLockTimeoutSeconds = 60;
    settings.minPasswordLength = 16;
    settings.encryptionOptions.keyIterations = 20000;

    auto newManager = std::make_unique<PasswordManager>("TestApp", settings);

    // Verify settings were applied
    PasswordManagerSettings appliedSettings = newManager->getSettings();
    EXPECT_EQ(appliedSettings.autoLockTimeoutSeconds, 60);
    EXPECT_EQ(appliedSettings.minPasswordLength, 16);
    EXPECT_EQ(appliedSettings.encryptionOptions.keyIterations, 20000);
}

// Test adding and retrieving entries
TEST_F(PasswordManagerTest, AddAndRetrieveEntry) {
    // Add an entry
    auto result = manager->addEntry(testEntry, masterPassword);
    if (!result.isSuccess()) {
        std::cout << "Add entry failed with error: "
                  << static_cast<int>(result.errorCode()) << std::endl;
    }
    EXPECT_TRUE(result.isSuccess());

    // Retrieve the entry
    auto retrieveResult = manager->getEntry(testTitle, masterPassword);
    if (!retrieveResult.isSuccess()) {
        std::cout << "Get entry failed with error: "
                  << static_cast<int>(retrieveResult.errorCode()) << std::endl;
    }
    EXPECT_TRUE(retrieveResult.isSuccess());

    if (retrieveResult.isSuccess()) {
        const auto& retrievedEntry = retrieveResult.value();
        EXPECT_EQ(retrievedEntry.password, testEntry.password);
        EXPECT_EQ(retrievedEntry.username, testEntry.username);
        EXPECT_EQ(retrievedEntry.url, testEntry.url);
        EXPECT_EQ(retrievedEntry.notes, testEntry.notes);
        EXPECT_EQ(retrievedEntry.category, testEntry.category);
    }
}

// Test updating entries
TEST_F(PasswordManagerTest, UpdateEntry) {
    // Add an entry first
    auto result = manager->addEntry(testEntry, masterPassword);
    EXPECT_TRUE(result.isSuccess());

    // Update the entry
    testEntry.password = "NewPassword456!";
    testEntry.notes = "Updated notes";
    auto updateResult = manager->updateEntry(testEntry, masterPassword);
    EXPECT_TRUE(updateResult.isSuccess());

    // Retrieve and verify the update
    auto retrieveResult = manager->getEntry(testTitle, masterPassword);
    EXPECT_TRUE(retrieveResult.isSuccess());

    if (retrieveResult.isSuccess()) {
        const auto& retrievedEntry = retrieveResult.value();
        EXPECT_EQ(retrievedEntry.password, "NewPassword456!");
        EXPECT_EQ(retrievedEntry.notes, "Updated notes");
    }
}

// Test removing entries
TEST_F(PasswordManagerTest, RemoveEntry) {
    // Add an entry first
    auto result = manager->addEntry(testEntry, masterPassword);
    EXPECT_TRUE(result.isSuccess());

    // Verify it exists
    auto retrieveResult = manager->getEntry(testTitle, masterPassword);
    EXPECT_TRUE(retrieveResult.isSuccess());

    // Remove the entry
    auto removeResult = manager->removeEntry(testTitle);
    EXPECT_TRUE(removeResult.isSuccess());

    // Verify it's gone
    auto retrieveResult2 = manager->getEntry(testTitle, masterPassword);
    EXPECT_FALSE(retrieveResult2.isSuccess());
    EXPECT_EQ(retrieveResult2.errorCode(), ErrorCode::EntryNotFound);
}

// Test password generation
TEST_F(PasswordManagerTest, PasswordGeneration) {
    // Generate a password with default options
    auto result = manager->generatePassword();
    EXPECT_TRUE(result.isSuccess());

    if (result.isSuccess()) {
        const auto& password = result.value();
        EXPECT_FALSE(password.empty());
        EXPECT_GE(password.length(), 16);  // Default length
    }

    // Generate a password with custom options
    PasswordGenerationOptions options;
    options.length = 20;
    options.includeSpecialChars = false;

    auto result2 = manager->generatePassword(options);
    EXPECT_TRUE(result2.isSuccess());

    if (result2.isSuccess()) {
        const auto& password = result2.value();
        EXPECT_EQ(password.length(), 20);
        // Check that no special characters are present
        bool hasSpecial = false;
        for (char c : password) {
            if (!std::isalnum(c)) {
                hasSpecial = true;
                break;
            }
        }
        EXPECT_FALSE(hasSpecial);
    }
}

// Test password strength analysis
TEST_F(PasswordManagerTest, PasswordStrengthAnalysis) {
    // Test various password strengths
    EXPECT_EQ(manager->analyzePasswordStrength("abc"),
              PasswordStrength::VeryWeak);
    EXPECT_EQ(manager->analyzePasswordStrength("abcdefgh123456"),
              PasswordStrength::Weak);
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456"),
              PasswordStrength::Medium);
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456!"),
              PasswordStrength::Strong);
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456!@#$%^&*()"),
              PasswordStrength::VeryStrong);
}

// Test settings management
TEST_F(PasswordManagerTest, SettingsManagement) {
    // Get current settings
    auto currentSettings = manager->getSettings();
    EXPECT_GT(currentSettings.minPasswordLength, 0);

    // Update settings
    PasswordManagerSettings newSettings = currentSettings;
    newSettings.minPasswordLength = 20;
    newSettings.requireSpecialChars = false;

    manager->updateSettings(newSettings);

    // Verify settings were updated
    auto updatedSettings = manager->getSettings();
    EXPECT_EQ(updatedSettings.minPasswordLength, 20);
    EXPECT_FALSE(updatedSettings.requireSpecialChars);
}

// Test getting all entries
TEST_F(PasswordManagerTest, GetAllEntries) {
    // Initially there should be no entries
    auto result = manager->getAllEntries(masterPassword);
    EXPECT_TRUE(result.isSuccess());
    if (result.isSuccess()) {
        EXPECT_TRUE(result.value().empty());
    }

    // Add some entries
    auto entry1 = createTestEntry("Password1", "Entry1");
    auto entry2 = createTestEntry("Password2", "Entry2");

    EXPECT_TRUE(manager->addEntry(entry1, masterPassword).isSuccess());
    EXPECT_TRUE(manager->addEntry(entry2, masterPassword).isSuccess());

    // Get all entries and verify
    auto allResult = manager->getAllEntries(masterPassword);
    EXPECT_TRUE(allResult.isSuccess());
    if (allResult.isSuccess()) {
        const auto& entries = allResult.value();
        EXPECT_EQ(entries.size(), 2);
    }
}

// Test search functionality
TEST_F(PasswordManagerTest, SearchEntries) {
    // Add some test entries
    auto entry1 = createTestEntry("Password1", "Gmail");
    entry1.username = "user@gmail.com";
    entry1.url = "https://gmail.com";

    auto entry2 = createTestEntry("Password2", "Yahoo");
    entry2.username = "user@yahoo.com";
    entry2.url = "https://yahoo.com";

    EXPECT_TRUE(manager->addEntry(entry1, masterPassword).isSuccess());
    EXPECT_TRUE(manager->addEntry(entry2, masterPassword).isSuccess());

    // Search for entries
    SearchOptions options;
    options.query = "gmail";
    options.searchInUrl = true;

    auto searchResult = manager->searchEntries(options, masterPassword);
    EXPECT_TRUE(searchResult.isSuccess());

    if (searchResult.isSuccess()) {
        const auto& entries = searchResult.value();
        EXPECT_EQ(entries.size(), 1);
        EXPECT_EQ(entries[0].title, "Gmail");
    }
}

// Test password strength evaluation
TEST_F(PasswordManagerTest, AnalyzePasswordStrength) {
    // Very weak password (too short)
    EXPECT_EQ(manager->analyzePasswordStrength("abc123"),
              PasswordStrength::VeryWeak);

    // Weak password (only lowercase and numbers)
    EXPECT_EQ(manager->analyzePasswordStrength("abcdefgh123456"),
              PasswordStrength::Weak);

    // Medium password (mixed case and numbers)
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456"),
              PasswordStrength::Medium);

    // Strong password (mixed case, numbers, and special)
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456!"),
              PasswordStrength::Strong);

    // Very strong password (long, mixed case, numbers, special)
    EXPECT_EQ(manager->analyzePasswordStrength("Abcdefgh123456!@#$%^&*()"),
              PasswordStrength::VeryStrong);
}
