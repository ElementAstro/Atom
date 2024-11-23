#include "atom/utils/aes.hpp"

#include <iostream>
#include <vector>

using namespace atom::utils;

int main() {
    // Example plaintext and key for AES encryption
    std::string plaintext = "Hello, World!";
    std::string key = "thisisaverysecretkey123456";

    // Vectors to store IV and tag for AES encryption
    std::vector<unsigned char> iv(12);
    std::vector<unsigned char> tag(16);

    // Encrypt the plaintext using AES
    std::string ciphertext = encryptAES(plaintext, key, iv, tag);
    std::cout << "Encrypted ciphertext: " << ciphertext << std::endl;

    // Decrypt the ciphertext using AES
    std::string decryptedText = decryptAES(ciphertext, key, iv, tag);
    std::cout << "Decrypted plaintext: " << decryptedText << std::endl;

    // Example data for compression
    std::string data = "This is some data to be compressed.";

    // Compress the data using Zlib
    std::string compressedData = compress(data);
    std::cout << "Compressed data: " << compressedData << std::endl;

    // Decompress the data using Zlib
    std::string decompressedData = decompress(compressedData);
    std::cout << "Decompressed data: " << decompressedData << std::endl;

    // Calculate the SHA-256 hash of a file
    std::string filename = "example.txt";
    std::string sha256Hash = calculateSha256(filename);
    std::cout << "SHA-256 hash of file: " << sha256Hash << std::endl;

    // Calculate the SHA-224 hash of a string
    std::string sha224Hash = calculateSha224(data);
    std::cout << "SHA-224 hash of string: " << sha224Hash << std::endl;

    // Calculate the SHA-384 hash of a string
    std::string sha384Hash = calculateSha384(data);
    std::cout << "SHA-384 hash of string: " << sha384Hash << std::endl;

    // Calculate the SHA-512 hash of a string
    std::string sha512Hash = calculateSha512(data);
    std::cout << "SHA-512 hash of string: " << sha512Hash << std::endl;

    return 0;
}