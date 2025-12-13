/**
 * @file uuid_example.cpp
 * @brief Examples for atom::utils UUID class
 */

#include "atom/utils/random/uuid.hpp"
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicUUID() {
    printSection("1. Basic UUID Generation");

    std::cout << "Generating random UUIDs:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        UUID uuid;
        std::cout << "  " << uuid.toString() << std::endl;
    }
}

void demonstrateUUIDVersions() {
    printSection("2. UUID Versions");

    std::cout << "--- Version 1 (Time-based) ---" << std::endl;
    UUID v1 = UUID::generateV1();
    std::cout << "  UUID: " << v1.toString() << std::endl;
    std::cout << "  Version: " << static_cast<int>(v1.version()) << std::endl;

    std::cout << "\n--- Version 4 (Random) ---" << std::endl;
    UUID v4 = UUID::generateV4();
    std::cout << "  UUID: " << v4.toString() << std::endl;
    std::cout << "  Version: " << static_cast<int>(v4.version()) << std::endl;

    std::cout << "\n--- Version 3 (MD5 Name-based) ---" << std::endl;
    UUID namespace_uuid;
    UUID v3 = UUID::generateV3(namespace_uuid, "example.com");
    std::cout << "  UUID: " << v3.toString() << std::endl;
    std::cout << "  Version: " << static_cast<int>(v3.version()) << std::endl;

    std::cout << "\n--- Version 5 (SHA-1 Name-based) ---" << std::endl;
    UUID v5 = UUID::generateV5(namespace_uuid, "example.com");
    std::cout << "  UUID: " << v5.toString() << std::endl;
    std::cout << "  Version: " << static_cast<int>(v5.version()) << std::endl;
}

void demonstrateUUIDParsing() {
    printSection("3. UUID Parsing");

    std::vector<std::string> testStrings = {
        "550e8400-e29b-41d4-a716-446655440000",
        "550E8400-E29B-41D4-A716-446655440000",
        "invalid-uuid-string",
        "550e8400e29b41d4a716446655440000",
        ""
    };

    for (const auto& str : testStrings) {
        std::cout << "Parsing: \"" << str << "\"" << std::endl;
        auto result = UUID::fromString(str);
        if (result.has_value()) {
            std::cout << "  Success: " << result.value().toString() << std::endl;
        } else {
            std::cout << "  Failed: Invalid UUID format" << std::endl;
        }
    }
}

void demonstrateUUIDValidation() {
    printSection("4. UUID Validation");

    std::vector<std::string> testStrings = {
        "550e8400-e29b-41d4-a716-446655440000",
        "not-a-uuid",
        "550e8400-e29b-41d4-a716",
        "550e8400-e29b-41d4-a716-4466554400001",
        "gggggggg-gggg-gggg-gggg-gggggggggggg"
    };

    for (const auto& str : testStrings) {
        bool valid = UUID::isValidUUID(str);
        std::cout << "  \"" << str << "\": "
                  << (valid ? "Valid" : "Invalid") << std::endl;
    }
}

void demonstrateUUIDComparison() {
    printSection("5. UUID Comparison");

    UUID uuid1 = UUID::generateV4();
    UUID uuid2 = UUID::generateV4();
    UUID uuid3 = uuid1;

    std::cout << "UUID 1: " << uuid1.toString() << std::endl;
    std::cout << "UUID 2: " << uuid2.toString() << std::endl;
    std::cout << "UUID 3: (copy of UUID 1)" << std::endl;

    std::cout << "\nComparisons:" << std::endl;
    std::cout << "  UUID 1 == UUID 2: " << (uuid1 == uuid2 ? "true" : "false") << std::endl;
    std::cout << "  UUID 1 == UUID 3: " << (uuid1 == uuid3 ? "true" : "false") << std::endl;
    std::cout << "  UUID 1 != UUID 2: " << (uuid1 != uuid2 ? "true" : "false") << std::endl;
    std::cout << "  UUID 1 < UUID 2: " << (uuid1 < uuid2 ? "true" : "false") << std::endl;
}

void demonstrateUUIDUniqueness() {
    printSection("6. UUID Uniqueness Test");

    const int count = 10000;
    std::set<std::string> uuids;

    std::cout << "Generating " << count << " UUIDs..." << std::endl;

    for (int i = 0; i < count; ++i) {
        UUID uuid = UUID::generateV4();
        uuids.insert(uuid.toString());
    }

    std::cout << "  Generated: " << count << std::endl;
    std::cout << "  Unique: " << uuids.size() << std::endl;
    std::cout << "  Duplicates: " << (count - uuids.size()) << std::endl;
    std::cout << "  All unique: " << (uuids.size() == count ? "Yes" : "No") << std::endl;
}

void demonstrateNameBasedUUID() {
    printSection("7. Name-Based UUID Consistency");

    UUID namespace_uuid;
    std::string name = "user@example.com";

    std::cout << "Namespace: " << namespace_uuid.toString() << std::endl;
    std::cout << "Name: " << name << std::endl;

    std::cout << "\nGenerating V5 UUID multiple times:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        UUID uuid = UUID::generateV5(namespace_uuid, name);
        std::cout << "  " << uuid.toString() << std::endl;
    }
    std::cout << "(Same name + namespace = same UUID)" << std::endl;

    std::cout << "\nDifferent names:" << std::endl;
    std::vector<std::string> names = {"alice@example.com", "bob@example.com", "charlie@example.com"};
    for (const auto& n : names) {
        UUID uuid = UUID::generateV5(namespace_uuid, n);
        std::cout << "  " << n << " -> " << uuid.toString() << std::endl;
    }
}

void demonstrateUUIDData() {
    printSection("8. UUID Internal Data");

    UUID uuid = UUID::generateV4();
    std::cout << "UUID: " << uuid.toString() << std::endl;

    const auto& data = uuid.getData();
    std::cout << "\nRaw bytes (hex):" << std::endl;
    std::cout << "  ";
    for (size_t i = 0; i < data.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) std::cout << "-";
    }
    std::cout << std::dec << std::endl;

    std::cout << "\nVersion: " << static_cast<int>(uuid.version()) << std::endl;
    std::cout << "Variant: " << static_cast<int>(uuid.variant()) << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  UUID Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicUUID();
        demonstrateUUIDVersions();
        demonstrateUUIDParsing();
        demonstrateUUIDValidation();
        demonstrateUUIDComparison();
        demonstrateUUIDUniqueness();
        demonstrateNameBasedUUID();
        demonstrateUUIDData();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All UUID examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
