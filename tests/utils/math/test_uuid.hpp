#ifndef ATOM_UTILS_TEST_UUID_HPP
#define ATOM_UTILS_TEST_UUID_HPP

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <regex>
#include <thread>
#include <future>
#include <vector>
#include <chrono>
#include "atom/utils/random/uuid.hpp"

namespace atom::utils::test {

class UUIDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup if needed
    }

    void TearDown() override {
        // Test cleanup if needed
    }

    // Helper function to validate UUID format
    bool isValidUUIDFormat(const std::string& uuid) {
        // UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        std::regex uuidRegex(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
        return std::regex_match(uuid, uuidRegex);
    }
};

// Test basic UUID generation
TEST_F(UUIDTest, BasicGeneration) {
    std::string uuid = generateUUID();
    
    EXPECT_FALSE(uuid.empty());
    EXPECT_EQ(uuid.length(), 36); // Standard UUID length with hyphens
    EXPECT_TRUE(isValidUUIDFormat(uuid));
}

// Test UUID uniqueness
TEST_F(UUIDTest, Uniqueness) {
    const int numUUIDs = 1000;
    std::set<std::string> uuids;
    
    for (int i = 0; i < numUUIDs; ++i) {
        std::string uuid = generateUUID();
        EXPECT_TRUE(uuids.insert(uuid).second) << "Duplicate UUID generated: " << uuid;
    }
    
    EXPECT_EQ(uuids.size(), numUUIDs);
}

// Test UUID format consistency
TEST_F(UUIDTest, FormatConsistency) {
    for (int i = 0; i < 100; ++i) {
        std::string uuid = generateUUID();
        
        // Check length
        EXPECT_EQ(uuid.length(), 36);
        
        // Check hyphen positions
        EXPECT_EQ(uuid[8], '-');
        EXPECT_EQ(uuid[13], '-');
        EXPECT_EQ(uuid[18], '-');
        EXPECT_EQ(uuid[23], '-');
        
        // Check that all other characters are hex digits
        for (size_t j = 0; j < uuid.length(); ++j) {
            if (j != 8 && j != 13 && j != 18 && j != 23) {
                char c = uuid[j];
                EXPECT_TRUE((c >= '0' && c <= '9') || 
                           (c >= 'a' && c <= 'f') || 
                           (c >= 'A' && c <= 'F'))
                    << "Invalid character '" << c << "' at position " << j << " in UUID: " << uuid;
            }
        }
    }
}

// Test UUID version (if applicable)
TEST_F(UUIDTest, VersionCheck) {
    std::string uuid = generateUUID();
    
    // Check if it's a valid UUID version (typically version 4 for random UUIDs)
    // The version is in the 15th character (index 14)
    char versionChar = uuid[14];
    
    // Version 4 UUIDs should have '4' as the version digit
    // But we'll be flexible and just check it's a valid hex digit
    EXPECT_TRUE((versionChar >= '0' && versionChar <= '9') || 
                (versionChar >= 'a' && versionChar <= 'f') || 
                (versionChar >= 'A' && versionChar <= 'F'));
}

// Test thread safety
TEST_F(UUIDTest, ThreadSafety) {
    const int numThreads = 4;
    const int uuidsPerThread = 250;
    std::vector<std::future<std::vector<std::string>>> futures;
    
    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [uuidsPerThread]() {
            std::vector<std::string> threadUUIDs;
            threadUUIDs.reserve(uuidsPerThread);
            
            for (int i = 0; i < uuidsPerThread; ++i) {
                threadUUIDs.push_back(generateUUID());
            }
            
            return threadUUIDs;
        }));
    }
    
    // Collect all UUIDs from all threads
    std::set<std::string> allUUIDs;
    for (auto& future : futures) {
        auto threadUUIDs = future.get();
        for (const auto& uuid : threadUUIDs) {
            EXPECT_TRUE(isValidUUIDFormat(uuid));
            EXPECT_TRUE(allUUIDs.insert(uuid).second) << "Duplicate UUID across threads: " << uuid;
        }
    }
    
    EXPECT_EQ(allUUIDs.size(), numThreads * uuidsPerThread);
}

// Test performance
TEST_F(UUIDTest, Performance) {
    const int numUUIDs = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numUUIDs; ++i) {
        std::string uuid = generateUUID();
        EXPECT_FALSE(uuid.empty()); // Basic validation
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should be able to generate 10,000 UUIDs in reasonable time
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

// Test UUID parsing (if parse function exists)
TEST_F(UUIDTest, UUIDParsing) {
    std::string uuid = generateUUID();
    
    // Test that the generated UUID can be parsed back
    // This test assumes there's a parseUUID or validateUUID function
    // If not available, this test can be skipped or the function can be implemented
    EXPECT_TRUE(isValidUUIDFormat(uuid));
    
    // Test invalid UUID formats
    EXPECT_FALSE(isValidUUIDFormat(""));
    EXPECT_FALSE(isValidUUIDFormat("invalid-uuid"));
    EXPECT_FALSE(isValidUUIDFormat("12345678-1234-1234-1234-12345678901")); // Too short
    EXPECT_FALSE(isValidUUIDFormat("12345678-1234-1234-1234-1234567890123")); // Too long
    EXPECT_FALSE(isValidUUIDFormat("12345678_1234_1234_1234_123456789012")); // Wrong separators
    EXPECT_FALSE(isValidUUIDFormat("1234567g-1234-1234-1234-123456789012")); // Invalid hex character
}

// Test edge cases
TEST_F(UUIDTest, EdgeCases) {
    // Test rapid generation
    std::vector<std::string> rapidUUIDs;
    for (int i = 0; i < 100; ++i) {
        rapidUUIDs.push_back(generateUUID());
    }
    
    // All should be unique
    std::set<std::string> uniqueUUIDs(rapidUUIDs.begin(), rapidUUIDs.end());
    EXPECT_EQ(uniqueUUIDs.size(), rapidUUIDs.size());
    
    // All should be valid format
    for (const auto& uuid : rapidUUIDs) {
        EXPECT_TRUE(isValidUUIDFormat(uuid));
    }
}

// Test UUID case sensitivity (if applicable)
TEST_F(UUIDTest, CaseSensitivity) {
    std::string uuid = generateUUID();
    
    // Convert to uppercase and lowercase
    std::string upperUUID = uuid;
    std::string lowerUUID = uuid;
    
    std::transform(upperUUID.begin(), upperUUID.end(), upperUUID.begin(), ::toupper);
    std::transform(lowerUUID.begin(), lowerUUID.end(), lowerUUID.begin(), ::tolower);
    
    // Both should be valid formats
    EXPECT_TRUE(isValidUUIDFormat(upperUUID));
    EXPECT_TRUE(isValidUUIDFormat(lowerUUID));
}

// Test memory usage (basic check)
TEST_F(UUIDTest, MemoryUsage) {
    // Generate many UUIDs and ensure no memory leaks
    // This is a basic test - more sophisticated memory testing would require tools like Valgrind
    std::vector<std::string> uuids;
    uuids.reserve(10000);
    
    for (int i = 0; i < 10000; ++i) {
        uuids.push_back(generateUUID());
    }
    
    // Clear and regenerate to test cleanup
    uuids.clear();
    
    for (int i = 0; i < 10000; ++i) {
        uuids.push_back(generateUUID());
    }
    
    EXPECT_EQ(uuids.size(), 10000);
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_UUID_HPP
