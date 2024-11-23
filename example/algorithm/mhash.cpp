#include "atom/algorithm/mhash.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace atom::algorithm;

    // Example string to be converted to hexstring and back
    std::string exampleString = "Hello, World!";

    // Convert string to hexstring
    std::string hexString = hexstringFromData(exampleString);
    std::cout << "Hexstring: " << hexString << std::endl;

    // Convert hexstring back to string
    std::string originalString = dataFromHexstring(hexString);
    std::cout << "Original String: " << originalString << std::endl;

    // Create a MinHash object with 100 hash functions
    MinHash minHash(100);

    // Example sets
    std::vector<int> set1 = {1, 2, 3, 4, 5};
    std::vector<int> set2 = {4, 5, 6, 7, 8};

    // Compute MinHash signatures for the sets
    std::vector<size_t> signature1 = minHash.computeSignature(set1);
    std::vector<size_t> signature2 = minHash.computeSignature(set2);

    // Print MinHash signatures
    std::cout << "MinHash Signature for Set 1: ";
    for (const auto& hash : signature1) {
        std::cout << hash << " ";
    }
    std::cout << std::endl;

    std::cout << "MinHash Signature for Set 2: ";
    for (const auto& hash : signature2) {
        std::cout << hash << " ";
    }
    std::cout << std::endl;

    // Compute Jaccard index between the two sets based on their MinHash
    // signatures
    double jaccardIndex = MinHash::jaccardIndex(signature1, signature2);
    std::cout << "Jaccard Index between Set 1 and Set 2: " << jaccardIndex
              << std::endl;

    // Example input for keccak256 hash function
    const uint8_t input[] = "Hello, World!";
    size_t length = sizeof(input) - 1;

    // Compute keccak256 hash
    std::array<uint8_t, K_HASH_SIZE> hash = keccak256(input, length);

    // Print keccak256 hash
    std::cout << "Keccak256 Hash: ";
    for (const auto& byte : hash) {
        std::cout << std::hex << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;

    return 0;
}