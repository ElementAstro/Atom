/*
 * uuid.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating UUID utilities from atom/algorithm/utils/uuid.hpp
 */

#include "atom/algorithm/utils/uuid.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <vector>

using namespace atom::algorithm;

// Demonstrate random UUID generation (Version 4)
void demonstrateRandomUUID() {
    std::cout << "\n=== Random UUID Generation (Version 4) ===\n";

    std::cout << "Generating 10 random UUIDs:\n";
    for (int i = 0; i < 10; ++i) {
        auto uuid = UUID::generateRandom();
        std::cout << "  " << (i + 1) << ". " << uuid.toString() << "\n";
    }

    // Verify uniqueness
    std::set<std::string> uuid_set;
    constexpr int NUM_UUIDS = 10000;

    for (int i = 0; i < NUM_UUIDS; ++i) {
        auto uuid = UUID::generateRandom();
        uuid_set.insert(uuid.toString());
    }

    std::cout << "\nUniqueness test: Generated " << NUM_UUIDS << " UUIDs, "
              << uuid_set.size() << " unique ("
              << (uuid_set.size() == NUM_UUIDS ? "PASSED" : "FAILED") << ")\n";
}

// Demonstrate time-based UUID generation (Version 1)
void demonstrateTimeBasedUUID() {
    std::cout << "\n=== Time-Based UUID Generation (Version 1) ===\n";

    // Use a sample node ID (like MAC address)
    std::array<u8, 6> node_id = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};

    std::cout << "Node ID: ";
    for (auto byte : node_id) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte);
    }
    std::cout << std::dec << "\n\n";

    std::cout << "Generating 5 time-based UUIDs:\n";
    for (int i = 0; i < 5; ++i) {
        auto uuid = UUID::generateTimeBased(node_id);
        std::cout << "  " << (i + 1) << ". " << uuid.toString() << "\n";
    }

    // Show that time-based UUIDs are sequential
    std::cout << "\nTime-based UUIDs are roughly sequential (same node).\n";
}

// Demonstrate nil UUIDvoid demonstrateNilUUID() {
std::cout << "\n=== Nil UUID ===\n";

auto nil_uuid = UUID::generateNil();
std::cout << "Nil UUID: " << nil_uuid.toString() << "\n";
std::cout << "Is nil: " << (nil_uuid.isNil() ? "Yes" : "No") << "\n";

auto random_uuid = UUID::generateRandom();
std::cout << "\nRandom UUID: " << random_uuid.toString() << "\n";
std::cout << "Is nil: " << (random_uuid.isNil() ? "Yes" : "No") << "\n";
}

// Demonstrate UUID parsing from stringvoid demonstrateUUIDParsing() {
std::cout << "\n=== UUID Parsing ===\n";

// Valid UUID strings
std::vector<std::string> valid_uuids = {"550e8400-e29b-41d4-a716-446655440000",
                                        "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
                                        "f47ac10b-58cc-4372-a567-0e02b2c3d479"};

std::cout << "Parsing valid UUID strings:\n";
for (const auto& str : valid_uuids) {
    UUID uuid(str);
    std::cout << "  Input:  " << str << "\n";
    std::cout << "  Parsed: " << uuid.toString() << "\n";
    std::cout << "  Match:  " << (str == uuid.toString() ? "Yes" : "No")
              << "\n\n";
}

// Invalid UUID strings
std::vector<std::string> invalid_uuids = {
    "not-a-uuid", "550e8400-e29b-41d4-a716",  // Too short
    "550e8400-e29b-41d4-a716-446655440000-extra"};

std::cout << "Parsing invalid UUID strings:\n";
for (const auto& str : invalid_uuids) {
    UUID uuid(str);
    std::cout << "  Input:  \"" << str << "\"\n";
    std::cout << "  Result: "
              << (uuid.isNil() ? "Nil (invalid)" : uuid.toString()) << "\n\n";
}
}

// Demonstrate UUID version detectionvoid demonstrateUUIDVersion() {
std::cout << "\n=== UUID Version Detection ===\n";

// Version 4 (random)
auto v4_uuid = UUID::generateRandom();
std::cout << "Random UUID: " << v4_uuid.toString() << "\n";
std::cout << "  Version: " << static_cast<int>(v4_uuid.getVersion()) << "\n";

// Version 1 (time-based)
std::array<u8, 6> node_id = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
auto v1_uuid = UUID::generateTimeBased(node_id);
std::cout << "\nTime-based UUID: " << v1_uuid.toString() << "\n";
std::cout << "  Version: " << static_cast<int>(v1_uuid.getVersion()) << "\n";

// Nil UUID
auto nil_uuid = UUID::generateNil();
std::cout << "\nNil UUID: " << nil_uuid.toString() << "\n";
std::cout << "  Version: " << static_cast<int>(nil_uuid.getVersion()) << "\n";
}

// Demonstrate UUID comparisonvoid demonstrateUUIDComparison() {
std::cout << "\n=== UUID Comparison ===\n";

auto uuid1 = UUID::generateRandom();
auto uuid2 = UUID::generateRandom();
auto uuid3 = uuid1;  // Copy

std::cout << "UUID1: " << uuid1.toString() << "\n";
std::cout << "UUID2: " << uuid2.toString() << "\n";
std::cout << "UUID3: " << uuid3.toString() << " (copy of UUID1)\n\n";

std::cout << "UUID1 == UUID2: " << (uuid1 == uuid2 ? "true" : "false") << "\n";
std::cout << "UUID1 == UUID3: " << (uuid1 == uuid3 ? "true" : "false") << "\n";
std::cout << "UUID1 != UUID2: " << (uuid1 != uuid2 ? "true" : "false") << "\n";

// Sorting
std::vector<UUID> uuids;
for (int i = 0; i < 5; ++i) {
    uuids.push_back(UUID::generateRandom());
}

std::cout << "\nBefore sorting:\n";
for (const auto& uuid : uuids) {
    std::cout << "  " << uuid.toString() << "\n";
}

std::sort(uuids.begin(), uuids.end());

std::cout << "\nAfter sorting:\n";
for (const auto& uuid : uuids) {
    std::cout << "  " << uuid.toString() << "\n";
}
}

// Demonstrate UUID raw data accessvoid demonstrateRawDataAccess() {
std::cout << "\n=== UUID Raw Data Access ===\n";

auto uuid = UUID::generateRandom();
std::cout << "UUID: " << uuid.toString() << "\n";

const auto& data = uuid.getData();
std::cout << "Raw bytes: ";
for (auto byte : data) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(byte) << " ";
}
std::cout << std::dec << "\n";

// Reconstruct from raw data
UUID reconstructed(data);
std::cout << "Reconstructed: " << reconstructed.toString() << "\n";
std::cout << "Match: " << (uuid == reconstructed ? "Yes" : "No") << "\n";
}

// Benchmark UUID generationvoid benchmarkUUIDGeneration() {
std::cout << "\n=== UUID Generation Benchmark ===\n";

constexpr int ITERATIONS = 100000;

// Benchmark random UUID
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < ITERATIONS; ++i) {
    auto uuid = UUID::generateRandom();
    (void)uuid;
}
auto end = std::chrono::high_resolution_clock::now();
auto duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start);

std::cout << "Random UUID generation:\n";
std::cout << "  " << ITERATIONS << " UUIDs in " << duration.count() << " us\n";
std::cout << "  " << (ITERATIONS * 1000000.0 / duration.count())
          << " UUIDs/sec\n";

// Benchmark time-based UUID
std::array<u8, 6> node_id = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};

start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < ITERATIONS; ++i) {
    auto uuid = UUID::generateTimeBased(node_id);
    (void)uuid;
}
end = std::chrono::high_resolution_clock::now();
duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

std::cout << "\nTime-based UUID generation:\n";
std::cout << "  " << ITERATIONS << " UUIDs in " << duration.count() << " us\n";
std::cout << "  " << (ITERATIONS * 1000000.0 / duration.count())
          << " UUIDs/sec\n";

// Benchmark UUID to string
auto uuid = UUID::generateRandom();

start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < ITERATIONS; ++i) {
    auto str = uuid.toString();
    (void)str;
}
end = std::chrono::high_resolution_clock::now();
duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

std::cout << "\nUUID to string:\n";
std::cout << "  " << ITERATIONS << " conversions in " << duration.count()
          << " us\n";
std::cout << "  " << (ITERATIONS * 1000000.0 / duration.count())
          << " conversions/sec\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   UUID Utilities Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateRandomUUID();
        demonstrateTimeBasedUUID();
        demonstrateNilUUID();
        demonstrateUUIDParsing();
        demonstrateUUIDVersion();
        demonstrateUUIDComparison();
        demonstrateRawDataAccess();
        benchmarkUUIDGeneration();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
