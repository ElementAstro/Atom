#include "atom/algorithm/base.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Base64 Encoding/Decoding Examples ===" << std::endl;
    // Example usage of base64Encode and base64Decode with expected type
    {
        std::string data = "Hello, World!";

        // Using the expected return type
        auto encodedResult = atom::algorithm::base64Encode(data);

        if (encodedResult) {
            std::string encoded = encodedResult.value();
            std::cout << "Original data: " << data << std::endl;
            std::cout << "Base64 Encoded: " << encoded << std::endl;

            // Decode the encoded string
            auto decodedResult = atom::algorithm::base64Decode(encoded);

            if (decodedResult) {
                std::string decoded = decodedResult.value();
                std::cout << "Base64 Decoded: " << decoded << std::endl;
            } else {
                std::cout << "Base64 Decode Error: "
                          << decodedResult.error().error() << std::endl;
            }
        } else {
            std::cout << "Base64 Encode Error: "
                      << encodedResult.error().error() << std::endl;
        }
    }

    std::cout << "\n=== XOR Encryption/Decryption Examples ===" << std::endl;
    // Example usage of xorEncrypt and xorDecrypt
    {
        std::string plaintext = "Secret Message";
        uint8_t key = 0xAA;  // Example key

        std::string encrypted = atom::algorithm::xorEncrypt(plaintext, key);
        std::string decrypted = atom::algorithm::xorDecrypt(encrypted, key);

        std::cout << "Original plaintext: " << plaintext << std::endl;
        std::cout << "Encrypted text (hex): ";
        for (unsigned char c : encrypted) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(c) << " ";
        }
        std::cout << std::dec << std::endl;
        std::cout << "Decrypted text: " << decrypted << std::endl;
    }

    std::cout << "\n=== Base64 Validation Example ===" << std::endl;
    // Example usage of isBase64
    {
        std::string validBase64 =
            "SGVsbG8sIFdvcmxkIQ==";  // "Hello, World!" in Base64
        std::string invalidBase64 = "InvalidBase64String";

        bool isValid = atom::algorithm::isBase64(validBase64);
        bool isInvalid = atom::algorithm::isBase64(invalidBase64);

        std::cout << "Is valid Base64: " << std::boolalpha << isValid
                  << std::endl;
        std::cout << "Is invalid Base64: " << std::boolalpha << isInvalid
                  << std::endl;
    }

    std::cout << "\n=== Base32 Encoding/Decoding Examples ===" << std::endl;
    // Example usage of encodeBase32 and decodeBase32
    {
        std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"

        auto encodedResult =
            atom::algorithm::encodeBase32(std::span<const uint8_t>(data));

        if (encodedResult) {
            std::string encoded = encodedResult.value();
            std::cout << "Original data (hex): ";
            for (auto byte : data) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(byte) << " ";
            }
            std::cout << std::dec << std::endl;
            std::cout << "Base32 Encoded: " << encoded << std::endl;

            // Decode the encoded string
            auto decodedResult = atom::algorithm::decodeBase32(encoded);

            if (decodedResult) {
                std::vector<uint8_t> decoded = decodedResult.value();
                std::cout << "Base32 Decoded (hex): ";
                for (auto byte : decoded) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte) << " ";
                }
                std::cout << std::dec << std::endl;
            } else {
                std::cout << "Base32 Decode Error: "
                          << decodedResult.error().error() << std::endl;
            }
        } else {
            std::cout << "Base32 Encode Error: "
                      << encodedResult.error().error() << std::endl;
        }
    }

    std::cout << "\n=== Parallel Execution Example ===" << std::endl;
    // Example usage of parallelExecute
    {
        std::vector<int> data(1000);
        for (int i = 0; i < 1000; ++i) {
            data[i] = i;
        }

        // Square all numbers in parallel
        atom::algorithm::parallelExecute(std::span<int>(data), 4,
                                         [](std::span<int> chunk) {
                                             for (int& value : chunk) {
                                                 value = value * value;
                                             }
                                         });

        std::cout << "First 10 squared values: ";
        for (int i = 0; i < 10; ++i) {
            std::cout << data[i] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
