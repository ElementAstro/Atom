#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/system/wregistry.hpp"

#ifdef _WIN32
// Only run tests on Windows platforms
using namespace atom::system;
using namespace std::chrono_literals;

// Define Windows registry constants if needed
#ifndef HKEY_CLASSES_ROOT
#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)((LONG)0x80000000))
#endif
#ifndef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))
#endif
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
#endif

class WRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test registry keys and values
        test_key = "Software\\AtomTestRegistry";
        CreateTestKeys();
    }

    void TearDown() override {
        // Clean up test registry keys
        CleanupTestKeys();
    }

    void CreateTestKeys() {
        HKEY hKey;
        LONG lRes = RegCreateKeyEx(HKEY_CURRENT_USER, test_key.c_str(), 0,
                                   nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                   nullptr, &hKey, nullptr);

        if (lRes == ERROR_SUCCESS) {
            // Create some test values
            std::string testString = "TestValue";
            RegSetValueEx(hKey, "TestString", 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(testString.c_str()),
                          static_cast<DWORD>(testString.size() + 1));

            DWORD testDword = 12345;
            RegSetValueEx(hKey, "TestDword", 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&testDword),
                          sizeof(DWORD));

            // Create a subkey
            HKEY hSubKey;
            RegCreateKeyEx(hKey, "SubKey1", 0, nullptr, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, nullptr, &hSubKey, nullptr);

            std::string subKeyValue = "SubKeyValue";
            RegSetValueEx(hSubKey, "SubValue", 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(subKeyValue.c_str()),
                          static_cast<DWORD>(subKeyValue.size() + 1));

            RegCloseKey(hSubKey);

            // Create another subkey
            RegCreateKeyEx(hKey, "SubKey2", 0, nullptr, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, nullptr, &hSubKey, nullptr);
            RegCloseKey(hSubKey);

            RegCloseKey(hKey);
        }
    }

    void CleanupTestKeys() {
        // Delete test registry key and all subkeys
        RegDeleteTree(HKEY_CURRENT_USER, test_key.c_str());
    }

    // Helper method for creating temporary files
    std::string CreateTempFile() {
        char tempPath[MAX_PATH];
        GetTempPath(MAX_PATH, tempPath);

        char tempFileName[MAX_PATH];
        GetTempFileName(tempPath, "reg", 0, tempFileName);

        return std::string(tempFileName);
    }

    std::string test_key;
};

// Test getRegistrySubKeys function
TEST_F(WRegistryTest, GetRegistrySubKeys) {
    std::vector<std::string> subKeys;
    bool result = getRegistrySubKeys(HKEY_CURRENT_USER, test_key, subKeys);

    EXPECT_TRUE(result);
    EXPECT_EQ(2, subKeys.size());

    // Check if the correct subkeys are found
    bool foundSubKey1 = false;
    bool foundSubKey2 = false;

    for (const auto& key : subKeys) {
        if (key == "SubKey1")
            foundSubKey1 = true;
        if (key == "SubKey2")
            foundSubKey2 = true;
    }

    EXPECT_TRUE(foundSubKey1);
    EXPECT_TRUE(foundSubKey2);
}

// Test getRegistrySubKeys with non-existent key
TEST_F(WRegistryTest, GetRegistrySubKeysNonExistent) {
    std::vector<std::string> subKeys;
    bool result = getRegistrySubKeys(HKEY_CURRENT_USER,
                                     test_key + "\\NonExistent", subKeys);

    EXPECT_FALSE(result);
    EXPECT_TRUE(subKeys.empty());
}

// Test getRegistryValues function
TEST_F(WRegistryTest, GetRegistryValues) {
    std::vector<std::pair<std::string, std::string>> values;
    bool result = getRegistryValues(HKEY_CURRENT_USER, test_key, values);

    EXPECT_TRUE(result);
    EXPECT_EQ(2, values.size());

    bool foundStringValue = false;
    bool foundDwordValue = false;

    for (const auto& value : values) {
        if (value.first == "TestString") {
            foundStringValue = true;
            EXPECT_EQ("TestValue", value.second);
        }
        if (value.first == "TestDword") {
            foundDwordValue = true;
            EXPECT_EQ("12345", value.second);
        }
    }

    EXPECT_TRUE(foundStringValue);
    EXPECT_TRUE(foundDwordValue);
}

// Test getRegistryValues with a subkey
TEST_F(WRegistryTest, GetRegistryValuesSubKey) {
    std::vector<std::pair<std::string, std::string>> values;
    bool result =
        getRegistryValues(HKEY_CURRENT_USER, test_key + "\\SubKey1", values);

    EXPECT_TRUE(result);
    EXPECT_EQ(1, values.size());
    EXPECT_EQ("SubValue", values[0].first);
    EXPECT_EQ("SubKeyValue", values[0].second);
}

// Test getRegistryValues with non-existent key
TEST_F(WRegistryTest, GetRegistryValuesNonExistent) {
    std::vector<std::pair<std::string, std::string>> values;
    bool result = getRegistryValues(HKEY_CURRENT_USER,
                                    test_key + "\\NonExistent", values);

    EXPECT_FALSE(result);
    EXPECT_TRUE(values.empty());
}

// Test modifyRegistryValue function
TEST_F(WRegistryTest, ModifyRegistryValue) {
    // Modify an existing value
    bool result = modifyRegistryValue(HKEY_CURRENT_USER, test_key, "TestString",
                                      "ModifiedValue");
    EXPECT_TRUE(result);

    // Verify the modification
    std::vector<std::pair<std::string, std::string>> values;
    getRegistryValues(HKEY_CURRENT_USER, test_key, values);

    bool foundModifiedValue = false;
    for (const auto& value : values) {
        if (value.first == "TestString") {
            foundModifiedValue = true;
            EXPECT_EQ("ModifiedValue", value.second);
        }
    }

    EXPECT_TRUE(foundModifiedValue);
}

// Test modifyRegistryValue with new value
TEST_F(WRegistryTest, ModifyRegistryValueNewValue) {
    // Add a new value
    bool result = modifyRegistryValue(HKEY_CURRENT_USER, test_key, "NewValue",
                                      "NewValueData");
    EXPECT_TRUE(result);

    // Verify the new value
    std::vector<std::pair<std::string, std::string>> values;
    getRegistryValues(HKEY_CURRENT_USER, test_key, values);

    bool foundNewValue = false;
    for (const auto& value : values) {
        if (value.first == "NewValue") {
            foundNewValue = true;
            EXPECT_EQ("NewValueData", value.second);
        }
    }

    EXPECT_TRUE(foundNewValue);
}

// Test modifyRegistryValue with non-existent key
TEST_F(WRegistryTest, ModifyRegistryValueNonExistentKey) {
    bool result = modifyRegistryValue(
        HKEY_CURRENT_USER, test_key + "\\NonExistent", "AnyValue", "AnyData");
    EXPECT_FALSE(result);
}

// Test deleteRegistryValue function
TEST_F(WRegistryTest, DeleteRegistryValue) {
    // First verify the value exists
    std::vector<std::pair<std::string, std::string>> values;
    getRegistryValues(HKEY_CURRENT_USER, test_key, values);

    bool valueExists = false;
    for (const auto& value : values) {
        if (value.first == "TestString") {
            valueExists = true;
            break;
        }
    }
    EXPECT_TRUE(valueExists);

    // Delete the value
    bool result =
        deleteRegistryValue(HKEY_CURRENT_USER, test_key, "TestString");
    EXPECT_TRUE(result);

    // Verify it's gone
    values.clear();
    getRegistryValues(HKEY_CURRENT_USER, test_key, values);

    valueExists = false;
    for (const auto& value : values) {
        if (value.first == "TestString") {
            valueExists = true;
            break;
        }
    }
    EXPECT_FALSE(valueExists);
}

// Test deleteRegistryValue with non-existent value
TEST_F(WRegistryTest, DeleteRegistryValueNonExistent) {
    bool result =
        deleteRegistryValue(HKEY_CURRENT_USER, test_key, "NonExistentValue");
    EXPECT_FALSE(result);
}

// Test deleteRegistryValue with non-existent key
TEST_F(WRegistryTest, DeleteRegistryValueNonExistentKey) {
    bool result = deleteRegistryValue(HKEY_CURRENT_USER,
                                      test_key + "\\NonExistent", "AnyValue");
    EXPECT_FALSE(result);
}

// Test deleteRegistrySubKey function
TEST_F(WRegistryTest, DeleteRegistrySubKey) {
    // First verify the subkey exists
    std::vector<std::string> subKeys;
    getRegistrySubKeys(HKEY_CURRENT_USER, test_key, subKeys);

    bool subKeyExists = false;
    for (const auto& key : subKeys) {
        if (key == "SubKey1") {
            subKeyExists = true;
            break;
        }
    }
    EXPECT_TRUE(subKeyExists);

    // Delete the subkey
    std::string fullSubKey = test_key + "\\SubKey1";
    bool result = deleteRegistrySubKey(HKEY_CURRENT_USER, fullSubKey);
    EXPECT_TRUE(result);

    // Verify it's gone
    subKeys.clear();
    getRegistrySubKeys(HKEY_CURRENT_USER, test_key, subKeys);

    subKeyExists = false;
    for (const auto& key : subKeys) {
        if (key == "SubKey1") {
            subKeyExists = true;
            break;
        }
    }
    EXPECT_FALSE(subKeyExists);
}

// Test deleteRegistrySubKey with non-existent key
TEST_F(WRegistryTest, DeleteRegistrySubKeyNonExistent) {
    bool result =
        deleteRegistrySubKey(HKEY_CURRENT_USER, test_key + "\\NonExistent");
    EXPECT_FALSE(result);
}

// Test backupRegistry function
TEST_F(WRegistryTest, BackupRegistry) {
    std::string backupFile = CreateTempFile();
    bool result = backupRegistry(HKEY_CURRENT_USER, test_key, backupFile);

    EXPECT_TRUE(result);

    // Verify the backup file exists and has content
    std::filesystem::path backupPath(backupFile);
    EXPECT_TRUE(std::filesystem::exists(backupPath));
    EXPECT_GT(std::filesystem::file_size(backupPath), 0);

    // Clean up
    std::filesystem::remove(backupPath);
}

// Test backupRegistry with non-existent key
TEST_F(WRegistryTest, BackupRegistryNonExistent) {
    std::string backupFile = CreateTempFile();
    bool result = backupRegistry(HKEY_CURRENT_USER, test_key + "\\NonExistent",
                                 backupFile);

    EXPECT_FALSE(result);

    // Clean up
    std::filesystem::remove(backupFile);
}

// Test exportRegistry function (similar to backupRegistry)
TEST_F(WRegistryTest, ExportRegistry) {
    std::string exportFile = CreateTempFile();
    bool result = exportRegistry(HKEY_CURRENT_USER, test_key, exportFile);

    EXPECT_TRUE(result);

    // Verify the export file exists and has content
    std::filesystem::path exportPath(exportFile);
    EXPECT_TRUE(std::filesystem::exists(exportPath));
    EXPECT_GT(std::filesystem::file_size(exportPath), 0);

    // Clean up
    std::filesystem::remove(exportPath);
}

// Test exportRegistry with non-existent key
TEST_F(WRegistryTest, ExportRegistryNonExistent) {
    std::string exportFile = CreateTempFile();
    bool result = exportRegistry(HKEY_CURRENT_USER, test_key + "\\NonExistent",
                                 exportFile);

    EXPECT_FALSE(result);

    // Clean up
    std::filesystem::remove(exportFile);
}

// Test recursivelyEnumerateRegistrySubKeys function
// This is harder to test since it just logs output, but we can verify it
// doesn't crash
TEST_F(WRegistryTest, RecursivelyEnumerateRegistrySubKeys) {
    // This should not throw or crash
    EXPECT_NO_THROW(
        recursivelyEnumerateRegistrySubKeys(HKEY_CURRENT_USER, test_key));
}

// Test findRegistryKey function
// This is harder to test since it just logs output, but we can verify it
// doesn't crash
TEST_F(WRegistryTest, FindRegistryKey) {
    // This should not throw or crash
    EXPECT_NO_THROW(findRegistryKey(HKEY_CURRENT_USER, test_key, "SubKey1"));
}

// Test findRegistryValue function
// This is harder to test since it just logs output, but we can verify it
// doesn't crash
TEST_F(WRegistryTest, FindRegistryValue) {
    // This should not throw or crash
    EXPECT_NO_THROW(
        findRegistryValue(HKEY_CURRENT_USER, test_key, "TestString"));
}

// Edge case: test with empty subkey
TEST_F(WRegistryTest, EmptySubKey) {
    std::vector<std::string> subKeys;
    bool result = getRegistrySubKeys(HKEY_CURRENT_USER, "", subKeys);

    // This may succeed or fail depending on Windows version, we just verify it
    // doesn't crash
    SUCCEED();
}

// Edge case: test with very long key name
TEST_F(WRegistryTest, VeryLongKeyName) {
    std::string longKey(255, 'A');  // MAX_KEY_LENGTH

    // Create a key with a very long name (may fail, which is fine)
    HKEY hKey;
    LONG lRes = RegCreateKeyEx(
        HKEY_CURRENT_USER, (test_key + "\\" + longKey).c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);

    if (lRes == ERROR_SUCCESS) {
        RegCloseKey(hKey);

        // Now try to query it
        std::vector<std::string> subKeys;
        EXPECT_NO_THROW(
            getRegistrySubKeys(HKEY_CURRENT_USER, test_key, subKeys));
    }

    // We just verify it doesn't crash
    SUCCEED();
}

// Test that non-Windows platforms are properly skipped
#else

// Dummy test for non-Windows platforms
TEST(WRegistryTest, NonWindowsPlatform) {
    // This test should be skipped on non-Windows platforms
    SUCCEED();
}

#endif  // _WIN32

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
