#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>
#include "atom/algorithm/utils/uuid.hpp"

using namespace atom::algorithm;

class UUIDTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

TEST_F(UUIDTest, DefaultConstructor) {
    UUID uuid;
    EXPECT_TRUE(uuid.isNil());
    // Nil UUID has version 0 (all bytes are 0), which doesn't match any Version
    // enum value We just verify it's nil, version check is not meaningful for
    // nil UUIDs
    EXPECT_EQ(uuid.toString(), "00000000-0000-0000-0000-000000000000");
}

TEST_F(UUIDTest, GenerateRandom) {
    UUID uuid1 = UUID::generateRandom();
    UUID uuid2 = UUID::generateRandom();

    EXPECT_FALSE(uuid1.isNil());
    EXPECT_FALSE(uuid2.isNil());
    EXPECT_NE(uuid1, uuid2);
    EXPECT_EQ(uuid1.getVersion(), UUID::Version::RANDOM);
    EXPECT_EQ(uuid2.getVersion(), UUID::Version::RANDOM);

    // Test string format
    std::string uuid_str = uuid1.toString();
    std::regex uuid_pattern(
        R"(^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)");
    EXPECT_TRUE(std::regex_match(uuid_str, uuid_pattern))
        << "UUID string: " << uuid_str;
}

TEST_F(UUIDTest, GenerateNil) {
    UUID nil_uuid = UUID::generateNil();
    UUID default_uuid;

    EXPECT_TRUE(nil_uuid.isNil());
    EXPECT_EQ(nil_uuid, default_uuid);
    EXPECT_EQ(nil_uuid.toString(), "00000000-0000-0000-0000-000000000000");
}

TEST_F(UUIDTest, GenerateTimeBased) {
    auto node_id = UUID::generateRandomNodeId();
    UUID uuid1 = UUID::generateTimeBased(node_id);
    UUID uuid2 = UUID::generateTimeBased(node_id);

    EXPECT_FALSE(uuid1.isNil());
    EXPECT_FALSE(uuid2.isNil());
    EXPECT_EQ(uuid1.getVersion(), UUID::Version::TIME_BASED);
    EXPECT_EQ(uuid2.getVersion(), UUID::Version::TIME_BASED);

    // Time-based UUIDs should be different even with same node ID
    EXPECT_NE(uuid1, uuid2);

    // Test string format for version 1
    std::string uuid_str = uuid1.toString();
    std::regex uuid_pattern(
        R"(^[0-9a-f]{8}-[0-9a-f]{4}-1[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)");
    EXPECT_TRUE(std::regex_match(uuid_str, uuid_pattern))
        << "Time-based UUID string: " << uuid_str;
}

TEST_F(UUIDTest, GenerateRandomNodeId) {
    auto node_id1 = UUID::generateRandomNodeId();
    auto node_id2 = UUID::generateRandomNodeId();

    EXPECT_NE(node_id1, node_id2);

    // Test that multicast bit is set (first bit of first byte is 1)
    EXPECT_TRUE((node_id1[0] & 0x01) != 0);
    EXPECT_TRUE((node_id2[0] & 0x01) != 0);
}

TEST_F(UUIDTest, ConstructFromData) {
    UUID::Data data = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
                       0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    UUID uuid(data);

    EXPECT_FALSE(uuid.isNil());
    EXPECT_EQ(uuid.getData(), data);
    EXPECT_EQ(uuid.toString(), "12345678-9abc-def0-fedc-ba9876543210");
}

TEST_F(UUIDTest, ConstructFromString) {
    std::string uuid_str = "550e8400-e29b-41d4-a716-446655440000";
    UUID uuid(uuid_str);

    EXPECT_FALSE(uuid.isNil());
    EXPECT_EQ(uuid.toString(), uuid_str);
    EXPECT_EQ(uuid.getVersion(), UUID::Version::RANDOM);
}

TEST_F(UUIDTest, ConstructFromInvalidString) {
    // Test various invalid formats
    std::vector<std::string> invalid_strings = {
        "",                                  // Empty
        "short",                             // Too short
        "not-a-uuid-format",                 // Invalid characters
        "550e8400e29b41d4a716446655440000",  // Missing hyphens
        "550e8400-e29b-41d4-a716-44665544",  // Too short (missing 4 digits)
        "550e8400-e29b-41d4-a716-4466554400000",  // Too long
        "550e8400-e29b-41d4-a716-44665544zzzz",   // Invalid hex characters
        "550e8400-e29b-41d4-a716-44665544000-"    // Trailing hyphen
    };

    for (const auto& invalid_str : invalid_strings) {
        UUID uuid(invalid_str);
        EXPECT_TRUE(uuid.isNil())
            << "Should create nil UUID from invalid string: " << invalid_str;
        EXPECT_EQ(uuid.toString(), "00000000-0000-0000-0000-000000000000");
    }
}

TEST_F(UUIDTest, FromStringMethod) {
    UUID uuid;
    std::string valid_uuid = "550e8400-e29b-41d4-a716-446655440000";

    EXPECT_TRUE(uuid.fromString(valid_uuid));
    EXPECT_EQ(uuid.toString(), valid_uuid);

    EXPECT_FALSE(uuid.fromString("invalid-uuid"));
    EXPECT_TRUE(uuid.isNil());
}

TEST_F(UUIDTest, ComparisonOperators) {
    UUID uuid1("550e8400-e29b-41d4-a716-446655440000");
    UUID uuid2("550e8400-e29b-41d4-a716-446655440001");
    UUID uuid3("550e8400-e29b-41d4-a716-446655440000");

    EXPECT_EQ(uuid1, uuid3);
    EXPECT_NE(uuid1, uuid2);
    EXPECT_LT(uuid1, uuid2);
    EXPECT_FALSE(uuid2 < uuid1);
    EXPECT_FALSE(uuid1 < uuid3);
}

TEST_F(UUIDTest, StreamOutput) {
    UUID uuid("550e8400-e29b-41d4-a716-446655440000");
    std::ostringstream oss;
    oss << uuid;

    EXPECT_EQ(oss.str(), "550e8400-e29b-41d4-a716-446655440000");
}

TEST_F(UUIDTest, Uniqueness) {
    // Generate many UUIDs and check for uniqueness
    const size_t num_uuids = 10000;
    std::unordered_set<UUID> uuid_set;

    for (size_t i = 0; i < num_uuids; ++i) {
        UUID uuid = UUID::generateRandom();
        EXPECT_TRUE(uuid_set.insert(uuid).second)
            << "Duplicate UUID found at iteration " << i;
    }

    EXPECT_EQ(uuid_set.size(), num_uuids);
}

TEST_F(UUIDTest, TimeBasedUniqueness) {
    auto node_id = UUID::generateRandomNodeId();

    // Generate time-based UUIDs rapidly and check they're different
    const size_t num_uuids = 100;
    std::unordered_set<UUID> uuid_set;

    for (size_t i = 0; i < num_uuids; ++i) {
        UUID uuid = UUID::generateTimeBased(node_id);
        EXPECT_TRUE(uuid_set.insert(uuid).second)
            << "Duplicate time-based UUID found at iteration " << i;
        EXPECT_EQ(uuid.getVersion(), UUID::Version::TIME_BASED);

        // Small delay to ensure timestamp changes
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    EXPECT_EQ(uuid_set.size(), num_uuids);
}

TEST_F(UUIDTest, VersionDetection) {
    UUID nil_uuid = UUID::generateNil();
    UUID random_uuid = UUID::generateRandom();

    auto node_id = UUID::generateRandomNodeId();
    UUID time_uuid = UUID::generateTimeBased(node_id);

    EXPECT_EQ(random_uuid.getVersion(), UUID::Version::RANDOM);
    EXPECT_EQ(time_uuid.getVersion(), UUID::Version::TIME_BASED);

    // Test parsing versions from string
    UUID v4_uuid("550e8400-e29b-41d4-a716-446655440000");  // Version 4
    EXPECT_EQ(v4_uuid.getVersion(), UUID::Version::RANDOM);

    UUID v1_uuid("550e8400-e29b-11d4-a716-446655440000");  // Version 1
    EXPECT_EQ(v1_uuid.getVersion(), UUID::Version::TIME_BASED);
}

TEST_F(UUIDTest, HashFunctionality) {
    // Test that UUID can be used as a key in unordered containers
    std::unordered_map<UUID, std::string> uuid_map;

    UUID uuid1 = UUID::generateRandom();
    UUID uuid2 = UUID::generateRandom();

    uuid_map[uuid1] = "first";
    uuid_map[uuid2] = "second";

    EXPECT_EQ(uuid_map[uuid1], "first");
    EXPECT_EQ(uuid_map[uuid2], "second");
    EXPECT_EQ(uuid_map.size(), 2);

    // Test with unordered_set
    std::unordered_set<UUID> uuid_set;
    uuid_set.insert(uuid1);
    uuid_set.insert(uuid2);

    EXPECT_TRUE(uuid_set.find(uuid1) != uuid_set.end());
    EXPECT_TRUE(uuid_set.find(uuid2) != uuid_set.end());
    EXPECT_EQ(uuid_set.size(), 2);
}

TEST_F(UUIDTest, Performance) {
    const size_t num_uuids = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<UUID> uuids;
    uuids.reserve(num_uuids);

    for (size_t i = 0; i < num_uuids; ++i) {
        uuids.push_back(UUID::generateRandom());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Generated {} UUIDs in {} ms", num_uuids, duration.count());

    // Verify uniqueness
    std::unordered_set<UUID> unique_uuids(uuids.begin(), uuids.end());
    EXPECT_EQ(unique_uuids.size(), num_uuids);
}

TEST_F(UUIDTest, EdgeCases) {
    // Test UUID with all possible versions
    UUID::Data data = {};

    // Version 0 (nil)
    UUID nil_uuid(data);
    EXPECT_TRUE(nil_uuid.isNil());

    // Version 1 - version goes in upper nibble of byte 6
    data[6] = 0x10;
    UUID v1_uuid(data);
    EXPECT_EQ(v1_uuid.getVersion(), UUID::Version::TIME_BASED);

    // Version 4 - version goes in upper nibble of byte 6
    data[6] = 0x40;
    UUID v4_uuid(data);
    EXPECT_EQ(v4_uuid.getVersion(), UUID::Version::RANDOM);

    // Version 5 - version goes in upper nibble of byte 6
    data[6] = 0x50;
    UUID v5_uuid(data);
    EXPECT_EQ(v5_uuid.getVersion(), UUID::Version::NAME_SHA1);
}

TEST_F(UUIDTest, RoundTripConsistency) {
    UUID original = UUID::generateRandom();
    std::string uuid_str = original.toString();
    UUID parsed(uuid_str);

    EXPECT_EQ(original, parsed);
    EXPECT_EQ(original.getData(), parsed.getData());
    EXPECT_EQ(original.getVersion(), parsed.getVersion());
}

TEST_F(UUIDTest, ThreadSafety) {
    const int num_threads = 10;
    const int uuids_per_thread = 1000;

    std::vector<std::thread> threads;
    std::vector<std::vector<UUID>> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < uuids_per_thread; ++j) {
                results[i].push_back(UUID::generateRandom());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Check all UUIDs are unique across all threads
    std::unordered_set<UUID> all_uuids;
    for (const auto& thread_results : results) {
        for (const auto& uuid : thread_results) {
            EXPECT_TRUE(all_uuids.insert(uuid).second)
                << "Duplicate UUID found across threads";
        }
    }

    EXPECT_EQ(all_uuids.size(), num_threads * uuids_per_thread);
}
