#include "blowfish.hpp"

#include <cstring>
#include <fstream>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace atom::algorithm {
// Blowfish algorithm constants
static unsigned int P[18] = {
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344, 0xa4093822, 0x299f31d0,
    0x082efa98, 0xec4e6c89, 0x557a8e8c, 0x163f1dbe, 0x37e1e9af, 0x37cda6b7,
    0x58e0f419, 0x3de9c6a1, 0x6e10e33f, 0x28782c2f, 0x1f2b4e36, 0x74855fa2};

static unsigned int S[4][256] = {
    {0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0x1cf0c0b7, 0x3f86c1b7, 0x467b5f8a,
     0x8b7e1af1, 0x3c0f1f2c, 0x71c6d6b3, 0x1d28c3f1, 0x62dd8a3f, 0x77b5f3ed,
     0x8a3f8d6e, 0x201b69d7, 0x1b99848e, 0xd6971d3b, 0xa07f8b52, 0x09b6f693,
     0x877bb8b9, 0x322f726f, 0x41b7ad2e, 0x670c79c1, 0x4c87d4ff, 0x56b48e4b,
     0x23458c9e, 0xd5036e6e, 0x85a308d3, 0x312fd607, 0x5e1b56df, 0x61498c2b},
    {0x73801945, 0x736a75d8, 0x6a22cc5b, 0x603f91a4, 0x07f6ec47, 0x729a1992,
     0xc6f55b32, 0x6fe77233, 0x0209b040, 0x32f3c46e, 0xf5a625eb, 0x9b70c4a3,
     0x839a0735, 0x799462c2, 0x04f17a49, 0xe89f7c55, 0x3b7e2839, 0x40946727,
     0x591e6df8, 0x555de64e, 0x6a6b0b49, 0x057d314f, 0x412d9c1b, 0x1f86d778,
     0xdcc75e35, 0xb88d7a70, 0x5384ee44, 0x44346de2, 0xa0fa6349, 0x8dd6f9a3}};

Blowfish::Blowfish(const unsigned char* key, int key_len) {
    LOG_F(INFO, "Initializing Blowfish with key length: %d", key_len);

    // Initialize P and S-boxes from constants
    memcpy(P_, P, sizeof(P));
    memcpy(S_, S, sizeof(S));

    // Key setup
    unsigned int data[2] = {0, 0};
    int key_index = 0;
    for (int i = 0; i < P_ARRAY_SIZE; ++i) {
        data[0] = (key[key_index] << 24) |
                  (key[(key_index + 1) % key_len] << 16) |
                  (key[(key_index + 2) % key_len] << 8) |
                  (key[(key_index + 3) % key_len]);
        P_[i] ^= data[0];
        key_index = (key_index + 4) % key_len;
    }

    // Initialize S-boxes with key-dependent values
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < S_BOX_SIZE; ++j) {
            data[0] = (key[key_index] << 24) |
                      (key[(key_index + 1) % key_len] << 16) |
                      (key[(key_index + 2) % key_len] << 8) |
                      (key[(key_index + 3) % key_len]);
            S_[i][j] ^= data[0];
            key_index = (key_index + 4) % key_len;
        }
    }

    LOG_F(INFO, "Blowfish initialization complete");
}

unsigned int Blowfish::F(unsigned int x) {
    unsigned char a = (x >> 24) & 0xFF;
    unsigned char b = (x >> 16) & 0xFF;
    unsigned char c = (x >> 8) & 0xFF;
    unsigned char d = x & 0xFF;

    return (S_[0][a] + S_[1][b]) ^ S_[2][c] + S_[3][d];
}

void Blowfish::encrypt(unsigned char* block) {
    LOG_F(INFO, "Encrypting block");

    unsigned int left =
        (block[0] << 24) | (block[1] << 16) | (block[2] << 8) | block[3];
    unsigned int right =
        (block[4] << 24) | (block[5] << 16) | (block[6] << 8) | block[7];

    left ^= P_[0];
    for (int i = 1; i <= 16; i += 2) {
        right ^= F(left) ^ P_[i];
        left ^= F(right) ^ P_[i + 1];
    }

    right ^= P_[17];

    block[0] = (right >> 24) & 0xFF;
    block[1] = (right >> 16) & 0xFF;
    block[2] = (right >> 8) & 0xFF;
    block[3] = right & 0xFF;
    block[4] = (left >> 24) & 0xFF;
    block[5] = (left >> 16) & 0xFF;
    block[6] = (left >> 8) & 0xFF;
    block[7] = left & 0xFF;

    LOG_F(INFO, "Block encrypted");
}

void Blowfish::decrypt(unsigned char* block) {
    LOG_F(INFO, "Decrypting block");

    unsigned int left =
        (block[0] << 24) | (block[1] << 16) | (block[2] << 8) | block[3];
    unsigned int right =
        (block[4] << 24) | (block[5] << 16) | (block[6] << 8) | block[7];

    left ^= P_[17];
    for (int i = 16; i >= 1; i -= 2) {
        right ^= F(left) ^ P_[i];
        left ^= F(right) ^ P_[i - 1];
    }

    right ^= P_[0];

    block[0] = (right >> 24) & 0xFF;
    block[1] = (right >> 16) & 0xFF;
    block[2] = (right >> 8) & 0xFF;
    block[3] = right & 0xFF;
    block[4] = (left >> 24) & 0xFF;
    block[5] = (left >> 16) & 0xFF;
    block[6] = (left >> 8) & 0xFF;
    block[7] = left & 0xFF;

    LOG_F(INFO, "Block decrypted");
}

void Blowfish::pkcs7_padding(unsigned char* data, size_t& length,
                             size_t block_size) {
    LOG_F(INFO, "Applying PKCS7 padding");

    size_t padding_len = block_size - (length % block_size);
    for (size_t i = length; i < length + padding_len; ++i) {
        data[i] = (unsigned char)padding_len;
    }
    length += padding_len;

    LOG_F(INFO, "Padding applied, new length: %zu", length);
}

void Blowfish::remove_padding(unsigned char* data, size_t& length) {
    LOG_F(INFO, "Removing PKCS7 padding");

    if (length == 0)
        return;
    size_t padding_len = data[length - 1];
    if (padding_len > 8) {
        LOG_F(ERROR, "Invalid padding length: %zu", padding_len);
        THROW_RUNTIME_ERROR("Invalid padding length.");
    }
    length -= padding_len;
    memset(data + length, 0, padding_len);

    LOG_F(INFO, "Padding removed, new length: %zu", length);
}

void Blowfish::encrypt_data(unsigned char* data, size_t& length,
                            size_t block_size) {
    LOG_F(INFO, "Encrypting data of length: %zu", length);

    pkcs7_padding(data, length, block_size);
    for (size_t i = 0; i < length; i += block_size) {
        encrypt(data + i);
    }

    LOG_F(INFO, "Data encrypted, new length: %zu", length);
}

void Blowfish::decrypt_data(unsigned char* data, size_t& length,
                            size_t block_size) {
    LOG_F(INFO, "Decrypting data of length: %zu", length);

    for (size_t i = 0; i < length; i += block_size) {
        decrypt(data + i);
    }
    remove_padding(data, length);

    LOG_F(INFO, "Data decrypted, new length: %zu", length);
}

void Blowfish::encrypt_file(const std::string& input_file,
                            const std::string& output_file) {
    LOG_F(INFO, "Encrypting file: {}", input_file);

    std::ifstream infile(input_file, std::ios::binary | std::ios::ate);
    if (!infile) {
        LOG_F(ERROR, "Failed to open input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to open input file for reading.");
    }
    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile) {
        LOG_F(ERROR, "Failed to open output file: {}", output_file);
        THROW_RUNTIME_ERROR("Failed to open output file for writing.");
    }

    std::vector<unsigned char> buffer(size);
    if (!infile.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOG_F(ERROR, "Failed to read input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to read input file.");
    }

    size_t length = buffer.size();
    encrypt_data(buffer.data(), length, 8);
    outfile.write(reinterpret_cast<char*>(buffer.data()), length);

    infile.close();
    outfile.close();

    LOG_F(INFO, "File encrypted: {}", output_file);
}

void Blowfish::decrypt_file(const std::string& input_file,
                            const std::string& output_file) {
    LOG_F(INFO, "Decrypting file: {}", input_file);

    std::ifstream infile(input_file, std::ios::binary | std::ios::ate);
    if (!infile) {
        LOG_F(ERROR, "Failed to open input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to open input file for reading.");
    }
    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile) {
        LOG_F(ERROR, "Failed to open output file: {}", output_file);
        THROW_RUNTIME_ERROR("Failed to open output file for writing.");
    }

    std::vector<unsigned char> buffer(size);
    if (!infile.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOG_F(ERROR, "Failed to read input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to read input file.");
    }

    size_t length = buffer.size();
    decrypt_data(buffer.data(), length, 8);
    outfile.write(reinterpret_cast<char*>(buffer.data()), length);

    infile.close();
    outfile.close();

    LOG_F(INFO, "File decrypted: {}", output_file);
}

}  // namespace atom::algorithm