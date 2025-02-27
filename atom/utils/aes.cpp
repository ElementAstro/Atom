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

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"

namespace atom::utils {

// RAII wrapper for EVP_CIPHER_CTX
class CipherContext {
public:
    CipherContext() : ctx_(EVP_CIPHER_CTX_new()) {
        if (!ctx_) {
            THROW_RUNTIME_ERROR("Failed to create EVP_CIPHER_CTX");
        }
    }

    ~CipherContext() noexcept {
        if (ctx_) {
            EVP_CIPHER_CTX_free(ctx_);
        }
    }

    EVP_CIPHER_CTX* get() const noexcept { return ctx_; }

    // Prevent copying
    CipherContext(const CipherContext&) = delete;
    CipherContext& operator=(const CipherContext&) = delete;

private:
    EVP_CIPHER_CTX* ctx_;
};

// RAII wrapper for EVP_MD_CTX
class MessageDigestContext {
public:
    MessageDigestContext() : ctx_(EVP_MD_CTX_new()) {
        if (!ctx_) {
            THROW_RUNTIME_ERROR("Failed to create EVP_MD_CTX");
        }
    }

    ~MessageDigestContext() noexcept {
        if (ctx_) {
            EVP_MD_CTX_free(ctx_);
        }
    }

    EVP_MD_CTX* get() const noexcept { return ctx_; }

    // Prevent copying
    MessageDigestContext(const MessageDigestContext&) = delete;
    MessageDigestContext& operator=(const MessageDigestContext&) = delete;

private:
    EVP_MD_CTX* ctx_;
};

auto encryptAES(StringLike auto&& plaintext_arg, StringLike auto&& key_arg,
                std::vector<unsigned char>& iv,
                std::vector<unsigned char>& tag) -> std::string {
    std::string_view plaintext = plaintext_arg;
    std::string_view key = key_arg;

    LOG_F(INFO, "Starting AES encryption");

    // Input validation
    if (plaintext.empty()) {
        LOG_F(ERROR, "Plaintext is empty");
        THROW_INVALID_ARGUMENT("Plaintext cannot be empty");
    }

    if (key.empty() || key.length() < 16) {
        LOG_F(ERROR, "Key is invalid (must be at least 16 bytes)");
        THROW_INVALID_ARGUMENT("Key is invalid (must be at least 16 bytes)");
    }

    try {
        CipherContext ctx;

        // Prepare IV (12 bytes for GCM is recommended)
        iv.resize(12);
        if (RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate IV");
            THROW_RUNTIME_ERROR("Failed to generate IV");
        }

        // Initialize encryption context
        if (EVP_EncryptInit_ex(
                ctx.get(), EVP_aes_256_gcm(), nullptr,
                reinterpret_cast<const unsigned char*>(key.data()),
                iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize encryption context");
            THROW_RUNTIME_ERROR("Failed to initialize encryption context");
        }

        // Allocate buffer for ciphertext with proper size
        std::vector<unsigned char> ciphertext(plaintext.length() +
                                              EVP_MAX_BLOCK_LENGTH);

        int len;
        if (EVP_EncryptUpdate(
                ctx.get(), ciphertext.data(), &len,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                static_cast<int>(plaintext.length())) != 1) {
            LOG_F(ERROR, "Encryption failed");
            THROW_RUNTIME_ERROR("Encryption failed");
        }

        int ciphertextLen = len;

        if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) !=
            1) {
            LOG_F(ERROR, "Final encryption step failed");
            THROW_RUNTIME_ERROR("Final encryption step failed");
        }

        ciphertextLen += len;

        // Get authentication tag
        tag.resize(16);  // GCM tag is 16 bytes
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                                static_cast<int>(tag.size()),
                                tag.data()) != 1) {
            LOG_F(ERROR, "Failed to get tag");
            THROW_RUNTIME_ERROR("Failed to get tag");
        }

        LOG_F(INFO, "AES encryption completed successfully");
        return std::string(ciphertext.begin(),
                           ciphertext.begin() + ciphertextLen);
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in encryptAES: %s", ex.what());
        throw;
    }
}

auto decryptAES(StringLike auto&& ciphertext_arg, StringLike auto&& key_arg,
                std::span<const unsigned char> iv,
                std::span<const unsigned char> tag) -> std::string {
    std::string_view ciphertext = ciphertext_arg;
    std::string_view key = key_arg;

    LOG_F(INFO, "Starting AES decryption");

    // Input validation
    if (ciphertext.empty()) {
        LOG_F(ERROR, "Ciphertext is empty");
        THROW_INVALID_ARGUMENT("Ciphertext cannot be empty");
    }

    if (key.empty() || key.length() < 16) {
        LOG_F(ERROR, "Key is invalid (must be at least 16 bytes)");
        THROW_INVALID_ARGUMENT("Key is invalid (must be at least 16 bytes)");
    }

    if (iv.size() != 12) {
        LOG_F(ERROR, "IV size is invalid (must be 12 bytes)");
        THROW_INVALID_ARGUMENT("IV size is invalid (must be 12 bytes)");
    }

    if (tag.size() != 16) {
        LOG_F(ERROR, "Tag size is invalid (must be 16 bytes)");
        THROW_INVALID_ARGUMENT("Tag size is invalid (must be 16 bytes)");
    }

    try {
        CipherContext ctx;

        if (EVP_DecryptInit_ex(
                ctx.get(), EVP_aes_256_gcm(), nullptr,
                reinterpret_cast<const unsigned char*>(key.data()),
                iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize decryption context");
            THROW_RUNTIME_ERROR("Failed to initialize decryption context");
        }

        // Allocate buffer for plaintext
        std::vector<unsigned char> plaintext(ciphertext.length() +
                                             EVP_MAX_BLOCK_LENGTH);

        int len;
        if (EVP_DecryptUpdate(
                ctx.get(), plaintext.data(), &len,
                reinterpret_cast<const unsigned char*>(ciphertext.data()),
                static_cast<int>(ciphertext.length())) != 1) {
            LOG_F(ERROR, "Decryption failed");
            THROW_RUNTIME_ERROR("Decryption failed");
        }

        int plaintextLen = len;

        // Set expected tag value
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
                                static_cast<int>(tag.size()),
                                const_cast<unsigned char*>(tag.data())) != 1) {
            LOG_F(ERROR, "Failed to set tag");
            THROW_RUNTIME_ERROR("Failed to set tag");
        }

        // Verify the tag and finalize decryption
        int ret = EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len);
        if (ret <= 0) {
            LOG_F(ERROR,
                  "Authentication failed or final decryption step failed");
            THROW_RUNTIME_ERROR(
                "Authentication failed or final decryption step failed");
        }

        plaintextLen += len;

        LOG_F(INFO, "AES decryption completed successfully");
        return std::string(plaintext.begin(), plaintext.begin() + plaintextLen);
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in decryptAES: %s", ex.what());
        throw;
    }
}

auto compress(StringLike auto&& data_arg) -> std::string {
    std::string_view data = data_arg;
    LOG_F(INFO, "Starting compression");

    if (data.empty()) {
        LOG_F(ERROR, "Input data is empty");
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    try {
        z_stream zstream{};
        if (deflateInit(&zstream, Z_BEST_COMPRESSION) != Z_OK) {
            LOG_F(ERROR, "Failed to initialize compression");
            THROW_RUNTIME_ERROR("Failed to initialize compression.");
        }

        // RAII cleanup for zlib stream
        auto cleanupDeflate = [&zstream]() noexcept { deflateEnd(&zstream); };
        std::unique_ptr<z_stream, decltype(cleanupDeflate)> streamGuard(
            &zstream, cleanupDeflate);

        zstream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zstream.avail_in = static_cast<uInt>(data.size());

        int ret;
        constexpr size_t BUFFER_SIZE = 32768;
        std::array<char, BUFFER_SIZE> outbuffer{};
        std::string compressed;
        compressed.reserve(
            data.size());  // Pre-allocate to avoid frequent reallocations

        do {
            zstream.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
            zstream.avail_out = outbuffer.size();

            ret = deflate(&zstream, Z_FINISH);
            if (ret == Z_STREAM_ERROR) {
                LOG_F(ERROR, "Compression error during deflation");
                THROW_RUNTIME_ERROR("Compression error during deflation.");
            }

            // Append the output to the compressed string
            compressed.append(outbuffer.data(),
                              outbuffer.size() - zstream.avail_out);
        } while (ret == Z_OK);

        if (ret != Z_STREAM_END) {
            LOG_F(ERROR, "Compression did not finish successfully");
            THROW_RUNTIME_ERROR("Compression did not finish successfully.");
        }

        LOG_F(INFO,
              "Compression completed successfully: %zu bytes -> %zu bytes",
              data.size(), compressed.size());
        return compressed;
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in compress: %s", ex.what());
        throw;
    }
}

auto decompress(StringLike auto&& data_arg) -> std::string {
    std::string_view data = data_arg;
    LOG_F(INFO, "Starting decompression");

    if (data.empty()) {
        LOG_F(ERROR, "Input data is empty");
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    try {
        z_stream zstream{};
        if (inflateInit(&zstream) != Z_OK) {
            LOG_F(ERROR, "Failed to initialize decompression");
            THROW_RUNTIME_ERROR("Failed to initialize decompression.");
        }

        // RAII cleanup for zlib stream
        auto cleanupInflate = [&zstream]() noexcept { inflateEnd(&zstream); };
        std::unique_ptr<z_stream, decltype(cleanupInflate)> streamGuard(
            &zstream, cleanupInflate);

        zstream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        zstream.avail_in = static_cast<uInt>(data.size());

        int ret;
        constexpr size_t BUFFER_SIZE = 32768;
        std::array<char, BUFFER_SIZE> outbuffer{};
        std::string decompressed;
        decompressed.reserve(data.size() * 2);  // Estimate decompressed size

        do {
            zstream.next_out = reinterpret_cast<Bytef*>(outbuffer.data());
            zstream.avail_out = outbuffer.size();

            ret = inflate(&zstream, Z_NO_FLUSH);
            if (ret < 0) {
                LOG_F(ERROR, "Decompression error during inflation: %d", ret);
                THROW_RUNTIME_ERROR("Decompression error during inflation.");
            }

            // Append the output to the decompressed string
            decompressed.append(outbuffer.data(),
                                outbuffer.size() - zstream.avail_out);

            // Check if we need to expand buffer for large decompressed data
            if (ret == Z_OK && zstream.avail_out == 0) {
                decompressed.reserve(decompressed.capacity() * 2);
            }
        } while (ret == Z_OK);

        if (ret != Z_STREAM_END) {
            LOG_F(ERROR, "Decompression did not finish successfully");
            THROW_RUNTIME_ERROR("Decompression did not finish successfully.");
        }

        LOG_F(INFO,
              "Decompression completed successfully: %zu bytes -> %zu bytes",
              data.size(), decompressed.size());
        return decompressed;
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in decompress: %s", ex.what());
        throw;
    }
}

auto calculateSha256(StringLike auto&& filename_arg) -> std::string {
    std::string_view filename = filename_arg;
    LOG_F(INFO, "Calculating SHA-256 for file: %s", filename.data());

    try {
        if (filename.empty()) {
            LOG_F(ERROR, "Filename is empty");
            THROW_INVALID_ARGUMENT("Filename cannot be empty");
        }

        if (!atom::io::isFileExists(std::string(filename))) {
            LOG_F(ERROR, "File does not exist: %s", filename.data());
            return "";
        }

        std::ifstream file(filename.data(), std::ios::binary);
        if (!file || !file.good()) {
            LOG_F(ERROR, "Failed to open file: %s", filename.data());
            THROW_RUNTIME_ERROR("Failed to open file");
        }

        MessageDigestContext mdctx;

        if (EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr) != 1) {
            LOG_F(ERROR, "Failed to initialize digest context");
            THROW_RUNTIME_ERROR("Failed to initialize digest context");
        }

        constexpr size_t BUFFER_SIZE =
            16384;  // Increased buffer size for performance
        std::vector<char> buffer(BUFFER_SIZE);

        // Process file in chunks
        while (file.read(buffer.data(), buffer.size())) {
            if (EVP_DigestUpdate(mdctx.get(), buffer.data(), buffer.size()) !=
                1) {
                LOG_F(ERROR, "Failed to update digest");
                THROW_RUNTIME_ERROR("Failed to update digest");
            }
        }

        if (file.gcount() > 0) {
            if (EVP_DigestUpdate(mdctx.get(), buffer.data(), file.gcount()) !=
                1) {
                LOG_F(ERROR, "Failed to update digest with remaining data");
                THROW_RUNTIME_ERROR(
                    "Failed to update digest with remaining data");
            }
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int hashLen = 0;
        if (EVP_DigestFinal_ex(mdctx.get(), hash.data(), &hashLen) != 1) {
            LOG_F(ERROR, "Failed to finalize digest");
            THROW_RUNTIME_ERROR("Failed to finalize digest");
        }

        // Convert to hexadecimal string
        std::ostringstream sha256Val;
        sha256Val << std::hex << std::setfill('0');

        for (unsigned int i = 0; i < hashLen; ++i) {
            sha256Val << std::setw(2) << static_cast<int>(hash[i]);
        }

        LOG_F(INFO, "SHA-256 calculation completed successfully");
        return sha256Val.str();
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in calculateSha256: %s", ex.what());
        throw;
    }
}

auto calculateHash(const std::string& data,
                   const EVP_MD* (*hashFunction)()) noexcept -> std::string {
    try {
        if (data.empty()) {
            LOG_F(WARNING, "Empty data provided for hash calculation");
            return "";
        }

        LOG_F(INFO, "Calculating hash using custom hash function");

        MessageDigestContext context;
        const EVP_MD* messageDigest = hashFunction();

        if (!messageDigest) {
            LOG_F(ERROR, "Invalid hash function");
            return "";
        }

        if (EVP_DigestInit_ex(context.get(), messageDigest, nullptr) != 1) {
            LOG_F(ERROR, "Failed to initialize digest");
            return "";
        }

        if (EVP_DigestUpdate(context.get(), data.c_str(), data.size()) != 1) {
            LOG_F(ERROR, "Failed to update digest");
            return "";
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int lengthOfHash = 0;

        if (EVP_DigestFinal_ex(context.get(), hash.data(), &lengthOfHash) !=
            1) {
            LOG_F(ERROR, "Failed to finalize digest");
            return "";
        }

        std::stringstream stringStream;
        stringStream << std::hex << std::setfill('0');

        for (unsigned int i = 0; i < lengthOfHash; ++i) {
            stringStream << std::setw(2) << static_cast<int>(hash[i]);
        }

        LOG_F(INFO, "Hash calculation completed successfully");
        return stringStream.str();
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception in calculateHash: %s", ex.what());
        return "";
    }
}

auto calculateSha224(const std::string& data) noexcept -> std::string {
    LOG_F(INFO, "Calculating SHA-224 hash");
    return calculateHash(data, EVP_sha224);
}

auto calculateSha384(const std::string& data) noexcept -> std::string {
    LOG_F(INFO, "Calculating SHA-384 hash");
    return calculateHash(data, EVP_sha384);
}

auto calculateSha512(const std::string& data) noexcept -> std::string {
    LOG_F(INFO, "Calculating SHA-512 hash");
    return calculateHash(data, EVP_sha512);
}

}  // namespace atom::utils
