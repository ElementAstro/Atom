/*
 * fifo_codec.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Compression and encryption utilities for FIFO operations

*************************************************/

#include "fifo_codec.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

#ifdef ENABLE_COMPRESSION
#include <zlib.h>
#endif

#ifdef ENABLE_ENCRYPTION
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

namespace atom::connection {

std::string FifoCodec::defaultKey_ = "atom_fifo_default_key_2024";

auto FifoCodec::compress(const std::string& data, size_t threshold)
    -> FifoResult<std::string> {
#ifdef ENABLE_COMPRESSION
    if (data.empty() || (threshold > 0 && data.size() < threshold)) {
        return data;
    }

    z_stream zs{};
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return type::unexpected(make_error_code(FifoError::CompressionFailed));
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    std::string compressed;
    compressed.resize(compressBound(data.size()));

    zs.next_out = reinterpret_cast<Bytef*>(compressed.data());
    zs.avail_out = static_cast<uInt>(compressed.size());

    int result = deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    if (result != Z_STREAM_END) {
        return type::unexpected(make_error_code(FifoError::CompressionFailed));
    }

    compressed.resize(zs.total_out);

    // Only use compressed version if it's actually smaller
    if (compressed.size() >= data.size()) {
        return data;
    }

    // Prepend marker and original size for decompression
    std::string result_data;
    result_data.reserve(1 + sizeof(uint32_t) + compressed.size());
    result_data.push_back(static_cast<char>(MARKER_COMPRESSED));

    uint32_t original_size = static_cast<uint32_t>(data.size());
    result_data.append(reinterpret_cast<const char*>(&original_size),
                       sizeof(original_size));
    result_data.append(compressed);

    return result_data;
#else
    (void)threshold;
    return data;
#endif
}

auto FifoCodec::decompress(const std::string& data) -> FifoResult<std::string> {
#ifdef ENABLE_COMPRESSION
    if (data.empty()) {
        return data;
    }

    // Check if data is compressed (has marker)
    if (static_cast<uint8_t>(data[0]) != MARKER_COMPRESSED) {
        return data;  // Not compressed, return as-is
    }

    if (data.size() < 1 + sizeof(uint32_t)) {
        return type::unexpected(
            make_error_code(FifoError::DecompressionFailed));
    }

    // Extract original size
    uint32_t original_size;
    std::memcpy(&original_size, data.data() + 1, sizeof(original_size));

    // Sanity check on size
    if (original_size > 100 * 1024 * 1024) {  // 100MB limit
        return type::unexpected(
            make_error_code(FifoError::DecompressionFailed));
    }

    std::string decompressed;
    decompressed.resize(original_size);

    z_stream zs{};
    if (inflateInit(&zs) != Z_OK) {
        return type::unexpected(
            make_error_code(FifoError::DecompressionFailed));
    }

    zs.next_in = reinterpret_cast<Bytef*>(
        const_cast<char*>(data.data() + 1 + sizeof(uint32_t)));
    zs.avail_in = static_cast<uInt>(data.size() - 1 - sizeof(uint32_t));
    zs.next_out = reinterpret_cast<Bytef*>(decompressed.data());
    zs.avail_out = static_cast<uInt>(decompressed.size());

    int result = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);

    if (result != Z_STREAM_END) {
        return type::unexpected(
            make_error_code(FifoError::DecompressionFailed));
    }

    decompressed.resize(zs.total_out);
    return decompressed;
#else
    return data;
#endif
}

auto FifoCodec::encrypt(const std::string& data, const std::string& key)
    -> FifoResult<std::string> {
#ifdef ENABLE_ENCRYPTION
    if (data.empty()) {
        return data;
    }

    // Derive a 256-bit key using SHA-256
    std::array<unsigned char, 32> derived_key;
    SHA256(reinterpret_cast<const unsigned char*>(key.data()), key.size(),
           derived_key.data());

    // Generate random IV
    std::array<unsigned char, 12> iv;  // GCM uses 12-byte IV
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }

    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, derived_key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }

    std::string ciphertext;
    ciphertext.resize(data.size() + 16);  // Extra space for padding

    int len = 0;
    int ciphertext_len = 0;

    if (EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()),
                          &len, reinterpret_cast<const unsigned char*>(data.data()),
                          static_cast<int>(data.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(
            ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len,
            &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    // Get the authentication tag
    std::array<unsigned char, 16> tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::EncryptionFailed));
    }

    EVP_CIPHER_CTX_free(ctx);

    // Build result: marker + IV + tag + ciphertext
    std::string result;
    result.reserve(1 + iv.size() + tag.size() + ciphertext.size());
    result.push_back(static_cast<char>(MARKER_ENCRYPTED));
    result.append(reinterpret_cast<const char*>(iv.data()), iv.size());
    result.append(reinterpret_cast<const char*>(tag.data()), tag.size());
    result.append(ciphertext);

    return result;
#else
    // Fallback: simple XOR encryption (NOT secure, for compatibility only)
    if (data.empty()) {
        return data;
    }

    std::string encrypted = data;
    for (size_t i = 0; i < encrypted.size(); ++i) {
        encrypted[i] ^= key[i % key.size()];
    }

    std::string result;
    result.reserve(1 + encrypted.size());
    result.push_back(static_cast<char>(MARKER_ENCRYPTED));
    result.append(encrypted);

    return result;
#endif
}

auto FifoCodec::decrypt(const std::string& data, const std::string& key)
    -> FifoResult<std::string> {
#ifdef ENABLE_ENCRYPTION
    if (data.empty()) {
        return data;
    }

    // Check if data is encrypted
    if (static_cast<uint8_t>(data[0]) != MARKER_ENCRYPTED) {
        return data;  // Not encrypted, return as-is
    }

    constexpr size_t IV_SIZE = 12;
    constexpr size_t TAG_SIZE = 16;
    constexpr size_t HEADER_SIZE = 1 + IV_SIZE + TAG_SIZE;

    if (data.size() < HEADER_SIZE) {
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }

    // Derive key
    std::array<unsigned char, 32> derived_key;
    SHA256(reinterpret_cast<const unsigned char*>(key.data()), key.size(),
           derived_key.data());

    // Extract IV and tag
    std::array<unsigned char, IV_SIZE> iv;
    std::array<unsigned char, TAG_SIZE> tag;
    std::memcpy(iv.data(), data.data() + 1, IV_SIZE);
    std::memcpy(tag.data(), data.data() + 1 + IV_SIZE, TAG_SIZE);

    const char* ciphertext = data.data() + HEADER_SIZE;
    size_t ciphertext_len = data.size() - HEADER_SIZE;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, derived_key.data(),
                           iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }

    std::string plaintext;
    plaintext.resize(ciphertext_len);

    int len = 0;
    int plaintext_len = 0;

    if (EVP_DecryptUpdate(
            ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
            reinterpret_cast<const unsigned char*>(ciphertext),
            static_cast<int>(ciphertext_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }
    plaintext_len = len;

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                            const_cast<unsigned char*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }

    // Verify tag and finalize
    int ret = EVP_DecryptFinal_ex(
        ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) {
        return type::unexpected(make_error_code(FifoError::DecryptionFailed));
    }
    plaintext_len += len;
    plaintext.resize(plaintext_len);

    return plaintext;
#else
    // Fallback: simple XOR decryption
    if (data.empty()) {
        return data;
    }

    if (static_cast<uint8_t>(data[0]) != MARKER_ENCRYPTED) {
        return data;
    }

    std::string decrypted(data.begin() + 1, data.end());
    for (size_t i = 0; i < decrypted.size(); ++i) {
        decrypted[i] ^= key[i % key.size()];
    }

    return decrypted;
#endif
}

auto FifoCodec::encode(const std::string& data, bool enable_compression,
                       size_t compression_threshold, bool enable_encryption,
                       const std::string& encryption_key)
    -> FifoResult<std::string> {
    std::string result = data;

    // First compress (if enabled)
    if (enable_compression) {
        auto compressed = compress(result, compression_threshold);
        if (!compressed) {
            return compressed;
        }
        result = std::move(*compressed);
    }

    // Then encrypt (if enabled)
    if (enable_encryption) {
        auto encrypted = encrypt(result, encryption_key);
        if (!encrypted) {
            return encrypted;
        }
        result = std::move(*encrypted);
    }

    return result;
}

auto FifoCodec::decode(const std::string& data, bool enable_encryption,
                       const std::string& encryption_key,
                       bool enable_compression) -> FifoResult<std::string> {
    std::string result = data;

    // First decrypt (if enabled)
    if (enable_encryption) {
        auto decrypted = decrypt(result, encryption_key);
        if (!decrypted) {
            return decrypted;
        }
        result = std::move(*decrypted);
    }

    // Then decompress (if enabled)
    if (enable_compression) {
        auto decompressed = decompress(result);
        if (!decompressed) {
            return decompressed;
        }
        result = std::move(*decompressed);
    }

    return result;
}

auto FifoCodec::isCompressionAvailable() noexcept -> bool {
#ifdef ENABLE_COMPRESSION
    return true;
#else
    return false;
#endif
}

auto FifoCodec::isEncryptionAvailable() noexcept -> bool {
#ifdef ENABLE_ENCRYPTION
    return true;
#else
    return false;
#endif
}

auto FifoCodec::getDefaultKey() -> const std::string& { return defaultKey_; }

void FifoCodec::setDefaultKey(const std::string& key) { defaultKey_ = key; }

}  // namespace atom::connection
