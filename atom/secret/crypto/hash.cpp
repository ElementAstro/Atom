#include "hash.hpp"

#include <openssl/evp.h>

#include "../core/types.hpp"

namespace atom::secret {

namespace {

Result<std::vector<uint8_t>> computeHash(const EVP_MD* md, const uint8_t* data,
                                         size_t dataLen) {
    if (!md) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidHashAlgorithm, "Hash algorithm not available");
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::HashFailed, "Failed to create hash context");
    }

    std::vector<uint8_t> digest(EVP_MD_size(md));
    unsigned int digestLen = 0;

    bool success = true;

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        success = false;
    }

    if (success && EVP_DigestUpdate(ctx, data, dataLen) != 1) {
        success = false;
    }

    if (success && EVP_DigestFinal_ex(ctx, digest.data(), &digestLen) != 1) {
        success = false;
    }

    EVP_MD_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::HashFailed,
                                                   "Hash computation failed");
    }

    digest.resize(digestLen);
    return Result<std::vector<uint8_t>>::success(std::move(digest));
}

}  // namespace

Result<std::vector<uint8_t>> Hash::sha256(const std::vector<uint8_t>& data) {
    return computeHash(EVP_sha256(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hash::sha256(std::string_view data) {
    return computeHash(EVP_sha256(),
                       reinterpret_cast<const uint8_t*>(data.data()),
                       data.size());
}

Result<std::vector<uint8_t>> Hash::sha384(const std::vector<uint8_t>& data) {
    return computeHash(EVP_sha384(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hash::sha384(std::string_view data) {
    return computeHash(EVP_sha384(),
                       reinterpret_cast<const uint8_t*>(data.data()),
                       data.size());
}

Result<std::vector<uint8_t>> Hash::sha512(const std::vector<uint8_t>& data) {
    return computeHash(EVP_sha512(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hash::sha512(std::string_view data) {
    return computeHash(EVP_sha512(),
                       reinterpret_cast<const uint8_t*>(data.data()),
                       data.size());
}

Result<std::vector<uint8_t>> Hash::sha3_256(const std::vector<uint8_t>& data) {
    return computeHash(EVP_sha3_256(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hash::sha3_256(std::string_view data) {
    return computeHash(EVP_sha3_256(),
                       reinterpret_cast<const uint8_t*>(data.data()),
                       data.size());
}

Result<std::vector<uint8_t>> Hash::sha3_512(const std::vector<uint8_t>& data) {
    return computeHash(EVP_sha3_512(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hash::sha3_512(std::string_view data) {
    return computeHash(EVP_sha3_512(),
                       reinterpret_cast<const uint8_t*>(data.data()),
                       data.size());
}

Result<std::vector<uint8_t>> Hash::blake2b(const std::vector<uint8_t>& data,
                                           size_t digestLength) {
    if (digestLength == 0 || digestLength > 64) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument,
            "BLAKE2b digest length must be 1-64 bytes");
    }

    const EVP_MD* md = nullptr;
    if (digestLength <= 32) {
        md = EVP_blake2b512();  // We'll truncate
    } else {
        md = EVP_blake2b512();
    }

    auto result = computeHash(md, data.data(), data.size());
    if (result.isSuccess() && digestLength < 64) {
        auto& hash = result.value();
        hash.resize(digestLength);
    }
    return result;
}

Result<std::vector<uint8_t>> Hash::blake2b(std::string_view data,
                                           size_t digestLength) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return blake2b(bytes, digestLength);
}

Result<std::vector<uint8_t>> Hash::blake2s(const std::vector<uint8_t>& data,
                                           size_t digestLength) {
    if (digestLength == 0 || digestLength > 32) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument,
            "BLAKE2s digest length must be 1-32 bytes");
    }

    const EVP_MD* md = EVP_blake2s256();
    auto result = computeHash(md, data.data(), data.size());
    if (result.isSuccess() && digestLength < 32) {
        auto& hash = result.value();
        hash.resize(digestLength);
    }
    return result;
}

Result<std::vector<uint8_t>> Hash::blake2s(std::string_view data,
                                           size_t digestLength) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return blake2s(bytes, digestLength);
}

Result<std::vector<uint8_t>> Hash::compute(HashAlgorithm algorithm,
                                           const std::vector<uint8_t>& data) {
    switch (algorithm) {
        case HashAlgorithm::SHA256:
            return sha256(data);
        case HashAlgorithm::SHA384:
            return sha384(data);
        case HashAlgorithm::SHA512:
            return sha512(data);
        case HashAlgorithm::SHA3_256:
            return sha3_256(data);
        case HashAlgorithm::SHA3_512:
            return sha3_512(data);
        case HashAlgorithm::BLAKE2b256:
            return blake2b(data, 32);
        case HashAlgorithm::BLAKE2b512:
            return blake2b(data, 64);
        case HashAlgorithm::BLAKE2s256:
            return blake2s(data, 32);
        case HashAlgorithm::MD5:
            return computeHash(EVP_md5(), data.data(), data.size());
        default:
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::InvalidHashAlgorithm, "Unknown hash algorithm");
    }
}

Result<std::vector<uint8_t>> Hash::compute(HashAlgorithm algorithm,
                                           std::string_view data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return compute(algorithm, bytes);
}

size_t Hash::getDigestSize(HashAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case HashAlgorithm::SHA256:
        case HashAlgorithm::SHA3_256:
        case HashAlgorithm::BLAKE2b256:
        case HashAlgorithm::BLAKE2s256:
            return 32;
        case HashAlgorithm::SHA384:
            return 48;
        case HashAlgorithm::SHA512:
        case HashAlgorithm::SHA3_512:
        case HashAlgorithm::BLAKE2b512:
            return 64;
        case HashAlgorithm::MD5:
            return 16;
        default:
            return 0;
    }
}

std::string Hash::getAlgorithmName(HashAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case HashAlgorithm::SHA256:
            return "SHA-256";
        case HashAlgorithm::SHA384:
            return "SHA-384";
        case HashAlgorithm::SHA512:
            return "SHA-512";
        case HashAlgorithm::SHA3_256:
            return "SHA3-256";
        case HashAlgorithm::SHA3_512:
            return "SHA3-512";
        case HashAlgorithm::BLAKE2b256:
            return "BLAKE2b-256";
        case HashAlgorithm::BLAKE2b512:
            return "BLAKE2b-512";
        case HashAlgorithm::BLAKE2s256:
            return "BLAKE2s-256";
        case HashAlgorithm::MD5:
            return "MD5";
        default:
            return "Unknown";
    }
}

bool Hash::isAvailable(HashAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case HashAlgorithm::SHA256:
            return EVP_sha256() != nullptr;
        case HashAlgorithm::SHA384:
            return EVP_sha384() != nullptr;
        case HashAlgorithm::SHA512:
            return EVP_sha512() != nullptr;
        case HashAlgorithm::SHA3_256:
            return EVP_sha3_256() != nullptr;
        case HashAlgorithm::SHA3_512:
            return EVP_sha3_512() != nullptr;
        case HashAlgorithm::BLAKE2b256:
        case HashAlgorithm::BLAKE2b512:
            return EVP_blake2b512() != nullptr;
        case HashAlgorithm::BLAKE2s256:
            return EVP_blake2s256() != nullptr;
        case HashAlgorithm::MD5:
            return EVP_md5() != nullptr;
        default:
            return false;
    }
}

Result<std::string> Hash::computeHex(HashAlgorithm algorithm,
                                     std::string_view data) {
    auto result = compute(algorithm, data);
    if (result.isError()) {
        return Result<std::string>::error(result.errorCode(),
                                          result.errorMessage());
    }
    return Result<std::string>::success(bytesToHex(result.value()));
}

bool Hash::verify(HashAlgorithm algorithm, const std::vector<uint8_t>& data,
                  const std::vector<uint8_t>& expectedHash) {
    auto result = compute(algorithm, data);
    if (result.isError()) {
        return false;
    }

    const auto& computed = result.value();
    if (computed.size() != expectedHash.size()) {
        return false;
    }

    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < computed.size(); ++i) {
        diff |= computed[i] ^ expectedHash[i];
    }
    return diff == 0;
}

bool Hash::verify(HashAlgorithm algorithm, std::string_view data,
                  std::string_view expectedHex) {
    auto expectedBytes = hexToBytes(expectedHex);
    if (expectedBytes.empty() && !expectedHex.empty()) {
        return false;  // Invalid hex
    }

    std::vector<uint8_t> dataBytes(data.begin(), data.end());
    return verify(algorithm, dataBytes, expectedBytes);
}

}  // namespace atom::secret
