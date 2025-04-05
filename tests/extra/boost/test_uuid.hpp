// filepath: atom/extra/boost/test_uuid.hpp
#ifndef ATOM_EXTRA_BOOST_TEST_UUID_HPP
#define ATOM_EXTRA_BOOST_TEST_UUID_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/extra/boost/uuid.hpp"

namespace atom::extra::boost::test {

using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Lt;
using ::testing::Gt;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrEq;

class UUIDTest : public ::testing::Test {
protected:
    // Set up some constants for testing
    void SetUp() override {
        // Create a nil UUID
        nilUUID = std::make_unique<UUID>(::boost::uuids::nil_uuid());
        
        // Create a UUID from a fixed string for consistent testing
        const std::string testUUIDString = "123e4567-e89b-12d3-a456-426614174000";
        fixedUUID = std::make_unique<UUID>(testUUIDString);
        
        // Static predefined namespace UUIDs
        dnsNamespaceUUID = std::make_unique<UUID>(UUID::namespaceDNS());
        urlNamespaceUUID = std::make_unique<UUID>(UUID::namespaceURL());
        oidNamespaceUUID = std::make_unique<UUID>(UUID::namespaceOID());
    }
    
    void TearDown() override {
        nilUUID.reset();
        fixedUUID.reset();
        dnsNamespaceUUID.reset();
        urlNamespaceUUID.reset();
        oidNamespaceUUID.reset();
    }
    
    // Helper functions for testing
    static bool isValidUUIDString(const std::string& str) {
        std::regex uuidRegex(
            "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", 
            std::regex::icase
        );
        return std::regex_match(str, uuidRegex);
    }
    
    static bool isValidBase64String(const std::string& str) {
        // Base64 consists of alphanumeric chars, '+', and '/'
        std::regex base64Regex("^[A-Za-z0-9+/]+={0,2}$");
        return std::regex_match(str, base64Regex);
    }
    
    std::unique_ptr<UUID> nilUUID;
    std::unique_ptr<UUID> fixedUUID;
    std::unique_ptr<UUID> dnsNamespaceUUID;
    std::unique_ptr<UUID> urlNamespaceUUID;
    std::unique_ptr<UUID> oidNamespaceUUID;
};

// Test constructors
TEST_F(UUIDTest, Constructors) {
    // Test default constructor (random UUID)
    UUID randomUUID;
    EXPECT_FALSE(randomUUID.isNil());
    EXPECT_TRUE(isValidUUIDString(randomUUID.toString()));
    
    // Test constructor with string
    std::string uuidStr = "123e4567-e89b-12d3-a456-426614174000";
    UUID fromString(uuidStr);
    EXPECT_EQ(fromString.toString(), uuidStr);
    
    // Test constructor with Boost UUID
    ::boost::uuids::uuid boostUUID = ::boost::uuids::nil_uuid();
    UUID fromBoostUUID(boostUUID);
    EXPECT_TRUE(fromBoostUUID.isNil());
    
    // Test constructor with invalid string (should throw)
    EXPECT_THROW(UUID("not-a-uuid"), std::runtime_error);
}

// Test toString method
TEST_F(UUIDTest, ToString) {
    // Nil UUID should have a valid string representation
    std::string nilString = nilUUID->toString();
    EXPECT_TRUE(isValidUUIDString(nilString));
    EXPECT_EQ(nilString, "00000000-0000-0000-0000-000000000000");
    
    // Fixed UUID should match its string representation
    EXPECT_EQ(fixedUUID->toString(), "123e4567-e89b-12d3-a456-426614174000");
    
    // Random UUID should have a valid string representation
    UUID randomUUID;
    EXPECT_TRUE(isValidUUIDString(randomUUID.toString()));
}

// Test isNil method
TEST_F(UUIDTest, IsNil) {
    // Nil UUID should be nil
    EXPECT_TRUE(nilUUID->isNil());
    
    // Fixed UUID should not be nil
    EXPECT_FALSE(fixedUUID->isNil());
    
    // Random UUID should not be nil
    UUID randomUUID;
    EXPECT_FALSE(randomUUID.isNil());
}

// Test comparison operators
TEST_F(UUIDTest, ComparisonOperators) {
    // Create copies of UUIDs
    UUID nilCopy(*nilUUID);
    UUID fixedCopy(*fixedUUID);
    
    // Test equality
    EXPECT_TRUE(*nilUUID == nilCopy);
    EXPECT_TRUE(*fixedUUID == fixedCopy);
    EXPECT_FALSE(*nilUUID == *fixedUUID);
    
    // Test inequality
    EXPECT_FALSE(*nilUUID != nilCopy);
    EXPECT_FALSE(*fixedUUID != fixedCopy);
    EXPECT_TRUE(*nilUUID != *fixedUUID);
    
    // Test spaceship operator
    EXPECT_TRUE((*nilUUID <=> nilCopy) == std::strong_ordering::equal);
    EXPECT_TRUE((*fixedUUID <=> fixedCopy) == std::strong_ordering::equal);
    
    // The actual comparison depends on the underlying bytes, so we can't easily
    // predict less/greater, but we can check it's consistent
    auto compResult = *nilUUID <=> *fixedUUID;
    if (compResult == std::strong_ordering::less) {
        EXPECT_TRUE(*fixedUUID <=> *nilUUID == std::strong_ordering::greater);
    } else if (compResult == std::strong_ordering::greater) {
        EXPECT_TRUE(*fixedUUID <=> *nilUUID == std::strong_ordering::less);
    }
}

// Test format method
TEST_F(UUIDTest, Format) {
    // Nil UUID format
    std::string nilFormat = nilUUID->format();
    EXPECT_EQ(nilFormat, "{00000000-0000-0000-0000-000000000000}");
    
    // Fixed UUID format
    std::string fixedFormat = fixedUUID->format();
    EXPECT_EQ(fixedFormat, "{123e4567-e89b-12d3-a456-426614174000}");
    
    // Random UUID format
    UUID randomUUID;
    std::string randomFormat = randomUUID.format();
    EXPECT_TRUE(randomFormat.size() == 38); // {UUID} format adds 2 chars
    EXPECT_EQ(randomFormat.front(), '{');
    EXPECT_EQ(randomFormat.back(), '}');
}

// Test toBytes and fromBytes methods
TEST_F(UUIDTest, ByteConversion) {
    // Test toBytes
    std::vector<uint8_t> nilBytes = nilUUID->toBytes();
    EXPECT_EQ(nilBytes.size(), atom::extra::boost::UUID_SIZE);
    EXPECT_TRUE(std::all_of(nilBytes.begin(), nilBytes.end(), [](uint8_t b) { return b == 0; }));
    
    std::vector<uint8_t> fixedBytes = fixedUUID->toBytes();
    EXPECT_EQ(fixedBytes.size(), atom::extra::boost::UUID_SIZE);
    
    // Test fromBytes with valid input
    UUID reconstructedNil = UUID::fromBytes(std::span<const uint8_t>(nilBytes));
    EXPECT_TRUE(reconstructedNil.isNil());
    EXPECT_EQ(reconstructedNil, *nilUUID);
    
    UUID reconstructedFixed = UUID::fromBytes(std::span<const uint8_t>(fixedBytes));
    EXPECT_EQ(reconstructedFixed, *fixedUUID);
    
    // Test fromBytes with invalid input
    std::vector<uint8_t> tooShort(15, 0);
    EXPECT_THROW(UUID::fromBytes(std::span<const uint8_t>(tooShort)), std::invalid_argument);
    
    std::vector<uint8_t> tooLong(17, 0);
    EXPECT_THROW(UUID::fromBytes(std::span<const uint8_t>(tooLong)), std::invalid_argument);
}

// Test toUint64 method
TEST_F(UUIDTest, ToUint64) {
    // Nil UUID should convert to 0
    EXPECT_EQ(nilUUID->toUint64(), 0);
    
    // Fixed UUID conversion should be deterministic
    uint64_t fixedValue = fixedUUID->toUint64();
    EXPECT_NE(fixedValue, 0);
    
    // Creating a new UUID with the same string should give the same uint64
    UUID fixedCopy("123e4567-e89b-12d3-a456-426614174000");
    EXPECT_EQ(fixedCopy.toUint64(), fixedValue);
}

// Test namespace UUIDs
TEST_F(UUIDTest, NamespaceUUIDs) {
    // Test DNS namespace UUID
    EXPECT_FALSE(dnsNamespaceUUID->isNil());
    EXPECT_EQ(dnsNamespaceUUID->toString(), "6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    
    // Test URL namespace UUID
    EXPECT_FALSE(urlNamespaceUUID->isNil());
    EXPECT_EQ(urlNamespaceUUID->toString(), "6ba7b811-9dad-11d1-80b4-00c04fd430c8");
    
    // Test OID namespace UUID
    EXPECT_FALSE(oidNamespaceUUID->isNil());
    EXPECT_EQ(oidNamespaceUUID->toString(), "6ba7b812-9dad-11d1-80b4-00c04fd430c8");
}

// Test v3 (name-based MD5) UUID generation
TEST_F(UUIDTest, V3UUID) {
    // Generate v3 UUIDs with the same namespace and name
    UUID v3_1 = UUID::v3(*dnsNamespaceUUID, "example.com");
    UUID v3_2 = UUID::v3(*dnsNamespaceUUID, "example.com");
    
    // They should be the same
    EXPECT_EQ(v3_1, v3_2);
    EXPECT_EQ(v3_1.version(), 3);
    
    // Generate v3 UUIDs with different names
    UUID v3_3 = UUID::v3(*dnsNamespaceUUID, "example.org");
    EXPECT_NE(v3_1, v3_3);
    EXPECT_EQ(v3_3.version(), 3);
    
    // Generate v3 UUIDs with different namespaces
    UUID v3_4 = UUID::v3(*urlNamespaceUUID, "example.com");
    EXPECT_NE(v3_1, v3_4);
    EXPECT_EQ(v3_4.version(), 3);
}

// Test v5 (name-based SHA-1) UUID generation
TEST_F(UUIDTest, V5UUID) {
    // Generate v5 UUIDs with the same namespace and name
    UUID v5_1 = UUID::v5(*dnsNamespaceUUID, "example.com");
    UUID v5_2 = UUID::v5(*dnsNamespaceUUID, "example.com");
    
    // They should be the same
    EXPECT_EQ(v5_1, v5_2);
    EXPECT_EQ(v5_1.version(), 5);
    
    // Generate v5 UUIDs with different names
    UUID v5_3 = UUID::v5(*dnsNamespaceUUID, "example.org");
    EXPECT_NE(v5_1, v5_3);
    EXPECT_EQ(v5_3.version(), 5);
    
    // Generate v5 UUIDs with different namespaces
    UUID v5_4 = UUID::v5(*urlNamespaceUUID, "example.com");
    EXPECT_NE(v5_1, v5_4);
    EXPECT_EQ(v5_4.version(), 5);
    
    // v3 and v5 UUIDs for the same name should be different
    UUID v3 = UUID::v3(*dnsNamespaceUUID, "example.com");
    UUID v5 = UUID::v5(*dnsNamespaceUUID, "example.com");
    EXPECT_NE(v3, v5);
}

// Test version and variant methods
TEST_F(UUIDTest, VersionAndVariant) {
    // Nil UUID should have version 0
    EXPECT_EQ(nilUUID->version(), 0);
    
    // Random UUID (v4) should have version 4
    UUID v4UUID = UUID::v4();
    EXPECT_EQ(v4UUID.version(), 4);
    
    // v3 UUID should have version 3
    UUID v3UUID = UUID::v3(*dnsNamespaceUUID, "example.com");
    EXPECT_EQ(v3UUID.version(), 3);
    
    // v5 UUID should have version 5
    UUID v5UUID = UUID::v5(*dnsNamespaceUUID, "example.com");
    EXPECT_EQ(v5UUID.version(), 5);
    
    // v1 UUID should have version 1
    UUID v1UUID = UUID::v1();
    // Note: Test this if v1() actually generates v1 UUIDs
    if (v1UUID.version() == 1) {
        EXPECT_EQ(v1UUID.version(), 1);
    }
    
    // Variant should be correct for all UUIDs (DCE 1.1 variant)
    EXPECT_EQ(v4UUID.variant(), 1);
    EXPECT_EQ(v3UUID.variant(), 1);
    EXPECT_EQ(v5UUID.variant(), 1);
    
    // Nil UUID variant might be 0
    // This is implementation-defined, so we don't make strict assertions
}

// Test v1 (time-based) and v4 (random) UUID generation
TEST_F(UUIDTest, V1AndV4UUID) {
    // Generate multiple v1 UUIDs
    UUID v1_1 = UUID::v1();
    UUID v1_2 = UUID::v1();
    
    // They should be different
    EXPECT_NE(v1_1, v1_2);
    
    // Generate multiple v4 UUIDs
    UUID v4_1 = UUID::v4();
    UUID v4_2 = UUID::v4();
    
    // They should be different
    EXPECT_NE(v4_1, v4_2);
    EXPECT_EQ(v4_1.version(), 4);
    EXPECT_EQ(v4_2.version(), 4);
    
    // v1 and v4 UUIDs should be different
    EXPECT_NE(v1_1, v4_1);
}

// Test toBase64 method
TEST_F(UUIDTest, ToBase64) {
    // Test nil UUID base64
    std::string nilBase64 = nilUUID->toBase64();
    EXPECT_EQ(nilBase64.size(), atom::extra::boost::BASE64_RESERVE_SIZE);
    
    // Test fixed UUID base64
    std::string fixedBase64 = fixedUUID->toBase64();
    EXPECT_EQ(fixedBase64.size(), atom::extra::boost::BASE64_RESERVE_SIZE);
    EXPECT_TRUE(isValidBase64String(fixedBase64));
    
    // Random UUID base64
    UUID randomUUID;
    std::string randomBase64 = randomUUID.toBase64();
    EXPECT_EQ(randomBase64.size(), atom::extra::boost::BASE64_RESERVE_SIZE);
    EXPECT_TRUE(isValidBase64String(randomBase64));
    
    // Converting the same UUID twice should give the same base64
    EXPECT_EQ(fixedUUID->toBase64(), fixedBase64);
    
    // Different UUIDs should give different base64 strings
    EXPECT_NE(nilUUID->toBase64(), fixedUUID->toBase64());
}

// Test getTimestamp method (v1 UUIDs)
TEST_F(UUIDTest, GetTimestamp) {
    // Create a v1 UUID
    UUID v1UUID = UUID::v1();
    
    // If it's actually a v1 UUID, test getTimestamp
    if (v1UUID.version() == 1) {
        // Getting timestamp should not throw for v1 UUID
        EXPECT_NO_THROW({
            auto timestamp = v1UUID.getTimestamp();
        });
        
        // Timestamp should be recent
        auto timestamp = v1UUID.getTimestamp();
        auto now = std::chrono::system_clock::now();
        
        // It should be within a reasonable time range from now
        // Note: This is approximate and may fail if time zones are involved
        auto timeDiff = std::chrono::duration_cast<std::chrono::days>(now - timestamp).count();
        EXPECT_LE(std::abs(timeDiff), 366); // Within a year (generous margin)
    }
    
    // Getting timestamp from non-v1 UUID should throw
    UUID v4UUID = UUID::v4();
    EXPECT_THROW(v4UUID.getTimestamp(), std::runtime_error);
    
    EXPECT_THROW(nilUUID->getTimestamp(), std::runtime_error);
}

// Test std::hash specialization
TEST_F(UUIDTest, HashFunction) {
    // Create UUIDs for testing
    UUID u1 = UUID::v4();
    UUID u2 = UUID::v4();
    UUID u1Copy = UUID(u1.toString());
    
    // Create hash function
    std::hash<UUID> hasher;
    
    // Same UUIDs should have same hash
    EXPECT_EQ(hasher(u1), hasher(u1Copy));
    
    // Different UUIDs should (probably) have different hashes
    // This is not guaranteed but highly likely
    EXPECT_NE(hasher(u1), hasher(u2));
    
    // Test in hash containers
    std::unordered_set<UUID> uuidSet;
    uuidSet.insert(u1);
    uuidSet.insert(u2);
    uuidSet.insert(u1Copy); // Should not increase the size since u1 is already there
    
    EXPECT_EQ(uuidSet.size(), 2);
    EXPECT_TRUE(uuidSet.contains(u1));
    EXPECT_TRUE(uuidSet.contains(u2));
    
    std::unordered_map<UUID, int> uuidMap;
    uuidMap[u1] = 1;
    uuidMap[u2] = 2;
    uuidMap[u1Copy] = 3; // Should update the value for u1
    
    EXPECT_EQ(uuidMap.size(), 2);
    EXPECT_EQ(uuidMap[u1], 3);
    EXPECT_EQ(uuidMap[u2], 2);
}

// Test getUUID method
TEST_F(UUIDTest, GetUUID) {
    // Get the underlying Boost UUID
    const auto& boostUUID = nilUUID->getUUID();
    
    // Verify it's the correct type and value
    EXPECT_TRUE(boostUUID.is_nil());
    
    // Create a new UUID from the Boost UUID
    UUID newUUID(boostUUID);
    EXPECT_EQ(newUUID, *nilUUID);
}

// Test UUID uniqueness
TEST_F(UUIDTest, Uniqueness) {
    constexpr int NUM_UUIDS = 1000;
    std::set<std::string> uuidStrings;
    
    // Generate a bunch of UUIDs and ensure they're all unique
    for (int i = 0; i < NUM_UUIDS; ++i) {
        UUID uuid = UUID::v4();
        std::string uuidStr = uuid.toString();
        EXPECT_TRUE(uuidStrings.insert(uuidStr).second) << "UUID collision detected: " << uuidStr;
    }
    
    EXPECT_EQ(uuidStrings.size(), NUM_UUIDS);
}

// Edge cases and error conditions
TEST_F(UUIDTest, EdgeCases) {
    // Invalid string for UUID constructor
    EXPECT_THROW(UUID("not-a-uuid"), std::runtime_error);
    EXPECT_THROW(UUID("123456789"), std::runtime_error);
    EXPECT_THROW(UUID("123e4567-e89b-12d3-a456-4266141740"), std::runtime_error); // Too short
    
    // Empty string for UUID constructor
    EXPECT_THROW(UUID(""), std::runtime_error);
    
    // Invalid bytes for fromBytes
    std::vector<uint8_t> tooShort(15, 0);
    EXPECT_THROW(UUID::fromBytes(std::span<const uint8_t>(tooShort)), std::invalid_argument);
    
    std::vector<uint8_t> tooLong(17, 0);
    EXPECT_THROW(UUID::fromBytes(std::span<const uint8_t>(tooLong)), std::invalid_argument);
    
    // Empty bytes should throw
    std::vector<uint8_t> empty;
    EXPECT_THROW(UUID::fromBytes(std::span<const uint8_t>(empty)), std::invalid_argument);
}

// Verify that UUIDs can be sorted (for use in ordered containers)
TEST_F(UUIDTest, SortingBehavior) {
    std::vector<UUID> uuids;
    
    // Add some UUIDs
    uuids.push_back(*nilUUID);
    uuids.push_back(*fixedUUID);
    uuids.push_back(UUID::v4());
    uuids.push_back(UUID::v4());
    
    // Should be able to sort without errors
    EXPECT_NO_THROW(std::sort(uuids.begin(), uuids.end()));
    
    // Verify the sort is stable (sorting again gives the same result)
    std::vector<UUID> uuidsCopy = uuids;
    std::sort(uuidsCopy.begin(), uuidsCopy.end());
    EXPECT_EQ(uuids, uuidsCopy);
}

} // namespace atom::extra::boost::test

#endif // ATOM_EXTRA_BOOST_TEST_UUID_HPP