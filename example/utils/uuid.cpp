#include "atom/utils/uuid.hpp"

#include <iostream>
#include <sstream>

using namespace atom::utils;

int main() {
    // Generate a random UUID
    UUID uuid1;
    std::cout << "Random UUID: " << uuid1.toString() << std::endl;

    // Generate a UUID from a 16-byte array
    std::array<uint8_t, 16> data = {0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
                                    0xcd, 0xef, 0x12, 0x34, 0x56, 0x78,
                                    0x90, 0xab, 0xcd, 0xef};
    UUID uuid2(data);
    std::cout << "UUID from array: " << uuid2.toString() << std::endl;

    // Convert a UUID to a string and back
    std::string uuidStr = uuid1.toString();
    UUID uuid3 = UUID::fromString(uuidStr);
    std::cout << "UUID from string: " << uuid3.toString() << std::endl;

    // Compare UUIDs for equality
    bool isEqual = (uuid1 == uuid3);
    std::cout << "UUID1 is equal to UUID3: " << std::boolalpha << isEqual
              << std::endl;

    // Compare UUIDs for inequality
    bool isNotEqual = (uuid1 != uuid2);
    std::cout << "UUID1 is not equal to UUID2: " << std::boolalpha << isNotEqual
              << std::endl;

    // Compare UUIDs using less-than operator
    bool isLessThan = (uuid1 < uuid2);
    std::cout << "UUID1 is less than UUID2: " << std::boolalpha << isLessThan
              << std::endl;

    // Output UUID to a stream
    std::cout << "UUID1: " << uuid1 << std::endl;

    // Input UUID from a stream
    std::istringstream iss(uuidStr);
    UUID uuid4;
    iss >> uuid4;
    std::cout << "UUID4 from stream: " << uuid4.toString() << std::endl;

    // Get the underlying data of the UUID
    std::array<uint8_t, 16> uuidData = uuid1.getData();
    std::cout << "UUID1 data: ";
    for (auto byte : uuidData) {
        std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::endl;

    // Get the version and variant of the UUID
    uint8_t version = uuid1.version();
    uint8_t variant = uuid1.variant();
    std::cout << "UUID1 version: " << static_cast<int>(version) << std::endl;
    std::cout << "UUID1 variant: " << static_cast<int>(variant) << std::endl;

    // Generate a version 3 UUID using MD5 hashing
    UUID namespaceUUID = UUID::generateV4();
    UUID uuidV3 = UUID::generateV3(namespaceUUID, "example");
    std::cout << "Version 3 UUID: " << uuidV3.toString() << std::endl;

    // Generate a version 5 UUID using SHA-1 hashing
    UUID uuidV5 = UUID::generateV5(namespaceUUID, "example");
    std::cout << "Version 5 UUID: " << uuidV5.toString() << std::endl;

    // Generate a version 1, time-based UUID
    UUID uuidV1 = UUID::generateV1();
    std::cout << "Version 1 UUID: " << uuidV1.toString() << std::endl;

    // Generate a version 4, random UUID
    UUID uuidV4 = UUID::generateV4();
    std::cout << "Version 4 UUID: " << uuidV4.toString() << std::endl;

    // Generate a unique UUID and return it as a string
    std::string uniqueUUID = generateUniqueUUID();
    std::cout << "Unique UUID: " << uniqueUUID << std::endl;

#if ATOM_USE_SIMD
    // Create a FastUUID instance
    FastUUID fastUUID;
    std::cout << "FastUUID: " << fastUUID.str() << std::endl;

    // Create a FastUUID from a string
    FastUUID fastUUIDFromString = FastUUID::fromStrFactory(uniqueUUID);
    std::cout << "FastUUID from string: " << fastUUIDFromString.str()
              << std::endl;

    // Compare FastUUIDs
    bool fastUUIDEqual = (fastUUID == fastUUIDFromString);
    std::cout << "FastUUIDs are equal: " << std::boolalpha << fastUUIDEqual
              << std::endl;

    // Output FastUUID to a stream
    std::cout << "FastUUID: " << fastUUID << std::endl;

    // Input FastUUID from a stream
    std::istringstream fastUUIDStream(uniqueUUID);
    FastUUID fastUUIDFromStream;
    fastUUIDStream >> fastUUIDFromStream;
    std::cout << "FastUUID from stream: " << fastUUIDFromStream.str()
              << std::endl;

    // Generate FastUUID using a generator
    FastUUIDGenerator<std::mt19937> fastUUIDGen;
    FastUUID generatedFastUUID = fastUUIDGen.getUUID();
    std::cout << "Generated FastUUID: " << generatedFastUUID.str() << std::endl;
#endif

    return 0;
}