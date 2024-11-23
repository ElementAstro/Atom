#include "atom/extra/boost/uuid.hpp"

#include <chrono>
#include <iostream>
#include <span>
#include <vector>

using namespace atom::extra::boost;

int main() {
    // Default constructor that generates a random UUID (v4)
    UUID uuid1;
    std::cout << "UUID v4: " << uuid1.toString() << std::endl;

    // Constructs a UUID from a string representation
    UUID uuid2("550e8400-e29b-41d4-a716-446655440000");
    std::cout << "UUID from string: " << uuid2.toString() << std::endl;

    // Constructs a UUID from a Boost.UUID object
    ::boost::uuids::uuid boostUuid = ::boost::uuids::random_generator()();
    UUID uuid3(boostUuid);
    std::cout << "UUID from Boost.UUID: " << uuid3.toString() << std::endl;

    // Converts the UUID to a string representation
    std::string uuidStr = uuid1.toString();
    std::cout << "UUID to string: " << uuidStr << std::endl;

    // Checks if the UUID is nil (all zeros)
    bool isNil = uuid1.isNil();
    std::cout << "Is UUID nil: " << std::boolalpha << isNil << std::endl;

    // Compares this UUID with another UUID
    auto comparison = uuid1 <=> uuid2;
    std::cout << "UUID comparison: "
              << (comparison == 0 ? "equal"
                                  : (comparison < 0 ? "less" : "greater"))
              << std::endl;

    // Checks if this UUID is equal to another UUID
    bool isEqual = (uuid1 == uuid2);
    std::cout << "UUIDs are equal: " << std::boolalpha << isEqual << std::endl;

    // Formats the UUID as a string enclosed in curly braces
    std::string formattedUuid = uuid1.format();
    std::cout << "Formatted UUID: " << formattedUuid << std::endl;

    // Converts the UUID to a vector of bytes
    std::vector<uint8_t> uuidBytes = uuid1.toBytes();
    std::cout << "UUID to bytes: ";
    for (auto byte : uuidBytes) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Constructs a UUID from a span of bytes
    UUID uuidFromBytes = UUID::fromBytes(std::span<const uint8_t>(uuidBytes));
    std::cout << "UUID from bytes: " << uuidFromBytes.toString() << std::endl;

    // Converts the UUID to a 64-bit unsigned integer
    uint64_t uuidUint64 = uuid1.toUint64();
    std::cout << "UUID to uint64: " << uuidUint64 << std::endl;

    // Gets the DNS namespace UUID
    UUID dnsNamespace = UUID::namespaceDNS();
    std::cout << "DNS namespace UUID: " << dnsNamespace.toString() << std::endl;

    // Gets the URL namespace UUID
    UUID urlNamespace = UUID::namespaceURL();
    std::cout << "URL namespace UUID: " << urlNamespace.toString() << std::endl;

    // Gets the OID namespace UUID
    UUID oidNamespace = UUID::namespaceOID();
    std::cout << "OID namespace UUID: " << oidNamespace.toString() << std::endl;

    // Generates a version 3 (MD5) UUID based on a namespace UUID and a name
    UUID uuidV3 = UUID::v3(dnsNamespace, "example.com");
    std::cout << "UUID v3: " << uuidV3.toString() << std::endl;

    // Generates a version 5 (SHA-1) UUID based on a namespace UUID and a name
    UUID uuidV5 = UUID::v5(urlNamespace, "example.com");
    std::cout << "UUID v5: " << uuidV5.toString() << std::endl;

    // Gets the version of the UUID
    int uuidVersion = uuid1.version();
    std::cout << "UUID version: " << uuidVersion << std::endl;

    // Gets the variant of the UUID
    int uuidVariant = uuid1.variant();
    std::cout << "UUID variant: " << uuidVariant << std::endl;

    // Generates a version 1 (timestamp-based) UUID
    UUID uuidV1 = UUID::v1();
    std::cout << "UUID v1: " << uuidV1.toString() << std::endl;

    // Generates a version 4 (random) UUID
    UUID uuidV4 = UUID::v4();
    std::cout << "UUID v4: " << uuidV4.toString() << std::endl;

    // Converts the UUID to a Base64 string representation
    std::string uuidBase64 = uuid1.toBase64();
    std::cout << "UUID to Base64: " << uuidBase64 << std::endl;

    // Gets the timestamp from a version 1 UUID
    try {
        auto timestamp = uuidV1.getTimestamp();
        std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
        std::cout << "UUID v1 timestamp: " << std::ctime(&time);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Hash function for UUIDs
    std::hash<UUID> uuidHash;
    size_t hashValue = uuidHash(uuid1);
    std::cout << "UUID hash value: " << hashValue << std::endl;

    return 0;
}