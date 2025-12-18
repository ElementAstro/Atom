#include "encryption.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstring>

#include "key_derivation.hpp"
#include "secure_memory.hpp"

namespace atom::secret {

// ============================================================================
// EncryptedData Implementation
// ============================================================================

std::vector<uint8_t> EncryptedData::serialize() const {
    std::vector<uint8_t> result;

    // Format:
    // [version:1][algorithm:1][iterations:4][salt_len:4][iv_len:4][tag_len:4][cipher_len:4]
    // [salt][iv][tag][ciphertext]

    const uint8_t version = 2;  // Version 2 for new format
    result.push_back(version);
    result.push_back(static_cast<uint8_t>(algorithm));

    // Write iterations (4 bytes, big-endian)
    uint32_t iter = static_cast<uint32_t>(keyIterations);
    result.push_back((iter >> 24) & 0xFF);
    result.push_back((iter >> 16) & 0xFF);
    result.push_back((iter >> 8) & 0xFF);
    result.push_back(iter & 0xFF);

    auto writeLengthBE = [&result](uint32_t len) {
        result.push_back((len >> 24) & 0xFF);
        result.push_back((len >> 16) & 0xFF);
        result.push_back((len >> 8) & 0xFF);
        result.push_back(len & 0xFF);
    };

    writeLengthBE(static_cast<uint32_t>(salt.size()));
    writeLengthBE(static_cast<uint32_t>(iv.size()));
    writeLengthBE(static_cast<uint32_t>(tag.size()));
    writeLengthBE(static_cast<uint32_t>(ciphertext.size()));

    result.insert(result.end(), salt.begin(), salt.end());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), tag.begin(), tag.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());

    return result;
}

Result<EncryptedData> EncryptedData::deserialize(
    const std::vector<uint8_t>& data) {
    if (data.size() < 22) {
        return Result<EncryptedData>::error(ErrorCode::InvalidFormat,
                                            "Invalid encrypted data format");
    }

    size_t pos = 0;

    uint8_t version = data[pos++];
    if (version != 1 && version != 2) {
        return Result<EncryptedData>::error(
            ErrorCode::UnsupportedVersion,
            "Unsupported encrypted data version");
    }

    uint8_t algByte = data[pos++];
    if (algByte > 4) {
        return Result<EncryptedData>::error(ErrorCode::UnsupportedAlgorithm,
                                            "Unknown encryption algorithm");
    }

    EncryptionAlgorithm alg = static_cast<EncryptionAlgorithm>(algByte);

    if (pos + 4 > data.size()) {
        return Result<EncryptedData>::error(ErrorCode::InvalidFormat,
                                            "Truncated encrypted data");
    }

    uint32_t iterations = (static_cast<uint32_t>(data[pos]) << 24) |
                          (static_cast<uint32_t>(data[pos + 1]) << 16) |
                          (static_cast<uint32_t>(data[pos + 2]) << 8) |
                          static_cast<uint32_t>(data[pos + 3]);
    pos += 4;

    auto readLengthBE = [&data, &pos]() -> uint32_t {
        if (pos + 4 > data.size())
            return 0;
        uint32_t len = (static_cast<uint32_t>(data[pos]) << 24) |
                       (static_cast<uint32_t>(data[pos + 1]) << 16) |
                       (static_cast<uint32_t>(data[pos + 2]) << 8) |
                       static_cast<uint32_t>(data[pos + 3]);
        pos += 4;
        return len;
    };

    uint32_t saltLen = readLengthBE();
    uint32_t ivLen = readLengthBE();
    uint32_t tagLen = readLengthBE();
    uint32_t cipherLen = readLengthBE();

    if (pos + saltLen + ivLen + tagLen + cipherLen != data.size()) {
        return Result<EncryptedData>::error(ErrorCode::InvalidFormat,
                                            "Invalid encrypted data lengths");
    }

    EncryptedData result;
    result.algorithm = alg;
    result.keyIterations = static_cast<int>(iterations);

    result.salt.assign(data.begin() + pos, data.begin() + pos + saltLen);
    pos += saltLen;

    result.iv.assign(data.begin() + pos, data.begin() + pos + ivLen);
    pos += ivLen;

    result.tag.assign(data.begin() + pos, data.begin() + pos + tagLen);
    pos += tagLen;

    result.ciphertext.assign(data.begin() + pos,
                             data.begin() + pos + cipherLen);

    return Result<EncryptedData>::success(std::move(result));
}

bool EncryptedData::isAead() const noexcept {
    return Encryption::isAead(algorithm);
}

// ============================================================================
// Encryption Implementation
// ============================================================================

Result<EncryptedData> Encryption::encrypt(std::string_view plaintext,
                                          std::string_view password,
                                          const EncryptionParams& params) {
    return encrypt(std::vector<uint8_t>(plaintext.begin(), plaintext.end()),
                   password, params);
}

Result<EncryptedData> Encryption::encrypt(const std::vector<uint8_t>& plaintext,
                                          std::string_view password,
                                          const EncryptionParams& params) {
    if (plaintext.empty()) {
        return Result<EncryptedData>::error(ErrorCode::InvalidPlaintext,
                                            "Plaintext cannot be empty");
    }

    if (password.empty()) {
        return Result<EncryptedData>::error(ErrorCode::InvalidArgument,
                                            "Password cannot be empty");
    }

    // Generate salt
    auto saltResult = KeyDerivation::generateSalt(32);
    if (saltResult.isError()) {
        return Result<EncryptedData>::error(ErrorCode::RandomGenerationFailed,
                                            "Failed to generate salt");
    }

    // Derive key
    size_t keySize = getKeySize(params.algorithm);
    auto keyResult = KeyDerivation::pbkdf2Sha256(password, saltResult.value(),
                                                 params.keyIterations, keySize);
    if (keyResult.isError()) {
        return Result<EncryptedData>::error(ErrorCode::KeyDerivationFailed,
                                            "Failed to derive key");
    }

    // Encrypt with derived key
    auto encryptResult = encryptWithKey(plaintext, keyResult.value(), params);

    // Clear sensitive key material
    SecureMemory::secureClear(
        const_cast<std::vector<uint8_t>&>(keyResult.value()));

    if (encryptResult.isError()) {
        return encryptResult;
    }

    // Add salt to result
    EncryptedData result = std::move(encryptResult.value());
    result.salt = std::move(saltResult.value());
    result.keyIterations = params.keyIterations;

    return Result<EncryptedData>::success(std::move(result));
}

Result<std::string> Encryption::decrypt(const EncryptedData& encryptedData,
                                        std::string_view password) {
    auto result = decryptBytes(encryptedData, password);
    if (result.isError()) {
        return Result<std::string>::error(result.errorCode(),
                                          result.errorMessage());
    }
    return Result<std::string>::success(
        std::string(result.value().begin(), result.value().end()));
}

Result<std::vector<uint8_t>> Encryption::decryptBytes(
    const EncryptedData& encryptedData, std::string_view password) {
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (encryptedData.salt.empty()) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument, "Missing salt in encrypted data");
    }

    // Derive key
    size_t keySize = getKeySize(encryptedData.algorithm);
    auto keyResult = KeyDerivation::pbkdf2Sha256(
        password, encryptedData.salt, encryptedData.keyIterations, keySize);
    if (keyResult.isError()) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Failed to derive key");
    }

    // Decrypt with derived key
    auto decryptResult = decryptBytesWithKey(encryptedData, keyResult.value());

    // Clear sensitive key material
    SecureMemory::secureClear(
        const_cast<std::vector<uint8_t>&>(keyResult.value()));

    return decryptResult;
}

Result<EncryptedData> Encryption::encryptWithKey(
    std::string_view plaintext, const std::vector<uint8_t>& key,
    const EncryptionParams& params) {
    return encryptWithKey(
        std::vector<uint8_t>(plaintext.begin(), plaintext.end()), key, params);
}

Result<EncryptedData> Encryption::encryptWithKey(
    const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key,
    const EncryptionParams& params) {
    if (plaintext.empty()) {
        return Result<EncryptedData>::error(ErrorCode::InvalidPlaintext,
                                            "Plaintext cannot be empty");
    }

    if (key.size() != getKeySize(params.algorithm)) {
        return Result<EncryptedData>::error(ErrorCode::InvalidKey,
                                            "Invalid key size for algorithm");
    }

    // Generate IV
    size_t ivSize = getIvSize(params.algorithm);
    auto ivResult = KeyDerivation::generateRandomBytes(ivSize);
    if (ivResult.isError()) {
        return Result<EncryptedData>::error(ErrorCode::RandomGenerationFailed,
                                            "Failed to generate IV");
    }

    EncryptedData result;
    result.algorithm = params.algorithm;
    result.iv = std::move(ivResult.value());
    result.keyIterations = params.keyIterations;

    switch (params.algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_256_GCM: {
            auto encResult = encryptAesGcm(plaintext.data(), plaintext.size(),
                                           key, result.iv);
            if (encResult.isError()) {
                return Result<EncryptedData>::error(ErrorCode::EncryptionFailed,
                                                    encResult.errorMessage());
            }
            result.ciphertext = std::move(encResult.value().first);
            result.tag = std::move(encResult.value().second);
            break;
        }
        case EncryptionAlgorithm::AES_128_CBC:
        case EncryptionAlgorithm::AES_256_CBC: {
            auto encResult = encryptAesCbc(plaintext.data(), plaintext.size(),
                                           key, result.iv);
            if (encResult.isError()) {
                return Result<EncryptedData>::error(ErrorCode::EncryptionFailed,
                                                    encResult.errorMessage());
            }
            result.ciphertext = std::move(encResult.value());
            break;
        }
        case EncryptionAlgorithm::ChaCha20_Poly1305: {
            auto encResult = encryptChaCha20Poly1305(
                plaintext.data(), plaintext.size(), key, result.iv);
            if (encResult.isError()) {
                return Result<EncryptedData>::error(ErrorCode::EncryptionFailed,
                                                    encResult.errorMessage());
            }
            result.ciphertext = std::move(encResult.value().first);
            result.tag = std::move(encResult.value().second);
            break;
        }
        default:
            return Result<EncryptedData>::error(ErrorCode::UnsupportedAlgorithm,
                                                "Unknown encryption algorithm");
    }

    return Result<EncryptedData>::success(std::move(result));
}

Result<std::string> Encryption::decryptWithKey(
    const EncryptedData& encryptedData, const std::vector<uint8_t>& key) {
    auto result = decryptBytesWithKey(encryptedData, key);
    if (result.isError()) {
        return Result<std::string>::error(result.errorCode(),
                                          result.errorMessage());
    }
    return Result<std::string>::success(
        std::string(result.value().begin(), result.value().end()));
}

Result<std::vector<uint8_t>> Encryption::decryptBytesWithKey(
    const EncryptedData& encryptedData, const std::vector<uint8_t>& key) {
    if (key.size() != getKeySize(encryptedData.algorithm)) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidKey, "Invalid key size for algorithm");
    }

    if (encryptedData.ciphertext.empty()) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidCiphertext, "Ciphertext cannot be empty");
    }

    switch (encryptedData.algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_256_GCM:
            return decryptAesGcm(encryptedData.ciphertext, key,
                                 encryptedData.iv, encryptedData.tag);
        case EncryptionAlgorithm::AES_128_CBC:
        case EncryptionAlgorithm::AES_256_CBC:
            return decryptAesCbc(encryptedData.ciphertext, key,
                                 encryptedData.iv);
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return decryptChaCha20Poly1305(encryptedData.ciphertext, key,
                                           encryptedData.iv, encryptedData.tag);
        default:
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::UnsupportedAlgorithm,
                "Unknown encryption algorithm");
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

size_t Encryption::getKeySize(EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_128_CBC:
            return 16;
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::AES_256_CBC:
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return 32;
        default:
            return 0;
    }
}

size_t Encryption::getIvSize(EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return 12;
        case EncryptionAlgorithm::AES_128_CBC:
        case EncryptionAlgorithm::AES_256_CBC:
            return 16;
        default:
            return 0;
    }
}

size_t Encryption::getTagSize(EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return 16;
        default:
            return 0;
    }
}

bool Encryption::isAead(EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
        case EncryptionAlgorithm::AES_256_GCM:
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return true;
        default:
            return false;
    }
}

bool Encryption::isAvailable(EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
            return EVP_aes_128_gcm() != nullptr;
        case EncryptionAlgorithm::AES_256_GCM:
            return EVP_aes_256_gcm() != nullptr;
        case EncryptionAlgorithm::AES_128_CBC:
            return EVP_aes_128_cbc() != nullptr;
        case EncryptionAlgorithm::AES_256_CBC:
            return EVP_aes_256_cbc() != nullptr;
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return EVP_chacha20_poly1305() != nullptr;
        default:
            return false;
    }
}

std::string Encryption::getAlgorithmName(
    EncryptionAlgorithm algorithm) noexcept {
    switch (algorithm) {
        case EncryptionAlgorithm::AES_128_GCM:
            return "AES-128-GCM";
        case EncryptionAlgorithm::AES_256_GCM:
            return "AES-256-GCM";
        case EncryptionAlgorithm::AES_128_CBC:
            return "AES-128-CBC";
        case EncryptionAlgorithm::AES_256_CBC:
            return "AES-256-CBC";
        case EncryptionAlgorithm::ChaCha20_Poly1305:
            return "ChaCha20-Poly1305";
        default:
            return "Unknown";
    }
}

// ============================================================================
// Private Implementation Methods
// ============================================================================

Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
Encryption::encryptAesGcm(const uint8_t* plaintext, size_t plaintextLen,
                          const std::vector<uint8_t>& key,
                          const std::vector<uint8_t>& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
            error(ErrorCode::EncryptionFailed,
                  "Failed to create cipher context");
    }

    const EVP_CIPHER* cipher =
        (key.size() == 16) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    std::vector<uint8_t> ciphertext(plaintextLen + 16);
    std::vector<uint8_t> tag(16);
    int len = 0;
    int ciphertextLen = 0;

    bool success = true;

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        success = false;
    }

    if (success &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(iv.size()), nullptr) != 1) {
        success = false;
    }

    if (success &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        success = false;
    }

    if (success && EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext,
                                     static_cast<int>(plaintextLen)) != 1) {
        success = false;
    }
    ciphertextLen = len;

    if (success &&
        EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        success = false;
    }
    ciphertextLen += len;

    if (success &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        success = false;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
            error(ErrorCode::EncryptionFailed, "AES-GCM encryption failed");
    }

    ciphertext.resize(ciphertextLen);
    return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
        success(std::make_pair(std::move(ciphertext), std::move(tag)));
}

Result<std::vector<uint8_t>> Encryption::decryptAesGcm(
    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::DecryptionFailed, "Failed to create cipher context");
    }

    const EVP_CIPHER* cipher =
        (key.size() == 16) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();

    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    int plaintextLen = 0;

    bool success = true;

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
        success = false;
    }

    if (success &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                            static_cast<int>(iv.size()), nullptr) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        success = false;
    }
    plaintextLen = len;

    if (success && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                                       static_cast<int>(tag.size()),
                                       const_cast<uint8_t*>(tag.data())) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::AuthenticationFailed,
            "Authentication verification failed");
    }
    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::DecryptionFailed,
                                                   "AES-GCM decryption failed");
    }

    plaintext.resize(plaintextLen);
    return Result<std::vector<uint8_t>>::success(std::move(plaintext));
}

Result<std::vector<uint8_t>> Encryption::encryptAesCbc(
    const uint8_t* plaintext, size_t plaintextLen,
    const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::EncryptionFailed, "Failed to create cipher context");
    }

    const EVP_CIPHER* cipher =
        (key.size() == 16) ? EVP_aes_128_cbc() : EVP_aes_256_cbc();

    std::vector<uint8_t> ciphertext(plaintextLen + 16);
    int len = 0;
    int ciphertextLen = 0;

    bool success = true;

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data()) != 1) {
        success = false;
    }

    if (success && EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext,
                                     static_cast<int>(plaintextLen)) != 1) {
        success = false;
    }
    ciphertextLen = len;

    if (success &&
        EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        success = false;
    }
    ciphertextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::EncryptionFailed,
                                                   "AES-CBC encryption failed");
    }

    ciphertext.resize(ciphertextLen);
    return Result<std::vector<uint8_t>>::success(std::move(ciphertext));
}

Result<std::vector<uint8_t>> Encryption::decryptAesCbc(
    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::DecryptionFailed, "Failed to create cipher context");
    }

    const EVP_CIPHER* cipher =
        (key.size() == 16) ? EVP_aes_128_cbc() : EVP_aes_256_cbc();

    std::vector<uint8_t> plaintext(ciphertext.size() + 16);
    int len = 0;
    int plaintextLen = 0;

    bool success = true;

    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, key.data(), iv.data()) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        success = false;
    }
    plaintextLen = len;

    if (success &&
        EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::DecryptionFailed,
            "Decryption failed or invalid padding");
    }
    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::DecryptionFailed,
                                                   "AES-CBC decryption failed");
    }

    plaintext.resize(plaintextLen);
    return Result<std::vector<uint8_t>>::success(std::move(plaintext));
}

Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
Encryption::encryptChaCha20Poly1305(const uint8_t* plaintext,
                                    size_t plaintextLen,
                                    const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& nonce) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
            error(ErrorCode::EncryptionFailed,
                  "Failed to create cipher context");
    }

    std::vector<uint8_t> ciphertext(plaintextLen + 16);
    std::vector<uint8_t> tag(16);
    int len = 0;
    int ciphertextLen = 0;

    bool success = true;

    if (EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key.data(),
                           nonce.data()) != 1) {
        success = false;
    }

    if (success && EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext,
                                     static_cast<int>(plaintextLen)) != 1) {
        success = false;
    }
    ciphertextLen = len;

    if (success &&
        EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        success = false;
    }
    ciphertextLen += len;

    if (success &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag.data()) != 1) {
        success = false;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
            error(ErrorCode::EncryptionFailed,
                  "ChaCha20-Poly1305 encryption failed");
    }

    ciphertext.resize(ciphertextLen);
    return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
        success(std::make_pair(std::move(ciphertext), std::move(tag)));
}

Result<std::vector<uint8_t>> Encryption::decryptChaCha20Poly1305(
    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& nonce, const std::vector<uint8_t>& tag) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::DecryptionFailed, "Failed to create cipher context");
    }

    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    int plaintextLen = 0;

    bool success = true;

    if (EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), nullptr, key.data(),
                           nonce.data()) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                          static_cast<int>(ciphertext.size())) != 1) {
        success = false;
    }
    plaintextLen = len;

    if (success && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
                                       static_cast<int>(tag.size()),
                                       const_cast<uint8_t*>(tag.data())) != 1) {
        success = false;
    }

    if (success &&
        EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::AuthenticationFailed,
            "Authentication verification failed");
    }
    plaintextLen += len;

    EVP_CIPHER_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::DecryptionFailed, "ChaCha20-Poly1305 decryption failed");
    }

    plaintext.resize(plaintextLen);
    return Result<std::vector<uint8_t>>::success(std::move(plaintext));
}

}  // namespace atom::secret
