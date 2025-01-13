#include "atom/algorithm/blowfish.hpp"
#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <vector>

using namespace atom::algorithm;

class BlowfishTest : public ::testing::Test {
protected:
    void SetUp() override {
        const unsigned char key[] = "testkey123";
        blowfish = std::make_unique<Blowfish>(key, sizeof(key) - 1);

        // Create test files directory if it doesn't exist
        system("mkdir -p test_files");
    }

    void TearDown() override {
        // Cleanup test files
        system("rm -rf test_files");
    }

    // Utility function to compare binary data
    bool compareData(const unsigned char* data1, const unsigned char* data2,
                     size_t length) {
        return memcmp(data1, data2, length) == 0;
    }

    // Utility function to create test file
    void createTestFile(const std::string& filename,
                        const std::string& content) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::unique_ptr<Blowfish> blowfish;
};

TEST_F(BlowfishTest, BasicEncryptionDecryption) {
    unsigned char data[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    unsigned char original[8];
    memcpy(original, data, 8);

    // Encrypt
    blowfish->encrypt(data);
    EXPECT_FALSE(compareData(data, original, 8));

    // Decrypt
    blowfish->decrypt(data);
    EXPECT_TRUE(compareData(data, original, 8));
}

TEST_F(BlowfishTest, PKCS7Padding) {
    std::vector<unsigned char> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t length = data.size();

    // Add padding
    blowfish->pkcs7_padding(data.data(), length, 8);
    EXPECT_EQ(length, 8);

    // Check padding value
    for (size_t i = 5; i < length; i++) {
        EXPECT_EQ(data[i], 3);  // Should be padded with value 3
    }
}

TEST_F(BlowfishTest, RemovePadding) {
    std::vector<unsigned char> data(8, 0);
    size_t length = 8;
    data[7] = 3;  // Padding of 3 bytes

    blowfish->remove_padding(data.data(), length);
    EXPECT_EQ(length, 5);
}

TEST_F(BlowfishTest, EncryptDecryptData) {
    std::vector<unsigned char> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<unsigned char> original = data;
    size_t length = data.size();

    blowfish->encrypt_data(data.data(), length, 8);
    EXPECT_NE(length, original.size());  // Length should change due to padding

    blowfish->decrypt_data(data.data(), length, 8);
    EXPECT_EQ(length, original.size());
    EXPECT_TRUE(compareData(data.data(), original.data(), original.size()));
}

TEST_F(BlowfishTest, FileEncryptionDecryption) {
    std::string test_content = "Hello, World!";
    createTestFile("test_files/input.txt", test_content);

    EXPECT_NO_THROW({
        blowfish->encrypt_file("test_files/input.txt",
                               "test_files/encrypted.txt");
        blowfish->decrypt_file("test_files/encrypted.txt",
                               "test_files/decrypted.txt");
    });

    // Read decrypted content
    std::ifstream decrypted_file("test_files/decrypted.txt");
    std::string decrypted_content;
    std::getline(decrypted_file, decrypted_content);

    EXPECT_EQ(decrypted_content, test_content);
}

TEST_F(BlowfishTest, InvalidFileHandling) {
    EXPECT_THROW(
        blowfish->encrypt_file("nonexistent.txt", "test_files/output.txt"),
        std::runtime_error);
}

TEST_F(BlowfishTest, InvalidPaddingHandling) {
    std::vector<unsigned char> data(8, 0);
    size_t length = 8;
    data[7] = 9;  // Invalid padding value (greater than block size)

    EXPECT_THROW(blowfish->remove_padding(data.data(), length),
                 std::runtime_error);
}

TEST_F(BlowfishTest, ZeroLengthData) {
    std::vector<unsigned char> data;
    size_t length = 0;

    EXPECT_NO_THROW({ blowfish->remove_padding(data.data(), length); });
    EXPECT_EQ(length, 0);
}

TEST_F(BlowfishTest, LargeDataEncryption) {
    std::vector<unsigned char> data(1024, 'A');
    size_t length = data.size();
    std::vector<unsigned char> original = data;

    EXPECT_NO_THROW({
        blowfish->encrypt_data(data.data(), length, 8);
        blowfish->decrypt_data(data.data(), length, 8);
    });

    EXPECT_TRUE(compareData(data.data(), original.data(), original.size()));
}

TEST_F(BlowfishTest, FFunction) {
    unsigned int input = 0x01234567;
    unsigned int output = blowfish->F(input);
    EXPECT_NE(input, output);  // F function should transform the input
}
