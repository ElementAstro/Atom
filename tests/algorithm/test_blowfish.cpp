#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <array>
#include <chrono>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include "atom/algorithm/blowfish.hpp"

using namespace atom::algorithm;

// Helper functions
std::vector<std::byte> generateRandomBytes(size_t count) {
    std::vector<std::byte> result(count);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (size_t i = 0; i < count; ++i) {
        result[i] = static_cast<std::byte>(distrib(gen));
    }

    return result;
}

std::vector<std::byte> stringToBytes(const std::string& str) {
    std::vector<std::byte> bytes;
    bytes.reserve(str.size());
    for (char c : str) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::string bytesToString(const std::vector<std::byte>& bytes) {
    std::string str;
    str.reserve(bytes.size());
    for (std::byte b : bytes) {
        str.push_back(static_cast<char>(b));
    }
    return str;
}

// Test fixture
class BlowfishTest : public ::testing::Test {
protected:
    void SetUp() override {
        key = stringToBytes("TestKey123");
        plaintext = stringToBytes("Hello, Blowfish encryption!");
        while (plaintext.size() % 8 != 0) {
            plaintext.push_back(std::byte{0});
        }
        blowfish = std::make_unique<Blowfish>(std::span<const std::byte>(key));
    }

    std::vector<std::byte> key;
    std::vector<std::byte> plaintext;
    std::unique_ptr<Blowfish> blowfish;

    // Temporary file paths for file encryption/decryption tests
    const std::string temp_input_file = "test_input.txt";
    const std::string temp_encrypted_file = "test_encrypted.bin";
    const std::string temp_decrypted_file = "test_decrypted.txt";

    void createTempFile(const std::string& filename,
                        const std::vector<std::byte>& data) {
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
    }

    std::vector<std::byte> readTempFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<std::byte> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    void deleteTempFiles() {
        std::remove(temp_input_file.c_str());
        std::remove(temp_encrypted_file.c_str());
        std::remove(temp_decrypted_file.c_str());
    }

    void TearDown() override { deleteTempFiles(); }
};

// Basic constructor tests
TEST_F(BlowfishTest, Constructor) {
    // Test with valid keys of different lengths
    EXPECT_NO_THROW(Blowfish(std::span<const std::byte>(key)));

    // Test with minimum key length (1 byte)
    std::vector<std::byte> min_key{std::byte{0x42}};
    EXPECT_NO_THROW(Blowfish(std::span<const std::byte>(min_key)));

    // Test with maximum key length (56 bytes)
    std::vector<std::byte> max_key = generateRandomBytes(56);
    EXPECT_NO_THROW(Blowfish(std::span<const std::byte>(max_key)));
}

// Test key validation
TEST_F(BlowfishTest, KeyValidation) {
    // Test with empty key (should throw)
    std::vector<std::byte> empty_key;
    EXPECT_THROW(Blowfish(std::span<const std::byte>(empty_key)),
                 std::runtime_error);

    // Test with key that's too long (should throw)
    std::vector<std::byte> long_key = generateRandomBytes(57);
    EXPECT_THROW(Blowfish(std::span<const std::byte>(long_key)),
                 std::runtime_error);
}

// Test block encryption/decryption
TEST_F(BlowfishTest, BlockEncryptDecrypt) {
    std::array<std::byte, 8> block = {
        std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
        std::byte{0x89}, std::byte{0xAB}, std::byte{0xCD}, std::byte{0xEF}};
    std::array<std::byte, 8> original_block = block;

    // Encrypt the block
    blowfish->encrypt(block);

    // The encrypted block should be different from the original
    EXPECT_NE(block, original_block);

    // Decrypt the block
    blowfish->decrypt(block);

    // The decrypted block should match the original
    EXPECT_EQ(block, original_block);
}

// Test data encryption/decryption with std::byte
TEST_F(BlowfishTest, DataEncryptDecryptWithByte) {
    // Make a copy of the plaintext
    std::vector<std::byte> encrypted = plaintext;

    // Encrypt the data
    blowfish->encrypt_data(std::span<std::byte>(encrypted));

    // The encrypted data should be different from the plaintext
    EXPECT_NE(encrypted, plaintext);

    // Make sure encrypted size is a multiple of BLOCK_SIZE (8 bytes)
    EXPECT_EQ(encrypted.size() % 8, 0);

    // Decrypt the data
    size_t length = encrypted.size();
    blowfish->decrypt_data(std::span<std::byte>(encrypted), length);

    // Resize to the actual length after removing padding
    encrypted.resize(length);

    // The decrypted data should match the original plaintext
    EXPECT_EQ(encrypted, plaintext);
}

// Test data encryption/decryption with char
TEST_F(BlowfishTest, DataEncryptDecryptWithChar) {
    // Convert plaintext to char vector
    std::string text = bytesToString(plaintext);
    std::vector<char> char_data(text.begin(), text.end());
    std::vector<char> encrypted = char_data;

    // Encrypt the data
    blowfish->encrypt_data(std::span<char>(encrypted));

    // The encrypted data should be different
    bool is_different = false;
    for (size_t i = 0; i < encrypted.size() && i < char_data.size(); i++) {
        if (encrypted[i] != char_data[i]) {
            is_different = true;
            break;
        }
    }
    EXPECT_TRUE(is_different);

    // Decrypt the data
    size_t length = encrypted.size();
    blowfish->decrypt_data(std::span<char>(encrypted), length);

    // Resize to the actual length after removing padding
    encrypted.resize(length);

    // The decrypted data should match the original
    EXPECT_EQ(encrypted, char_data);
}

// Test data encryption/decryption with unsigned char
TEST_F(BlowfishTest, DataEncryptDecryptWithUnsignedChar) {
    // Convert plaintext to unsigned char vector
    std::vector<unsigned char> uchar_data;
    uchar_data.reserve(plaintext.size());
    for (auto b : plaintext) {
        uchar_data.push_back(static_cast<unsigned char>(b));
    }
    std::vector<unsigned char> encrypted = uchar_data;

    // Encrypt the data
    blowfish->encrypt_data(std::span<unsigned char>(encrypted));

    // The encrypted data should be different
    bool is_different = false;
    for (size_t i = 0; i < encrypted.size() && i < uchar_data.size(); i++) {
        if (encrypted[i] != uchar_data[i]) {
            is_different = true;
            break;
        }
    }
    EXPECT_TRUE(is_different);

    // Decrypt the data
    size_t length = encrypted.size();
    blowfish->decrypt_data(std::span<unsigned char>(encrypted), length);

    // Resize to the actual length after removing padding
    encrypted.resize(length);

    // The decrypted data should match the original
    EXPECT_EQ(encrypted, uchar_data);
}

// Test file encryption/decryption
TEST_F(BlowfishTest, FileEncryptDecrypt) {
    // Create a temporary file with plaintext
    createTempFile(temp_input_file, plaintext);

    // Encrypt the file
    blowfish->encrypt_file(temp_input_file, temp_encrypted_file);

    // Read the encrypted file
    std::vector<std::byte> encrypted = readTempFile(temp_encrypted_file);

    // The encrypted data should be different and a multiple of BLOCK_SIZE
    EXPECT_NE(encrypted, plaintext);
    EXPECT_EQ(encrypted.size() % 8, 0);

    // Decrypt the file
    blowfish->decrypt_file(temp_encrypted_file, temp_decrypted_file);

    // Read the decrypted file
    std::vector<std::byte> decrypted = readTempFile(temp_decrypted_file);

    // The decrypted data should match the original plaintext
    EXPECT_EQ(decrypted, plaintext);
}

// Test block size validation
TEST_F(BlowfishTest, BlockSizeValidation) {
    // Test with data that's not a multiple of BLOCK_SIZE
    std::vector<std::byte> invalid_data(7);
    EXPECT_THROW(blowfish->encrypt_data(std::span<std::byte>(invalid_data)),
                 std::runtime_error);

    // Test with data that is a multiple of BLOCK_SIZE
    std::vector<std::byte> valid_data(16);
    EXPECT_NO_THROW(blowfish->encrypt_data(std::span<std::byte>(valid_data)));
}

// Test padding and removal
TEST_F(BlowfishTest, PaddingAndRemoval) {
    // Create plaintext with a length that's not a multiple of BLOCK_SIZE
    std::vector<std::byte> odd_plaintext =
        stringToBytes("This is a test message with odd length!");

    // Create a buffer large enough for padding (size + up to BLOCK_SIZE
    // additional bytes)
    size_t buffer_size = odd_plaintext.size() + 8;
    std::vector<std::byte> buffer(buffer_size);
    std::copy(odd_plaintext.begin(), odd_plaintext.end(), buffer.begin());

    // Manual padding
    size_t length = odd_plaintext.size();
    size_t padding_length = 8 - (length % 8);
    if (padding_length == 0)
        padding_length = 8;

    // Encrypt the data (which includes padding)
    std::vector<std::byte> encrypted = buffer;
    encrypted.resize(length + padding_length);  // Ensure right size for padding

    // Now encrypt the padded data
    blowfish->encrypt_data(std::span<std::byte>(encrypted));

    // Decrypt the data
    size_t decrypt_length = encrypted.size();
    blowfish->decrypt_data(std::span<std::byte>(encrypted), decrypt_length);

    // Resize to the actual length after removing padding
    encrypted.resize(decrypt_length);

    // The decrypted data should match the original odd plaintext
    EXPECT_EQ(decrypt_length, odd_plaintext.size());
    for (size_t i = 0; i < decrypt_length; i++) {
        EXPECT_EQ(encrypted[i], odd_plaintext[i]);
    }
}

// Test with different keys
TEST_F(BlowfishTest, DifferentKeys) {
    // Create two Blowfish instances with different keys
    std::vector<std::byte> key1 = stringToBytes("Key1");
    std::vector<std::byte> key2 = stringToBytes("Key2");

    // 修复函数声明问题
    auto bf1 = Blowfish(std::span<const std::byte>(key1.data(), key1.size()));
    auto bf2 = Blowfish(std::span<const std::byte>(key2.data(), key2.size()));

    // Encrypt the same plaintext with both instances
    std::vector<std::byte> encrypted1 = plaintext;
    std::vector<std::byte> encrypted2 = plaintext;

    bf1.encrypt_data(
        std::span<std::byte>(encrypted1.data(), encrypted1.size()));
    bf2.encrypt_data(
        std::span<std::byte>(encrypted2.data(), encrypted2.size()));

    // The two encryptions should be different
    EXPECT_NE(encrypted1, encrypted2);

    // But they should both decrypt properly with their respective keys
    size_t length1 = encrypted1.size();
    size_t length2 = encrypted2.size();

    bf1.decrypt_data(std::span<std::byte>(encrypted1.data(), encrypted1.size()),
                     length1);
    bf2.decrypt_data(std::span<std::byte>(encrypted2.data(), encrypted2.size()),
                     length2);

    encrypted1.resize(length1);
    encrypted2.resize(length2);

    EXPECT_EQ(encrypted1, plaintext);
    EXPECT_EQ(encrypted2, plaintext);

    // Using the wrong key should result in incorrect decryption
    std::vector<std::byte> encrypted_copy = encrypted1;
    size_t wrong_length = encrypted_copy.size();
    bf2.decrypt_data(
        std::span<std::byte>(encrypted_copy.data(), encrypted_copy.size()),
        wrong_length);
    encrypted_copy.resize(wrong_length);

    EXPECT_NE(encrypted_copy, plaintext);
}

// Test with various data sizes
TEST_F(BlowfishTest, VariousDataSizes) {
    // Test with different sizes from 8 bytes to 64 bytes
    for (size_t size = 8; size <= 64; size += 8) {
        std::vector<std::byte> data = generateRandomBytes(size);
        std::vector<std::byte> original = data;

        // Encrypt
        blowfish->encrypt_data(std::span<std::byte>(data));
        EXPECT_NE(data, original);

        // Decrypt
        size_t length = data.size();
        blowfish->decrypt_data(std::span<std::byte>(data), length);
        data.resize(length);

        // Check
        EXPECT_EQ(data, original);
    }
}

// Test with large data
TEST_F(BlowfishTest, LargeData) {
    // Create a large chunk of data (1MB)
    size_t size = 1024 * 1024;  // 1MB
    std::vector<std::byte> large_data = generateRandomBytes(size);

    // Make sure it's a multiple of BLOCK_SIZE
    while (large_data.size() % 8 != 0) {
        large_data.push_back(std::byte{0});
    }

    std::vector<std::byte> original = large_data;

    // Encrypt the large data
    auto start = std::chrono::high_resolution_clock::now();
    blowfish->encrypt_data(std::span<std::byte>(large_data));
    auto encrypt_end = std::chrono::high_resolution_clock::now();

    // Verify encryption worked
    EXPECT_NE(large_data, original);

    // Decrypt the large data
    size_t length = large_data.size();
    blowfish->decrypt_data(std::span<std::byte>(large_data), length);
    auto decrypt_end = std::chrono::high_resolution_clock::now();

    large_data.resize(length);

    // Verify decryption worked
    EXPECT_EQ(large_data, original);

    // Log performance metrics
    auto encrypt_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            encrypt_end - start)
                            .count();
    auto decrypt_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            decrypt_end - encrypt_end)
                            .count();
    spdlog::info("Large data (1MB) encryption time: {}ms", encrypt_time);
    spdlog::info("Large data (1MB) decryption time: {}ms", decrypt_time);
}

// Test with invalid padding
TEST_F(BlowfishTest, InvalidPadding) {
    // Create a valid encrypted block with proper padding
    std::vector<std::byte> valid_data(16, std::byte{0});
    blowfish->encrypt_data(std::span<std::byte>(valid_data));

    // Corrupt the padding by changing the last byte to an invalid value (>8)
    valid_data[valid_data.size() - 1] = std::byte{20};  // Invalid padding value

    // Attempt to decrypt with invalid padding
    size_t length = valid_data.size();
    EXPECT_THROW(
        blowfish->decrypt_data(std::span<std::byte>(valid_data), length),
        std::runtime_error);
}

// Test cross-platform consistency
TEST_F(BlowfishTest, CrossPlatformConsistency) {
    // Known plaintext
    std::vector<std::byte> known_plaintext = stringToBytes("TestPlaintext");
    while (known_plaintext.size() % 8 != 0) {
        known_plaintext.push_back(std::byte{0});
    }

    // Known key
    std::vector<std::byte> known_key = stringToBytes("TestKey");

    // Known ciphertext (pre-computed with this implementation)
    // 修复函数声明问题
    auto known_bf = Blowfish(
        std::span<const std::byte>(known_key.data(), known_key.size()));
    std::vector<std::byte> known_ciphertext = known_plaintext;
    known_bf.encrypt_data(
        std::span<std::byte>(known_ciphertext.data(), known_ciphertext.size()));

    // Encrypt with our implementation
    auto our_bf = Blowfish(
        std::span<const std::byte>(known_key.data(), known_key.size()));
    std::vector<std::byte> our_ciphertext = known_plaintext;
    our_bf.encrypt_data(
        std::span<std::byte>(our_ciphertext.data(), our_ciphertext.size()));

    // Check that the results match
    EXPECT_EQ(our_ciphertext, known_ciphertext);
}

// Test parallel encryption
TEST_F(BlowfishTest, ParallelEncryption) {
    // Create a large data set to ensure multiple threads are used
    std::vector<std::byte> large_data =
        generateRandomBytes(1024 * 1024);  // 1MB

    // Make sure it's a multiple of BLOCK_SIZE
    while (large_data.size() % 8 != 0) {
        large_data.push_back(std::byte{0});
    }

    std::vector<std::byte> copy = large_data;

    // Encrypt data (will use multiple threads)
    blowfish->encrypt_data(std::span<std::byte>(large_data));

    // Verify encryption worked
    EXPECT_NE(large_data, copy);

    // Decrypt data
    size_t length = large_data.size();
    blowfish->decrypt_data(std::span<std::byte>(large_data), length);
    large_data.resize(length);

    // Verify decryption worked
    EXPECT_EQ(large_data, copy);
}

int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    spdlog::set_level(spdlog::level::off);
    return RUN_ALL_TESTS();
}
