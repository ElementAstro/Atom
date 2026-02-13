#include "hmac.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "../core/types.hpp"

namespace atom::secret {

namespace {

const EVP_MD* getEvpMd(HashAlgorithm algorithm) {
    switch (algorithm) {
        case HashAlgorithm::SHA256:
            return EVP_sha256();
        case HashAlgorithm::SHA384:
            return EVP_sha384();
        case HashAlgorithm::SHA512:
            return EVP_sha512();
        case HashAlgorithm::SHA3_256:
            return EVP_sha3_256();
        case HashAlgorithm::SHA3_512:
            return EVP_sha3_512();
        case HashAlgorithm::MD5:
            return EVP_md5();
        default:
            return nullptr;
    }
}

Result<std::vector<uint8_t>> computeHmac(const EVP_MD* md, const uint8_t* key,
                                         size_t keyLen, const uint8_t* data,
                                         size_t dataLen) {
    if (!md) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidHashAlgorithm,
            "Hash algorithm not available for HMAC");
    }

    std::vector<uint8_t> mac(EVP_MD_size(md));
    unsigned int macLen = 0;

    unsigned char* result = HMAC(md, key, static_cast<int>(keyLen), data,
                                 dataLen, mac.data(), &macLen);

    if (!result) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::HmacFailed,
                                                   "HMAC computation failed");
    }

    mac.resize(macLen);
    return Result<std::vector<uint8_t>>::success(std::move(mac));
}

}  // namespace

Result<std::vector<uint8_t>> Hmac::sha256(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& data) {
    return computeHmac(EVP_sha256(), key.data(), key.size(), data.data(),
                       data.size());
}

Result<std::vector<uint8_t>> Hmac::sha256(std::string_view key,
                                          std::string_view data) {
    return computeHmac(
        EVP_sha256(), reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Result<std::vector<uint8_t>> Hmac::sha384(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& data) {
    return computeHmac(EVP_sha384(), key.data(), key.size(), data.data(),
                       data.size());
}

Result<std::vector<uint8_t>> Hmac::sha384(std::string_view key,
                                          std::string_view data) {
    return computeHmac(
        EVP_sha384(), reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Result<std::vector<uint8_t>> Hmac::sha512(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& data) {
    return computeHmac(EVP_sha512(), key.data(), key.size(), data.data(),
                       data.size());
}

Result<std::vector<uint8_t>> Hmac::sha512(std::string_view key,
                                          std::string_view data) {
    return computeHmac(
        EVP_sha512(), reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Result<std::vector<uint8_t>> Hmac::compute(HashAlgorithm algorithm,
                                           const std::vector<uint8_t>& key,
                                           const std::vector<uint8_t>& data) {
    const EVP_MD* md = getEvpMd(algorithm);
    return computeHmac(md, key.data(), key.size(), data.data(), data.size());
}

Result<std::vector<uint8_t>> Hmac::compute(HashAlgorithm algorithm,
                                           std::string_view key,
                                           std::string_view data) {
    const EVP_MD* md = getEvpMd(algorithm);
    return computeHmac(
        md, reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool Hmac::verify(HashAlgorithm algorithm, const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& data,
                  const std::vector<uint8_t>& expectedMac) {
    auto result = compute(algorithm, key, data);
    if (result.isError()) {
        return false;
    }

    const auto& computed = result.value();
    if (computed.size() != expectedMac.size()) {
        return false;
    }

    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < computed.size(); ++i) {
        diff |= computed[i] ^ expectedMac[i];
    }
    return diff == 0;
}

bool Hmac::verify(HashAlgorithm algorithm, std::string_view key,
                  std::string_view data, std::string_view expectedMacHex) {
    auto expectedBytes = hexToBytes(expectedMacHex);
    if (expectedBytes.empty() && !expectedMacHex.empty()) {
        return false;  // Invalid hex
    }

    std::vector<uint8_t> keyBytes(key.begin(), key.end());
    std::vector<uint8_t> dataBytes(data.begin(), data.end());
    return verify(algorithm, keyBytes, dataBytes, expectedBytes);
}

Result<std::string> Hmac::computeHex(HashAlgorithm algorithm,
                                     std::string_view key,
                                     std::string_view data) {
    auto result = compute(algorithm, key, data);
    if (result.isError()) {
        return Result<std::string>::error(result.errorCode(),
                                          result.errorMessage());
    }
    return Result<std::string>::success(bytesToHex(result.value()));
}

}  // namespace atom::secret
