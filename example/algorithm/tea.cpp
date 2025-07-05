#include "atom/algorithm/tea.hpp"

#include <iostream>
#include <vector>

int main() {
    using namespace atom::algorithm;

    // Define a 128-bit key for TEA, XTEA, and XXTEA
    std::array<uint32_t, 4> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                   0x2468ACE0};

    // TEA encryption and decryption
    uint32_t teaValue0 = 0x01234567;
    uint32_t teaValue1 = 0x89ABCDEF;
    std::cout << "Original TEA values: " << std::hex << teaValue0 << " "
              << teaValue1 << std::endl;

    teaEncrypt(teaValue0, teaValue1, key);
    std::cout << "Encrypted TEA values: " << std::hex << teaValue0 << " "
              << teaValue1 << std::endl;

    teaDecrypt(teaValue0, teaValue1, key);
    std::cout << "Decrypted TEA values: " << std::hex << teaValue0 << " "
              << teaValue1 << std::endl;

    // XTEA encryption and decryption
    uint32_t xteaValue0 = 0x01234567;
    uint32_t xteaValue1 = 0x89ABCDEF;
    std::cout << "Original XTEA values: " << std::hex << xteaValue0 << " "
              << xteaValue1 << std::endl;

    xteaEncrypt(xteaValue0, xteaValue1, key);
    std::cout << "Encrypted XTEA values: " << std::hex << xteaValue0 << " "
              << xteaValue1 << std::endl;

    xteaDecrypt(xteaValue0, xteaValue1, key);
    std::cout << "Decrypted XTEA values: " << std::hex << xteaValue0 << " "
              << xteaValue1 << std::endl;

    // XXTEA encryption and decryption
    std::vector<uint32_t> xxteaData = {0x01234567, 0x89ABCDEF, 0xFEDCBA98,
                                       0x76543210};
    std::vector<uint32_t> xxteaKey = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                      0x2468ACE0};
    std::cout << "Original XXTEA data: ";
    for (const auto& val : xxteaData) {
        std::cout << std::hex << val << " ";
    }
    std::cout << std::endl;

    std::vector<uint32_t> encryptedXXTEAData =
        xxteaEncrypt(xxteaData, xxteaKey);
    std::cout << "Encrypted XXTEA data: ";
    for (const auto& val : encryptedXXTEAData) {
        std::cout << std::hex << val << " ";
    }
    std::cout << std::endl;

    std::vector<uint32_t> decryptedXXTEAData =
        xxteaDecrypt(encryptedXXTEAData, xxteaKey);
    std::cout << "Decrypted XXTEA data: ";
    for (const auto& val : decryptedXXTEAData) {
        std::cout << std::hex << val << " ";
    }
    std::cout << std::endl;

    // Convert byte array to vector of 32-bit unsigned integers and back
    std::vector<uint8_t> byteArray = {0x01, 0x23, 0x45, 0x67,
                                      0x89, 0xAB, 0xCD, 0xEF};
    std::vector<uint32_t> uint32Vector = toUint32Vector(byteArray);
    std::cout << "Converted to uint32 vector: ";
    for (const auto& val : uint32Vector) {
        std::cout << std::hex << val << " ";
    }
    std::cout << std::endl;

    std::vector<uint8_t> convertedByteArray = toByteArray(uint32Vector);
    std::cout << "Converted back to byte array: ";
    for (const auto& val : convertedByteArray) {
        std::cout << std::hex << static_cast<int>(val) << " ";
    }
    std::cout << std::endl;

    return 0;
}
