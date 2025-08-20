/*
 * aes.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "aes.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#include <spdlog/spdlog.h>

#include "atom/error/exception.hpp"

namespace atom::utils {

constexpr size_t ZLIB_BUFFER_SIZE = 32768;
constexpr size_t FILE_BUFFER_SIZE = 16384;
constexpr size_t AES_IV_SIZE = 12;
constexpr size_t AES_TAG_SIZE = 16;
constexpr size_t MIN_KEY_SIZE = 16;

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

    CipherContext(const CipherContext&) = delete;
    CipherContext& operator=(const CipherContext&) = delete;
    CipherContext(CipherContext&&) = delete;
    CipherContext& operator=(CipherContext&&) = delete;

private:
    EVP_CIPHER_CTX* ctx_;
};

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

    MessageDigestContext(const MessageDigestContext&) = delete;
    MessageDigestContext& operator=(const MessageDigestContext&) = delete;
    MessageDigestContext(MessageDigestContext&&) = delete;
    MessageDigestContext& operator=(MessageDigestContext&&) = delete;

private:
    EVP_MD_CTX* ctx_;
};

class ZlibStream {
public:
    explicit ZlibStream(bool isInflate) : isInflate_(isInflate) {
        std::memset(&stream_, 0, sizeof(stream_));
        if (isInflate_) {
            if (inflateInit(&stream_) != Z_OK) {
                THROW_RUNTIME_ERROR("Failed to initialize decompression");
            }
        } else {
            if (deflateInit(&stream_, Z_BEST_COMPRESSION) != Z_OK) {
                THROW_RUNTIME_ERROR("Failed to initialize compression");
            }
        }
    }

    ~ZlibStream() noexcept {
        if (isInflate_) {
            inflateEnd(&stream_);
        } else {
            deflateEnd(&stream_);
        }
    }

    z_stream& get() noexcept { return stream_; }

    ZlibStream(const ZlibStream&) = delete;
    ZlibStream& operator=(const ZlibStream&) = delete;
    ZlibStream(ZlibStream&&) = delete;
    ZlibStream& operator=(ZlibStream&&) = delete;

private:
    z_stream stream_;
    bool isInflate_;
};

auto encryptAES(StringLike auto&& plaintext_arg, StringLike auto&& key_arg,
                std::vector<unsigned char>& iv, std::vector<unsigned char>& tag)
    -> std::string {
    const std::string_view plaintext = plaintext_arg;
    const std::string_view key = key_arg;

    spdlog::info("Starting AES encryption");

    if (plaintext.empty()) {
        spdlog::error("Plaintext is empty");
        THROW_INVALID_ARGUMENT("Plaintext cannot be empty");
    }

    if (key.empty() || key.length() < MIN_KEY_SIZE) {
        spdlog::error("Key is invalid (must be at least {} bytes)",
                      MIN_KEY_SIZE);
        THROW_INVALID_ARGUMENT("Key is invalid (must be at least 16 bytes)");
    }

    try {
        CipherContext ctx;

        iv.resize(AES_IV_SIZE);
        if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
            spdlog::error("Failed to generate IV");
            THROW_RUNTIME_ERROR("Failed to generate IV");
        }

        if (EVP_EncryptInit_ex(
                ctx.get(), EVP_aes_256_gcm(), nullptr,
                reinterpret_cast<const unsigned char*>(key.data()),
                iv.data()) != 1) {
            spdlog::error("Failed to initialize encryption context");
            THROW_RUNTIME_ERROR("Failed to initialize encryption context");
        }

        std::string ciphertext;
        ciphertext.reserve(plaintext.length() + EVP_MAX_BLOCK_LENGTH);

        std::vector<unsigned char> buffer(plaintext.length() +
                                          EVP_MAX_BLOCK_LENGTH);
        int len = 0;

        if (EVP_EncryptUpdate(
                ctx.get(), buffer.data(), &len,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                static_cast<int>(plaintext.length())) != 1) {
            spdlog::error("Encryption failed");
            THROW_RUNTIME_ERROR("Encryption failed");
        }

        int ciphertextLen = len;

        if (EVP_EncryptFinal_ex(ctx.get(), buffer.data() + len, &len) != 1) {
            spdlog::error("Final encryption step failed");
            THROW_RUNTIME_ERROR("Final encryption step failed");
        }

        ciphertextLen += len;

        tag.resize(AES_TAG_SIZE);
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
                                static_cast<int>(tag.size()),
                                tag.data()) != 1) {
            spdlog::error("Failed to get tag");
            THROW_RUNTIME_ERROR("Failed to get tag");
        }

        spdlog::info("AES encryption completed successfully");
        return std::string(buffer.begin(), buffer.begin() + ciphertextLen);
    } catch (const std::exception& ex) {
        spdlog::error("Exception in encryptAES: {}", ex.what());
        throw;
    }
}

auto decryptAES(StringLike auto&& ciphertext_arg, StringLike auto&& key_arg,
                std::span<const unsigned char> iv,
                std::span<const unsigned char> tag) -> std::string {
    const std::string_view ciphertext = ciphertext_arg;
    const std::string_view key = key_arg;

    spdlog::info("Starting AES decryption");

    if (ciphertext.empty()) {
        spdlog::error("Ciphertext is empty");
        THROW_INVALID_ARGUMENT("Ciphertext cannot be empty");
    }

    if (key.empty() || key.length() < MIN_KEY_SIZE) {
        spdlog::error("Key is invalid (must be at least {} bytes)",
                      MIN_KEY_SIZE);
        THROW_INVALID_ARGUMENT("Key is invalid (must be at least 16 bytes)");
    }

    if (iv.size() != AES_IV_SIZE) {
        spdlog::error("IV size is invalid (must be {} bytes)", AES_IV_SIZE);
        THROW_INVALID_ARGUMENT("IV size is invalid (must be 12 bytes)");
    }

    if (tag.size() != AES_TAG_SIZE) {
        spdlog::error("Tag size is invalid (must be {} bytes)", AES_TAG_SIZE);
        THROW_INVALID_ARGUMENT("Tag size is invalid (must be 16 bytes)");
    }

    try {
        CipherContext ctx;

        if (EVP_DecryptInit_ex(
                ctx.get(), EVP_aes_256_gcm(), nullptr,
                reinterpret_cast<const unsigned char*>(key.data()),
                iv.data()) != 1) {
            spdlog::error("Failed to initialize decryption context");
            THROW_RUNTIME_ERROR("Failed to initialize decryption context");
        }

        std::vector<unsigned char> buffer(ciphertext.length() +
                                          EVP_MAX_BLOCK_LENGTH);
        int len = 0;

        if (EVP_DecryptUpdate(
                ctx.get(), buffer.data(), &len,
                reinterpret_cast<const unsigned char*>(ciphertext.data()),
                static_cast<int>(ciphertext.length())) != 1) {
            spdlog::error("Decryption failed");
            THROW_RUNTIME_ERROR("Decryption failed");
        }

        int plaintextLen = len;

        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG,
                                static_cast<int>(tag.size()),
                                const_cast<unsigned char*>(tag.data())) != 1) {
            spdlog::error("Failed to set tag");
            THROW_RUNTIME_ERROR("Failed to set tag");
        }

        if (EVP_DecryptFinal_ex(ctx.get(), buffer.data() + len, &len) <= 0) {
            spdlog::error(
                "Authentication failed or final decryption step failed");
            THROW_RUNTIME_ERROR(
                "Authentication failed or final decryption step failed");
        }

        plaintextLen += len;

        spdlog::info("AES decryption completed successfully");
        return std::string(buffer.begin(), buffer.begin() + plaintextLen);
    } catch (const std::exception& ex) {
        spdlog::error("Exception in decryptAES: {}", ex.what());
        throw;
    }
}

auto compress(StringLike auto&& data_arg) -> std::string {
    const std::string_view data = data_arg;
    spdlog::info("Starting compression");

    if (data.empty()) {
        spdlog::error("Input data is empty");
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    try {
        ZlibStream zstream(false);
        auto& stream = zstream.get();

        stream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        stream.avail_in = static_cast<uInt>(data.size());

        std::string compressed;
        compressed.reserve(data.size() / 2);

        std::array<char, ZLIB_BUFFER_SIZE> buffer{};
        int ret;

        do {
            stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
            stream.avail_out = static_cast<uInt>(buffer.size());

            ret = deflate(&stream, Z_FINISH);
            if (ret == Z_STREAM_ERROR) {
                spdlog::error("Compression error during deflation");
                THROW_RUNTIME_ERROR("Compression error during deflation.");
            }

            compressed.append(buffer.data(), buffer.size() - stream.avail_out);
        } while (ret == Z_OK);

        if (ret != Z_STREAM_END) {
            spdlog::error("Compression did not finish successfully");
            THROW_RUNTIME_ERROR("Compression did not finish successfully.");
        }

        spdlog::info("Compression completed successfully: {} bytes -> {} bytes",
                     data.size(), compressed.size());
        return compressed;
    } catch (const std::exception& ex) {
        spdlog::error("Exception in compress: {}", ex.what());
        throw;
    }
}

auto decompress(StringLike auto&& data_arg) -> std::string {
    const std::string_view data = data_arg;
    spdlog::info("Starting decompression");

    if (data.empty()) {
        spdlog::error("Input data is empty");
        THROW_INVALID_ARGUMENT("Input data is empty.");
    }

    try {
        ZlibStream zstream(true);
        auto& stream = zstream.get();

        stream.next_in =
            reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
        stream.avail_in = static_cast<uInt>(data.size());

        std::string decompressed;
        decompressed.reserve(data.size() * 3);

        std::array<char, ZLIB_BUFFER_SIZE> buffer{};
        int ret;

        do {
            stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
            stream.avail_out = static_cast<uInt>(buffer.size());

            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret < 0) {
                spdlog::error("Decompression error during inflation: {}", ret);
                THROW_RUNTIME_ERROR("Decompression error during inflation.");
            }

            decompressed.append(buffer.data(),
                                buffer.size() - stream.avail_out);
        } while (ret == Z_OK);

        if (ret != Z_STREAM_END) {
            spdlog::error("Decompression did not finish successfully");
            THROW_RUNTIME_ERROR("Decompression did not finish successfully.");
        }

        spdlog::info(
            "Decompression completed successfully: {} bytes -> {} bytes",
            data.size(), decompressed.size());
        return decompressed;
    } catch (const std::exception& ex) {
        spdlog::error("Exception in decompress: {}", ex.what());
        throw;
    }
}

auto calculateSha256(StringLike auto&& filename_arg) -> std::string {
    const std::string_view filename = filename_arg;
    spdlog::info("Calculating SHA-256 for file: {}", filename);

    try {
        if (filename.empty()) {
            spdlog::error("Filename is empty");
            THROW_INVALID_ARGUMENT("Filename cannot be empty");
        }

        const std::filesystem::path filepath(filename);
        if (!std::filesystem::exists(filepath)) {
            spdlog::error("File does not exist: {}", filename);
            return "";
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file || !file.good()) {
            spdlog::error("Failed to open file: {}", filename);
            THROW_RUNTIME_ERROR("Failed to open file");
        }

        MessageDigestContext mdctx;

        if (EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr) != 1) {
            spdlog::error("Failed to initialize digest context");
            THROW_RUNTIME_ERROR("Failed to initialize digest context");
        }

        std::array<char, FILE_BUFFER_SIZE> buffer{};

        while (file.read(buffer.data(), buffer.size())) {
            if (EVP_DigestUpdate(mdctx.get(), buffer.data(), buffer.size()) !=
                1) {
                spdlog::error("Failed to update digest");
                THROW_RUNTIME_ERROR("Failed to update digest");
            }
        }

        if (file.gcount() > 0) {
            if (EVP_DigestUpdate(mdctx.get(), buffer.data(), file.gcount()) !=
                1) {
                spdlog::error("Failed to update digest with remaining data");
                THROW_RUNTIME_ERROR(
                    "Failed to update digest with remaining data");
            }
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int hashLen = 0;
        if (EVP_DigestFinal_ex(mdctx.get(), hash.data(), &hashLen) != 1) {
            spdlog::error("Failed to finalize digest");
            THROW_RUNTIME_ERROR("Failed to finalize digest");
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::setw(2) << static_cast<int>(hash[i]);
        }

        spdlog::info("SHA-256 calculation completed successfully");
        return oss.str();
    } catch (const std::exception& ex) {
        spdlog::error("Exception in calculateSha256: {}", ex.what());
        throw;
    }
}

auto calculateHash(const std::string& data,
                   const EVP_MD* (*hashFunction)()) noexcept -> std::string {
    try {
        if (data.empty()) {
            spdlog::warn("Empty data provided for hash calculation");
            return "";
        }

        MessageDigestContext context;
        const EVP_MD* messageDigest = hashFunction();

        if (!messageDigest) {
            spdlog::error("Invalid hash function");
            return "";
        }

        if (EVP_DigestInit_ex(context.get(), messageDigest, nullptr) != 1) {
            spdlog::error("Failed to initialize digest");
            return "";
        }

        if (EVP_DigestUpdate(context.get(), data.c_str(), data.size()) != 1) {
            spdlog::error("Failed to update digest");
            return "";
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int lengthOfHash = 0;

        if (EVP_DigestFinal_ex(context.get(), hash.data(), &lengthOfHash) !=
            1) {
            spdlog::error("Failed to finalize digest");
            return "";
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < lengthOfHash; ++i) {
            oss << std::setw(2) << static_cast<int>(hash[i]);
        }

        return oss.str();
    } catch (const std::exception& ex) {
        spdlog::error("Exception in calculateHash: {}", ex.what());
        return "";
    }
}

auto calculateSha224(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-224 hash");
    return calculateHash(data, EVP_sha224);
}

auto calculateSha384(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-384 hash");
    return calculateHash(data, EVP_sha384);
}

auto calculateSha512(const std::string& data) noexcept -> std::string {
    spdlog::info("Calculating SHA-512 hash");
    return calculateHash(data, EVP_sha512);
}

}  // namespace atom::utils
