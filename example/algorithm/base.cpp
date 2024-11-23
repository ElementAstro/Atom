#include "base.hpp"

#include <iostream>

int main() {
    // Example usage of base64Encode and base64Decode
    {
        std::string data = "Hello, World!";
        std::string encoded = atom::algorithm::base64Encode(data);
        std::string decoded = atom::algorithm::base64Decode(encoded);

        std::cout << "Original data: " << data << std::endl;
        std::cout << "Base64 Encoded: " << encoded << std::endl;
        std::cout << "Base64 Decoded: " << decoded << std::endl;
    }

    // Example usage of xorEncrypt and xorDecrypt
    {
        std::string plaintext = "Secret Message";
        uint8_t key = 0xAA;  // Example key
        std::string encrypted = atom::algorithm::xorEncrypt(plaintext, key);
        std::string decrypted = atom::algorithm::xorDecrypt(encrypted, key);

        std::cout << "Original plaintext: " << plaintext << std::endl;
        std::cout << "Encrypted text: " << encrypted << std::endl;
        std::cout << "Decrypted text: " << decrypted << std::endl;
    }

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

    return 0;
}