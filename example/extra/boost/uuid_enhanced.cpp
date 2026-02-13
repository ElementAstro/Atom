#include "atom/extra/boost/uuid.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <unordered_set>

using namespace atom::extra::boost;

void demonstrateBasicUUIDOperations() {
    std::cout << "=== Basic UUID Operations ===" << std::endl;

    // Generate different types of UUIDs
    auto uuid_v1 = UUID::v1();
    auto uuid_v4 = UUID::v4();
    auto uuid_nil = UUID::nil();

    std::cout << "UUID v1: " << uuid_v1.toString() << " (version: " << uuid_v1.version() << ")" << std::endl;
    std::cout << "UUID v4: " << uuid_v4.toString() << " (version: " << uuid_v4.version() << ")" << std::endl;
    std::cout << "UUID nil: " << uuid_nil.toString() << " (is nil: " << uuid_nil.isNil() << ")" << std::endl;

    // Test different string formats
    std::cout << "\nString formats for v4 UUID:" << std::endl;
    std::cout << "  Standard: " << uuid_v4.toString() << std::endl;
    std::cout << "  Uppercase: " << uuid_v4.toUpperString() << std::endl;
    std::cout << "  Compact: " << uuid_v4.toCompactString() << std::endl;
    std::cout << "  Hex: " << uuid_v4.toHex() << std::endl;
    std::cout << "  Base64: " << uuid_v4.toBase64() << std::endl;
    std::cout << "  Formatted: " << uuid_v4.format() << std::endl;
}

void demonstrateNamespaceUUIDs() {
    std::cout << "\n=== Namespace UUIDs ===" << std::endl;

    auto dns_namespace = UUID::namespaceDNS();
    auto url_namespace = UUID::namespaceURL();
    auto oid_namespace = UUID::namespaceOID();

    std::cout << "DNS namespace: " << dns_namespace.toString() << std::endl;
    std::cout << "URL namespace: " << url_namespace.toString() << std::endl;
    std::cout << "OID namespace: " << oid_namespace.toString() << std::endl;

    // Generate name-based UUIDs
    std::string name = "example.com";
    auto uuid_v3 = UUID::v3(dns_namespace, name);
    auto uuid_v5 = UUID::v5(dns_namespace, name);

    std::cout << "\nName-based UUIDs for '" << name << "':" << std::endl;
    std::cout << "  v3 (MD5): " << uuid_v3.toString() << std::endl;
    std::cout << "  v5 (SHA-1): " << uuid_v5.toString() << std::endl;

    // Test deterministic generation
    auto uuid_v3_again = UUID::v3(dns_namespace, name);
    auto uuid_v5_again = UUID::v5(dns_namespace, name);

    std::cout << "\nDeterministic test:" << std::endl;
    std::cout << "  v3 matches: " << (uuid_v3 == uuid_v3_again) << std::endl;
    std::cout << "  v5 matches: " << (uuid_v5 == uuid_v5_again) << std::endl;
}

void demonstrateUUIDValidation() {
    std::cout << "\n=== UUID Validation ===" << std::endl;

    std::vector<std::string> test_strings = {
        "550e8400-e29b-41d4-a716-446655440000",  // Valid
        "550e8400-e29b-41d4-a716-44665544000",   // Too short
        "550e8400-e29b-41d4-a716-44665544000g",  // Invalid character
        "550e8400e29b41d4a716446655440000",       // No hyphens
        "not-a-uuid-at-all",                     // Invalid format
        "00000000-0000-0000-0000-000000000000"   // Nil UUID
    };

    for (const auto& str : test_strings) {
        bool is_valid_format = UUIDValidator::isValidFormat(str);
        auto parsed_uuid = UUID::parse(str);

        std::cout << "'" << str << "'" << std::endl;
        std::cout << "  Valid format: " << is_valid_format << std::endl;
        std::cout << "  Parsed successfully: " << (parsed_uuid.has_value()) << std::endl;

        if (parsed_uuid) {
            std::cout << "  Is valid UUID: " << parsed_uuid->isValid() << std::endl;
            std::cout << "  Version: " << parsed_uuid->version() << std::endl;
            std::cout << "  Variant: " << parsed_uuid->variant() << std::endl;
        }
        std::cout << std::endl;
    }
}

void demonstrateUUIDBinaryOperations() {
    std::cout << "\n=== Binary Operations ===" << std::endl;

    auto uuid = UUID::v4();
    std::cout << "Original UUID: " << uuid.toString() << std::endl;

    // Convert to bytes and back
    auto bytes_vector = uuid.toBytes();
    auto bytes_array = uuid.toBytesArray();

    std::cout << "Bytes (vector): ";
    for (auto byte : bytes_vector) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;

    // Reconstruct from bytes
    auto reconstructed = UUID::fromBytes(bytes_vector);
    std::cout << "Reconstructed: " << reconstructed.toString() << std::endl;
    std::cout << "Match: " << (uuid == reconstructed) << std::endl;

    // Test Hamming distance
    auto another_uuid = UUID::v4();
    auto distance = uuid.hammingDistance(another_uuid);
    std::cout << "\nHamming distance to another UUID: " << distance << " bits" << std::endl;
}

void demonstrateTimestampExtraction() {
    std::cout << "\n=== Timestamp Extraction ===" << std::endl;

    auto uuid_v1 = UUID::v1();
    std::cout << "UUID v1: " << uuid_v1.toString() << std::endl;

    try {
        auto timestamp = uuid_v1.getTimestamp();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::cout << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;

        auto node_id = uuid_v1.getNodeId();
        auto clock_seq = uuid_v1.getClockSequence();
        std::cout << "Node ID: 0x" << std::hex << node_id << std::dec << std::endl;
        std::cout << "Clock sequence: " << clock_seq << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error extracting timestamp: " << e.what() << std::endl;
    }

    // Try with v4 UUID (should fail)
    auto uuid_v4 = UUID::v4();
    std::cout << "\nUUID v4: " << uuid_v4.toString() << std::endl;
    try {
        auto timestamp = uuid_v4.getTimestamp();
        std::cout << "Unexpected success!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Expected error: " << e.what() << std::endl;
    }
}

void demonstrateBulkOperations() {
    std::cout << "\n=== Bulk Operations ===" << std::endl;

    // Initialize UUID pool
    UUIDPool::initialize();

    // Generate UUIDs in bulk
    const size_t batch_size = 10000;
    std::cout << "Generating " << batch_size << " UUIDs..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto uuids = UUID::generateBatch(batch_size);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Generated " << uuids.size() << " UUIDs in " << duration.count() << " microseconds" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(0)
              << (static_cast<double>(batch_size) / duration.count() * 1000000) << " UUIDs/second" << std::endl;

    // Test uniqueness
    std::unordered_set<std::string> unique_uuids;
    for (const auto& uuid : uuids) {
        unique_uuids.insert(uuid.toString());
    }

    std::cout << "Unique UUIDs: " << unique_uuids.size() << " / " << uuids.size() << std::endl;
    std::cout << "Collision rate: " << std::fixed << std::setprecision(6)
              << (static_cast<double>(batch_size - unique_uuids.size()) / batch_size * 100.0) << "%" << std::endl;

    // Test bulk string conversion
    std::vector<boost::uuids::uuid> boost_uuids;
    for (const auto& uuid : uuids) {
        boost_uuids.push_back(uuid.getUUID());
    }

    start = std::chrono::high_resolution_clock::now();
    auto strings = UUIDBulkOperations::toStringsBulk(boost_uuids);
    end = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Converted " << strings.size() << " UUIDs to strings in " << duration.count() << " microseconds" << std::endl;

    // Test bulk parsing
    start = std::chrono::high_resolution_clock::now();
    auto parsed = UUIDBulkOperations::parseStringsBulk(strings);
    end = std::chrono::high_resolution_clock::now();

    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Parsed " << parsed.size() << " UUID strings in " << duration.count() << " microseconds" << std::endl;

    size_t successful_parses = 0;
    for (const auto& opt_uuid : parsed) {
        if (opt_uuid.has_value()) {
            successful_parses++;
        }
    }
    std::cout << "Successful parses: " << successful_parses << " / " << parsed.size() << std::endl;

    // Cleanup
    UUIDPool::shutdown();
}

void demonstrateStatistics() {
    std::cout << "\n=== Statistics ===" << std::endl;

    UUID::resetStatistics();

    // Generate various UUIDs
    auto v1_uuid = UUID::v1();
    auto v4_uuid = UUID::v4();
    auto v3_uuid = UUID::v3(UUID::namespaceDNS(), "test");
    auto v5_uuid = UUID::v5(UUID::namespaceURL(), "test");
    auto batch = UUID::generateBatch(100);

    auto stats = UUID::getStatistics();
    std::cout << "UUID Generation Statistics:" << std::endl;
    std::cout << "  Total generated: " << stats.total_generated.load() << std::endl;
    std::cout << "  v1 generated: " << stats.v1_generated.load() << std::endl;
    std::cout << "  v3 generated: " << stats.v3_generated.load() << std::endl;
    std::cout << "  v4 generated: " << stats.v4_generated.load() << std::endl;
    std::cout << "  v5 generated: " << stats.v5_generated.load() << std::endl;
    std::cout << "  Bulk operations: " << stats.bulk_operations.load() << std::endl;
    std::cout << "  Pool hits: " << stats.pool_hits.load() << std::endl;
    std::cout << "  Pool misses: " << stats.pool_misses.load() << std::endl;
}

int main() {
    try {
        demonstrateBasicUUIDOperations();
        demonstrateNamespaceUUIDs();
        demonstrateUUIDValidation();
        demonstrateUUIDBinaryOperations();
        demonstrateTimestampExtraction();
        demonstrateBulkOperations();
        demonstrateStatistics();

        std::cout << "\n=== All UUID tests completed successfully! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
