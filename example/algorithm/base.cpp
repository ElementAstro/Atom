#include "atom/algorithm/base.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::algorithm;

int main() {
    // Example 1: Base64 Encoding
    std::string data = "Hello, World!";
    std::string encoded = base64Encode(data);
    std::cout << "Base64 Encoded: " << encoded << std::endl;

    // Example 2: Base64 Decoding
    std::string decoded = base64Decode(encoded);
    std::cout << "Base64 Decoded: " << decoded << std::endl;

    // Example 3: Faster Base64 Encoding
    std::vector<unsigned char> inputData(data.begin(), data.end());
    std::string fastEncoded = fbase64Encode(inputData);
    std::cout << "Faster Base64 Encoded: " << fastEncoded << std::endl;

    // Example 4: Faster Base64 Decoding
    std::vector<unsigned char> fastDecoded = fbase64Decode(fastEncoded);
    std::string fastDecodedStr(fastDecoded.begin(), fastDecoded.end());
    std::cout << "Faster Base64 Decoded: " << fastDecodedStr << std::endl;

    // Example 5: XOR Encryption
    uint8_t key = 0xAA;  // Example key
    std::string encrypted = xorEncrypt(data, key);
    std::cout << "XOR Encrypted: " << encrypted << std::endl;

    // Example 6: XOR Decryption
    std::string decrypted = xorDecrypt(encrypted, key);
    std::cout << "XOR Decrypted: " << decrypted << std::endl;

    /*
        TODO: Uncomment the following code to run the compile-time examples
        // Example 7: Compile-time Base64 Encoding
        constexpr StaticString<13> staticData("Hello, World!");
        constexpr auto staticEncoded = cbase64Encode(staticData);
        std::cout << "Compile-time Base64 Encoded: " << staticEncoded.c_str()
                  << std::endl;

        // Example 8: Compile-time Base64 Decoding
        constexpr auto staticDecoded = cbase64Decode(staticEncoded);
        std::cout << "Compile-time Base64 Decoded: " << staticDecoded.c_str()
                  << std::endl;
    */

    return 0;
}