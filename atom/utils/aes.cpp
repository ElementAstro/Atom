/*
 * aes.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Simple implementation of AES encryption

**************************************************/

#include "aes.hpp"

#include <cstring>
#include <fstream>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"

namespace atom::utils {
auto encryptAES(std::string_view plaintext, std::string_view key,
                std::vector<unsigned char> &iv,
                std::vector<unsigned char> &tag) -> std::string {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        THROW_RUNTIME_ERROR("Failed to create EVP_CIPHER_CTX");
    }

    iv.resize(12);  // GCM recommends a 12-byte IV
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to generate IV");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to initialize encryption context");
    }

    // Allocate buffer for ciphertext
    std::vector<unsigned char> ciphertext(plaintext.length() +
                                          EVP_MAX_BLOCK_LENGTH);

    int len;
    if (EVP_EncryptUpdate(
            ctx, ciphertext.data(), &len,
            reinterpret_cast<const unsigned char *>(plaintext.data()),
            plaintext.length()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Encryption failed");
    }

    int ciphertextLen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Final encryption step failed");
    }

    ciphertextLen += len;

    tag.resize(16);  // GCM tag is 16 bytes
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(),
                            tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to get tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    return std::string(ciphertext.begin(), ciphertext.begin() + ciphertextLen);
}

auto decryptAES(std::string_view ciphertext, std::string_view key,
                std::vector<unsigned char> &iv,
                std::vector<unsigned char> &tag) -> std::string {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        THROW_RUNTIME_ERROR("Failed to create EVP_CIPHER_CTX");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to initialize decryption context");
    }

    // Allocate buffer for plaintext
    std::vector<unsigned char> plaintext(ciphertext.length() +
                                         EVP_MAX_BLOCK_LENGTH);

    int len;
    if (EVP_DecryptUpdate(
            ctx, plaintext.data(), &len,
            reinterpret_cast<const unsigned char *>(ciphertext.data()),
            ciphertext.length()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Decryption failed");
    }

    int plaintextLen = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(),
                            tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Failed to set tag");
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        THROW_RUNTIME_ERROR("Final decryption step failed");
    }

    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    return std::string(plaintext.begin(), plaintext.begin() + plaintextLen);
}

auto compress(std::string_view data) -> std::string {
    if (data.empty()) {
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    z_stream zstream{};
    if (deflateInit(&zstream, Z_BEST_COMPRESSION) != Z_OK) {
        THROW_RUNTIME_ERROR("Failed to initialize compression.");
    }

    zstream.next_in =
        reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
    zstream.avail_in = data.size();

    int ret;
    constexpr size_t BUFFER_SIZE = 32768;
    std::array<char, BUFFER_SIZE> outbuffer{};
    std::string compressed;

    do {
        zstream.next_out = reinterpret_cast<Bytef *>(outbuffer.data());
        zstream.avail_out = outbuffer.size();

        ret = deflate(&zstream, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zstream);
            THROW_RUNTIME_ERROR("Compression error during deflation.");
        }

        // Append the output to the compressed string
        compressed.append(outbuffer.data(),
                          outbuffer.size() - zstream.avail_out);
    } while (ret == Z_OK);

    deflateEnd(&zstream);

    if (ret != Z_STREAM_END) {
        THROW_RUNTIME_ERROR("Compression did not finish successfully.");
    }

    return compressed;
}

auto decompress(std::string_view data) -> std::string {
    if (data.empty()) {
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    z_stream zstream{};
    if (inflateInit(&zstream) != Z_OK) {
        THROW_RUNTIME_ERROR("Failed to initialize decompression.");
    }

    zstream.next_in =
        reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
    zstream.avail_in = data.size();

    int ret;
    constexpr size_t BUFFER_SIZE = 32768;
    std::array<char, BUFFER_SIZE> outbuffer{};
    std::string decompressed;

    do {
        zstream.next_out = reinterpret_cast<Bytef *>(outbuffer.data());
        zstream.avail_out = outbuffer.size();

        ret = inflate(&zstream, Z_NO_FLUSH);
        if (ret < 0) {
            inflateEnd(&zstream);
            THROW_RUNTIME_ERROR("Decompression error during inflation.");
        }

        // Append the output to the decompressed string
        decompressed.append(outbuffer.data(),
                            outbuffer.size() - zstream.avail_out);
    } while (ret == Z_OK);

    inflateEnd(&zstream);

    if (ret != Z_STREAM_END) {
        THROW_RUNTIME_ERROR("Decompression did not finish successfully.");
    }

    return decompressed;
}

auto calculateSha256(std::string_view filename) -> std::string {
    if (!atom::io::isFileExists(std::string(filename))) {
        LOG_F(ERROR, "File not exist: {}", filename);
        return "";
    }

    std::ifstream file(filename.data(), std::ios::binary);
    if (!file || !file.good()) {
        return "";
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    constexpr size_t BUFFER_SIZE = 1024;
    std::array<char, BUFFER_SIZE> buffer{};
    while (file.read(buffer.data(), buffer.size())) {
        if (EVP_DigestUpdate(mdctx, buffer.data(), buffer.size()) != 1) {
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    if (file.gcount() > 0) {
        if (EVP_DigestUpdate(mdctx, buffer.data(), file.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash.data(), &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    // Convert to hexadecimal string
    std::ostringstream sha256Val;
    for (unsigned int i = 0; i < hashLen; ++i) {
        sha256Val << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(hash[i]);
    }

    return sha256Val.str();
}

auto calculateHash(const std::string &data,
                   const EVP_MD *(*hashFunction)()) -> std::string {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const EVP_MD *messageDigest = hashFunction();

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int lengthOfHash = 0;

    EVP_DigestInit_ex(context, messageDigest, nullptr);
    EVP_DigestUpdate(context, data.c_str(), data.size());
    EVP_DigestFinal_ex(context, hash.data(), &lengthOfHash);

    EVP_MD_CTX_free(context);

    std::stringstream stringStream;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        stringStream << std::hex << std::setw(2) << std::setfill('0')
                     << static_cast<int>(hash[i]);
    }
    return stringStream.str();
}

auto calculateSha224(const std::string &data) -> std::string {
    return calculateHash(data, EVP_sha224);
}

auto calculateSha384(const std::string &data) -> std::string {
    return calculateHash(data, EVP_sha384);
}

auto calculateSha512(const std::string &data) -> std::string {
    return calculateHash(data, EVP_sha512);
}
}  // namespace atom::utils
