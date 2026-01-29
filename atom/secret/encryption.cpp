#include "encryption.hpp"

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "atom/error/exception.hpp"

namespace atom::secret {

SslCipherContext::SslCipherContext() : ctx(EVP_CIPHER_CTX_new()) {
    if (!ctx) {
        THROW_RUNTIME_ERROR("Failed to create OpenSSL cipher context");
    }
}

SslCipherContext::~SslCipherContext() {
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
        ctx = nullptr;
    }
}

SslCipherContext::SslCipherContext(SslCipherContext&& other) noexcept
    : ctx(other.ctx) {
    other.ctx = nullptr;
}

SslCipherContext& SslCipherContext::operator=(
    SslCipherContext&& other) noexcept {
    if (this != &other) {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
        ctx = other.ctx;
        other.ctx = nullptr;
    }
    return *this;
}

// ============================================================================
// SecureMemory Implementation
// ============================================================================

void SecureMemory::secureClear(void* ptr, size_t size) noexcept {
    if (!ptr || size == 0) {
        return;
    }

    // First pass: overwrite with random data
    if (RAND_bytes(static_cast<unsigned char*>(ptr), static_cast<int>(size)) !=
        1) {
        // Fallback to deterministic pattern if random fails
        std::memset(ptr, 0xAA, size);
        std::memset(ptr, 0x55, size);
    }

    // Second pass: zero out
    std::memset(ptr, 0, size);

    // Memory barrier to prevent compiler optimization
    std::atomic_signal_fence(std::memory_order_acq_rel);
}

void SecureMemory::secureClear(std::string& str) noexcept {
    if (!str.empty()) {
        secureClear(str.data(), str.size());
        str.clear();
        str.shrink_to_fit();
    }
}

template <typename T>
void SecureMemory::secureClear(std::vector<T>& vec) noexcept {
    if (!vec.empty()) {
        secureClear(vec.data(), vec.size() * sizeof(T));
        vec.clear();
        vec.shrink_to_fit();
    }
}

// Explicit template instantiations
template void SecureMemory::secureClear<uint8_t>(
    std::vector<uint8_t>&) noexcept;
template void SecureMemory::secureClear<char>(std::vector<char>&) noexcept;

bool SecureMemory::lockMemory(void* ptr, size_t size) noexcept {
    if (!ptr || size == 0) {
        return false;
    }

#if defined(_WIN32)
    return VirtualLock(ptr, size) != 0;
#elif defined(__linux__) || defined(__APPLE__)
    return mlock(ptr, size) == 0;
#else
    // Platform not supported, but don't fail
    return true;
#endif
}

bool SecureMemory::unlockMemory(void* ptr, size_t size) noexcept {
    if (!ptr || size == 0) {
        return false;
    }

#if defined(_WIN32)
    return VirtualUnlock(ptr, size) != 0;
#elif defined(__linux__) || defined(__APPLE__)
    return munlock(ptr, size) == 0;
#else
    // Platform not supported, but don't fail
    return true;
#endif
}

void* SecureMemory::allocateSecure(size_t size) noexcept {
    if (size == 0) {
        return nullptr;
    }

#if defined(_WIN32)
    void* ptr = _aligned_malloc(size, 64);
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, size) != 0) {
        ptr = nullptr;
    }
#endif

    if (ptr && !lockMemory(ptr, size)) {
        // Failed to lock memory, continue without lock
    }

    return ptr;
}

void SecureMemory::freeSecure(void* ptr, size_t size) noexcept {
    if (!ptr) {
        return;
    }

    // Clear memory before freeing
    secureClear(ptr, size);

    // Unlock memory
    unlockMemory(ptr, size);

    // Free memory
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

// ============================================================================
// SecureBuffer Template Implementation
// ============================================================================

template <typename T>
SecureBuffer<T>::SecureBuffer(size_t size)
    : data_(static_cast<T*>(SecureMemory::allocateSecure(size * sizeof(T)))),
      size_(data_ ? size : 0) {}

template <typename T>
SecureBuffer<T>::~SecureBuffer() {
    if (data_) {
        SecureMemory::freeSecure(data_, size_ * sizeof(T));
        data_ = nullptr;
        size_ = 0;
    }
}

template <typename T>
SecureBuffer<T>::SecureBuffer(SecureBuffer&& other) noexcept
    : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

template <typename T>
SecureBuffer<T>& SecureBuffer<T>::operator=(SecureBuffer&& other) noexcept {
    if (this != &other) {
        if (data_) {
            SecureMemory::freeSecure(data_, size_ * sizeof(T));
        }
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

// Explicit template instantiations
template class SecureBuffer<uint8_t>;
template class SecureBuffer<char>;

// ============================================================================
// KeyDerivation Implementation
// ============================================================================

Result<std::vector<uint8_t>> KeyDerivation::deriveKey(
    std::string_view password, const std::vector<uint8_t>& salt, int iterations,
    size_t keyLength) {
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error("Password cannot be empty");
    }

    if (salt.empty()) {
        return Result<std::vector<uint8_t>>::error("Salt cannot be empty");
    }

    if (iterations < 1000) {
        return Result<std::vector<uint8_t>>::error(
            "Iteration count too low (minimum 1000)");
    }

    if (keyLength == 0 || keyLength > 1024) {
        return Result<std::vector<uint8_t>>::error("Invalid key length");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    int result = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.length()), salt.data(),
        static_cast<int>(salt.size()), iterations, EVP_sha256(),
        static_cast<int>(keyLength), derivedKey.data());

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error("Key derivation failed");
    }

    return Result<std::vector<uint8_t>>(std::move(derivedKey));
}

Result<std::vector<uint8_t>> KeyDerivation::generateSalt(size_t length) {
    if (length == 0 || length > 1024) {
        return Result<std::vector<uint8_t>>::error("Invalid salt length");
    }

    std::vector<uint8_t> salt(length);

    if (RAND_bytes(salt.data(), static_cast<int>(length)) != 1) {
        return Result<std::vector<uint8_t>>::error(
            "Failed to generate random salt");
    }

    return Result<std::vector<uint8_t>>(std::move(salt));
}

Result<std::vector<uint8_t>> KeyDerivation::generateKey(size_t length) {
    if (length == 0 || length > 1024) {
        return Result<std::vector<uint8_t>>::error("Invalid key length");
    }

    std::vector<uint8_t> key(length);

    if (RAND_bytes(key.data(), static_cast<int>(length)) != 1) {
        return Result<std::vector<uint8_t>>::error(
            "Failed to generate random key");
    }

    return Result<std::vector<uint8_t>>(std::move(key));
}

// ============================================================================
// EncryptedData Implementation
// ============================================================================

std::vector<uint8_t> EncryptedData::serialize() const {
    std::vector<uint8_t> result;

    // Format:
    // [version:1][method:1][iterations:4][salt_len:4][iv_len:4][tag_len:4][cipher_len:4]
    //         [salt][iv][tag][ciphertext]

    const uint8_t version = 1;
    result.push_back(version);
    result.push_back(static_cast<uint8_t>(method));

    // Write iterations (4 bytes, big-endian)
    uint32_t iter = static_cast<uint32_t>(keyIterations);
    result.push_back((iter >> 24) & 0xFF);
    result.push_back((iter >> 16) & 0xFF);
    result.push_back((iter >> 8) & 0xFF);
    result.push_back(iter & 0xFF);

    // Write lengths (4 bytes each, big-endian)
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

    // Write data
    result.insert(result.end(), salt.begin(), salt.end());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), tag.begin(), tag.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());

    return result;
}

Result<EncryptedData> EncryptedData::deserialize(
    const std::vector<uint8_t>& data) {
    if (data.size() < 22) {  // Minimum header size
        return Result<EncryptedData>::error("Invalid encrypted data format");
    }

    size_t pos = 0;

    // Read version
    uint8_t version = data[pos++];
    if (version != 1) {
        return Result<EncryptedData>::error(
            "Unsupported encrypted data version");
    }

    // Read method
    uint8_t methodByte = data[pos++];
    if (methodByte > 2) {
        return Result<EncryptedData>::error("Unknown encryption method");
    }

    EncryptionOptions::Method method =
        static_cast<EncryptionOptions::Method>(methodByte);

    // Read iterations (4 bytes, big-endian)
    if (pos + 4 > data.size()) {
        return Result<EncryptedData>::error("Truncated encrypted data");
    }

    uint32_t iterations = (static_cast<uint32_t>(data[pos]) << 24) |
                          (static_cast<uint32_t>(data[pos + 1]) << 16) |
                          (static_cast<uint32_t>(data[pos + 2]) << 8) |
                          static_cast<uint32_t>(data[pos + 3]);
    pos += 4;

    // Read lengths
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

    // Validate lengths
    if (pos + saltLen + ivLen + tagLen + cipherLen != data.size()) {
        return Result<EncryptedData>::error("Invalid encrypted data lengths");
    }

    EncryptedData result;
    result.method = method;
    result.keyIterations = static_cast<int>(iterations);

    // Read data sections
    result.salt.assign(data.begin() + pos, data.begin() + pos + saltLen);
    pos += saltLen;

    result.iv.assign(data.begin() + pos, data.begin() + pos + ivLen);
    pos += ivLen;

    result.tag.assign(data.begin() + pos, data.begin() + pos + tagLen);
    pos += tagLen;

    result.ciphertext.assign(data.begin() + pos,
                             data.begin() + pos + cipherLen);

    return Result<EncryptedData>(std::move(result));
}

// ============================================================================
// Encryption Implementation
// ============================================================================

Result<EncryptedData> Encryption::encrypt(std::string_view plaintext,
                                          std::string_view password,
                                          const EncryptionOptions& options) {
    if (plaintext.empty()) {
        return Result<EncryptedData>::error("Plaintext cannot be empty");
    }

    if (password.empty()) {
        return Result<EncryptedData>::error("Password cannot be empty");
    }

    // Generate salt
    auto saltResult = KeyDerivation::generateSalt(32);
    if (saltResult.isError()) {
        return Result<EncryptedData>::error("Failed to generate salt: " +
                                            saltResult.error());
    }

    // Derive key
    size_t keySize = getKeySize(options.encryptionMethod);
    auto keyResult = KeyDerivation::deriveKey(password, saltResult.value(),
                                              options.keyIterations, keySize);
    if (keyResult.isError()) {
        return Result<EncryptedData>::error("Failed to derive key: " +
                                            keyResult.error());
    }

    // Encrypt with derived key
    auto encryptResult = encryptWithKey(plaintext, keyResult.value(), options);
    if (encryptResult.isError()) {
        return encryptResult;
    }

    // Update salt in result
    EncryptedData result = encryptResult.value();
    result.salt = std::move(saltResult.value());
    result.keyIterations = options.keyIterations;

    // Clear sensitive data
    SecureMemory::secureClear(
        const_cast<std::vector<uint8_t>&>(keyResult.value()));

    return Result<EncryptedData>(std::move(result));
}

Result<std::string> Encryption::decrypt(const EncryptedData& encryptedData,
                                        std::string_view password) {
    if (password.empty()) {
        return Result<std::string>::error("Password cannot be empty");
    }

    if (encryptedData.salt.empty()) {
        return Result<std::string>::error("Missing salt in encrypted data");
    }

    // Derive key
    size_t keySize = getKeySize(encryptedData.method);
    auto keyResult = KeyDerivation::deriveKey(
        password, encryptedData.salt, encryptedData.keyIterations, keySize);
    if (keyResult.isError()) {
        return Result<std::string>::error("Failed to derive key: " +
                                          keyResult.error());
    }

    // Decrypt with derived key
    auto decryptResult = decryptWithKey(encryptedData, keyResult.value());

    // Clear sensitive data
    SecureMemory::secureClear(
        const_cast<std::vector<uint8_t>&>(keyResult.value()));

    return decryptResult;
}

Result<EncryptedData> Encryption::encryptWithKey(
    std::string_view plaintext, const std::vector<uint8_t>& key,
    const EncryptionOptions& options) {
    if (plaintext.empty()) {
        return Result<EncryptedData>::error("Plaintext cannot be empty");
    }

    if (key.empty()) {
        return Result<EncryptedData>::error("Key cannot be empty");
    }

    // Generate IV
    size_t ivSize = getIvSize(options.encryptionMethod);
    auto ivResult = KeyDerivation::generateKey(ivSize);
    if (ivResult.isError()) {
        return Result<EncryptedData>::error("Failed to generate IV: " +
                                            ivResult.error());
    }

    EncryptedData result{};
    result.method = options.encryptionMethod;
    result.iv = std::move(ivResult.value());
    result.keyIterations = options.keyIterations;

    // Encrypt based on method
    switch (options.encryptionMethod) {
        case EncryptionOptions::Method::AES_GCM: {
            auto encryptResult = encryptAesGcm(plaintext, key, result.iv);
            if (encryptResult.isError()) {
                return Result<EncryptedData>::error(
                    "AES-GCM encryption failed: " + encryptResult.error());
            }
            result.ciphertext = std::move(encryptResult.value().first);
            result.tag = std::move(encryptResult.value().second);
            break;
        }
        case EncryptionOptions::Method::AES_CBC: {
            auto encryptResult = encryptAesCbc(plaintext, key, result.iv);
            if (encryptResult.isError()) {
                return Result<EncryptedData>::error(
                    "AES-CBC encryption failed: " + encryptResult.error());
            }
            result.ciphertext = std::move(encryptResult.value());
            break;
        }
        case EncryptionOptions::Method::CHACHA20_POLY1305: {
            return Result<EncryptedData>::error(
                "ChaCha20-Poly1305 not yet implemented");
        }
        default:
            return Result<EncryptedData>::error("Unknown encryption method");
    }

    return Result<EncryptedData>(std::move(result));
}

Result<std::string> Encryption::decryptWithKey(
    const EncryptedData& encryptedData, const std::vector<uint8_t>& key) {
    if (key.empty()) {
        return Result<std::string>::error("Key cannot be empty");
    }

    if (encryptedData.ciphertext.empty()) {
        return Result<std::string>::error("Ciphertext cannot be empty");
    }

    // Decrypt based on method
    switch (encryptedData.method) {
        case EncryptionOptions::Method::AES_GCM:
            return decryptAesGcm(encryptedData.ciphertext, key,
                                 encryptedData.iv, encryptedData.tag);
        case EncryptionOptions::Method::AES_CBC:
            return decryptAesCbc(encryptedData.ciphertext, key,
                                 encryptedData.iv);
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return Result<std::string>::error(
                "ChaCha20-Poly1305 not yet implemented");
        default:
            return Result<std::string>::error("Unknown encryption method");
    }
}

// ============================================================================
// Private AES Implementation Methods
// ============================================================================

Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
Encryption::encryptAesGcm(std::string_view plaintext,
                          const std::vector<uint8_t>& key,
                          const std::vector<uint8_t>& iv) {
    try {
        SslCipherContext ctx;

        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr,
                               nullptr) != 1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to initialize AES-GCM encryption");
        }

        // Set IV length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(iv.size()), nullptr) != 1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to set IV length");
        }

        // Set key and IV
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) !=
            1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to set key and IV");
        }

        // Encrypt
        std::vector<uint8_t> ciphertext(plaintext.length() +
                                        16);  // Extra space for padding
        int len = 0;
        int ciphertext_len = 0;

        if (EVP_EncryptUpdate(
                ctx, ciphertext.data(), &len,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                static_cast<int>(plaintext.length())) != 1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to encrypt data");
        }
        ciphertext_len = len;

        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to finalize encryption");
        }
        ciphertext_len += len;
        ciphertext.resize(ciphertext_len);

        // Get authentication tag
        std::vector<uint8_t> tag(16);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) !=
            1) {
            return Result<
                std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
                error("Failed to get authentication tag");
        }

        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            std::make_pair(std::move(ciphertext), std::move(tag)));

    } catch (const std::exception& e) {
        return Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>::
            error(std::string("AES-GCM encryption error: ") + e.what());
    }
}

Result<std::string> Encryption::decryptAesGcm(
    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag) {
    try {
        SslCipherContext ctx;

        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr,
                               nullptr) != 1) {
            return Result<std::string>::error(
                "Failed to initialize AES-GCM decryption");
        }

        // Set IV length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(iv.size()), nullptr) != 1) {
            return Result<std::string>::error("Failed to set IV length");
        }

        // Set key and IV
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) !=
            1) {
            return Result<std::string>::error("Failed to set key and IV");
        }

        // Decrypt
        std::vector<uint8_t> plaintext(ciphertext.size());
        int len = 0;
        int plaintext_len = 0;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                              static_cast<int>(ciphertext.size())) != 1) {
            return Result<std::string>::error("Failed to decrypt data");
        }
        plaintext_len = len;

        // Set authentication tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                                static_cast<int>(tag.size()),
                                const_cast<unsigned char*>(tag.data())) != 1) {
            return Result<std::string>::error(
                "Failed to set authentication tag");
        }

        // Finalize decryption (this verifies the tag)
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            return Result<std::string>::error(
                "Authentication verification failed");
        }
        plaintext_len += len;

        return Result<std::string>(
            std::string(plaintext.begin(), plaintext.begin() + plaintext_len));

    } catch (const std::exception& e) {
        return Result<std::string>::error(
            std::string("AES-GCM decryption error: ") + e.what());
    }
}

Result<std::vector<uint8_t>> Encryption::encryptAesCbc(
    std::string_view plaintext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {
    try {
        SslCipherContext ctx;

        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(),
                               iv.data()) != 1) {
            return Result<std::vector<uint8_t>>::error(
                "Failed to initialize AES-CBC encryption");
        }

        // Encrypt
        std::vector<uint8_t> ciphertext(plaintext.length() + AES_BLOCK_SIZE);
        int len = 0;
        int ciphertext_len = 0;

        if (EVP_EncryptUpdate(
                ctx, ciphertext.data(), &len,
                reinterpret_cast<const unsigned char*>(plaintext.data()),
                static_cast<int>(plaintext.length())) != 1) {
            return Result<std::vector<uint8_t>>::error(
                "Failed to encrypt data");
        }
        ciphertext_len = len;

        // Finalize encryption (adds padding)
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            return Result<std::vector<uint8_t>>::error(
                "Failed to finalize encryption");
        }
        ciphertext_len += len;
        ciphertext.resize(ciphertext_len);

        return Result<std::vector<uint8_t>>(std::move(ciphertext));

    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(
            std::string("AES-CBC encryption error: ") + e.what());
    }
}

Result<std::string> Encryption::decryptAesCbc(
    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv) {
    try {
        SslCipherContext ctx;

        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(),
                               iv.data()) != 1) {
            return Result<std::string>::error(
                "Failed to initialize AES-CBC decryption");
        }

        // Decrypt
        std::vector<uint8_t> plaintext(ciphertext.size() + AES_BLOCK_SIZE);
        int len = 0;
        int plaintext_len = 0;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(),
                              static_cast<int>(ciphertext.size())) != 1) {
            return Result<std::string>::error("Failed to decrypt data");
        }
        plaintext_len = len;

        // Finalize decryption (removes padding)
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            return Result<std::string>::error(
                "Decryption failed or invalid padding");
        }
        plaintext_len += len;

        return Result<std::string>(
            std::string(plaintext.begin(), plaintext.begin() + plaintext_len));

    } catch (const std::exception& e) {
        return Result<std::string>::error(
            std::string("AES-CBC decryption error: ") + e.what());
    }
}

const EVP_CIPHER* Encryption::getCipher(EncryptionOptions::Method method) {
    switch (method) {
        case EncryptionOptions::Method::AES_GCM:
            return EVP_aes_256_gcm();
        case EncryptionOptions::Method::AES_CBC:
            return EVP_aes_256_cbc();
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return EVP_chacha20_poly1305();
        default:
            return nullptr;
    }
}

size_t Encryption::getKeySize(EncryptionOptions::Method method) {
    switch (method) {
        case EncryptionOptions::Method::AES_GCM:
        case EncryptionOptions::Method::AES_CBC:
            return 32;  // 256 bits
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return 32;  // 256 bits
        default:
            return 0;
    }
}

size_t Encryption::getIvSize(EncryptionOptions::Method method) {
    switch (method) {
        case EncryptionOptions::Method::AES_GCM:
            return 12;  // 96 bits for GCM
        case EncryptionOptions::Method::AES_CBC:
            return 16;  // 128 bits for CBC
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return 12;  // 96 bits
        default:
            return 0;
    }
}

}  // namespace atom::secret
