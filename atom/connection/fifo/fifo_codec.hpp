/*
 * fifo_codec.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Compression and encryption utilities for FIFO operations

*************************************************/

#ifndef ATOM_CONNECTION_FIFO_CODEC_HPP
#define ATOM_CONNECTION_FIFO_CODEC_HPP

#include <cstdint>
#include <string>

#include "fifo_common.hpp"

namespace atom::connection {

/**
 * @brief Utility class for data compression and encryption in FIFO operations
 *
 * This class provides static methods for compressing/decompressing and
 * encrypting/decrypting data. It supports conditional compilation for
 * optional dependencies (zlib, OpenSSL).
 */
class FifoCodec {
public:
    /**
     * @brief Compress data using zlib
     *
     * @param data Input data to compress
     * @param threshold Minimum size for compression (smaller data passed
     * through)
     * @return FifoResult<std::string> Compressed data or error
     */
    static auto compress(const std::string& data, size_t threshold = 0)
        -> FifoResult<std::string>;

    /**
     * @brief Decompress data compressed with compress()
     *
     * @param data Compressed data
     * @return FifoResult<std::string> Decompressed data or error
     */
    static auto decompress(const std::string& data) -> FifoResult<std::string>;

    /**
     * @brief Encrypt data using AES-256-GCM
     *
     * @param data Input data to encrypt
     * @param key Encryption key (will be derived if not 32 bytes)
     * @return FifoResult<std::string> Encrypted data with IV prepended or error
     */
    static auto encrypt(const std::string& data,
                        const std::string& key = getDefaultKey())
        -> FifoResult<std::string>;

    /**
     * @brief Decrypt data encrypted with encrypt()
     *
     * @param data Encrypted data (with IV prepended)
     * @param key Encryption key (must match the one used for encryption)
     * @return FifoResult<std::string> Decrypted data or error
     */
    static auto decrypt(const std::string& data,
                        const std::string& key = getDefaultKey())
        -> FifoResult<std::string>;

    /**
     * @brief Process outgoing data (compress then encrypt if enabled)
     *
     * @param data Input data
     * @param enable_compression Whether to compress
     * @param compression_threshold Minimum size for compression
     * @param enable_encryption Whether to encrypt
     * @param encryption_key Key for encryption
     * @return FifoResult<std::string> Processed data or error
     */
    static auto encode(const std::string& data, bool enable_compression = false,
                       size_t compression_threshold = 1024,
                       bool enable_encryption = false,
                       const std::string& encryption_key = getDefaultKey())
        -> FifoResult<std::string>;

    /**
     * @brief Process incoming data (decrypt then decompress if enabled)
     *
     * @param data Encoded data
     * @param enable_encryption Whether data is encrypted
     * @param encryption_key Key for decryption
     * @param enable_compression Whether data might be compressed
     * @return FifoResult<std::string> Decoded data or error
     */
    static auto decode(const std::string& data, bool enable_encryption = false,
                       const std::string& encryption_key = getDefaultKey(),
                       bool enable_compression = false)
        -> FifoResult<std::string>;

    /**
     * @brief Check if compression is available
     */
    static auto isCompressionAvailable() noexcept -> bool;

    /**
     * @brief Check if encryption is available
     */
    static auto isEncryptionAvailable() noexcept -> bool;

    /**
     * @brief Get the default encryption key
     */
    static auto getDefaultKey() -> const std::string&;

    /**
     * @brief Set the default encryption key
     */
    static void setDefaultKey(const std::string& key);

private:
    // Marker bytes for encoded data
    static constexpr uint8_t MARKER_COMPRESSED = 0x01;
    static constexpr uint8_t MARKER_ENCRYPTED = 0x02;
    static constexpr uint8_t MARKER_BOTH = 0x03;

    // Default key storage
    static std::string defaultKey_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFO_CODEC_HPP
