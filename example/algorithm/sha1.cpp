#include "sha1.hpp"
#include <iostream>
#include <string>

int main() {
    using namespace atom::algorithm;

    // Create a SHA1 object
    SHA1 sha1;

    // Example data to hash
    std::string data = "Hello, World!";

    // Update the SHA1 object with the data
    sha1.update(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());

    // Get the digest (hash value)
    std::array<uint8_t, SHA1::DIGEST_SIZE> hash = sha1.digest();

    // Convert the digest to a hexadecimal string
    std::string hashHex = bytesToHex(hash);

    // Print the hexadecimal hash
    std::cout << "SHA1 Hash of \"" << data << "\": " << hashHex << std::endl;

    // Reset the SHA1 object for reuse
    sha1.reset();

    // Update the SHA1 object with new data
    std::string newData = "Another string to hash";
    sha1.update(reinterpret_cast<const uint8_t*>(newData.c_str()),
                newData.size());

    // Get the new digest (hash value)
    std::array<uint8_t, SHA1::DIGEST_SIZE> newHash = sha1.digest();

    // Convert the new digest to a hexadecimal string
    std::string newHashHex = bytesToHex(newHash);

    // Print the new hexadecimal hash
    std::cout << "SHA1 Hash of \"" << newData << "\": " << newHashHex
              << std::endl;

    return 0;
}