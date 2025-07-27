#include "encryption.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#include <intrin.h>
#include <immintrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#include <immintrin.h>
#endif

#include "atom/error/exception.hpp"

namespace atom::secret {

// Static member definitions
HardwareCapabilities Encryption::s_hwCapabilities;
bool Encryption::s_initialized = false;

namespace {
// RAII wrapper for OpenSSL EVP_CIPHER_CTX
class CipherContext {
public:
    CipherContext() : ctx_(EVP_CIPHER_CTX_new()) {
        if (!ctx_) {
            spdlog::error("Failed to create OpenSSL cipher context.");
            THROW_RUNTIME_ERROR("Failed to create OpenSSL cipher context.");
        }
    }
    ~CipherContext() {
        if (ctx_) {
            EVP_CIPHER_CTX_free(ctx_);
        }
    }
    CipherContext(const CipherContext&) = delete;
    CipherContext& operator=(const CipherContext&) = delete;
    CipherContext(CipherContext&& other) noexcept : ctx_(other.ctx_) {
        other.ctx_ = nullptr;
    }
    CipherContext& operator=(CipherContext&& other) noexcept {
        if (this != &other) {
            if (ctx_)
                EVP_CIPHER_CTX_free(ctx_);
            ctx_ = other.ctx_;
            other.ctx_ = nullptr;
        }
        return *this;
    }
    EVP_CIPHER_CTX* get() const { return ctx_; }

private:
    EVP_CIPHER_CTX* ctx_;
};

// Secure memory zeroing
void secure_zero(void* ptr, size_t size) {
#ifdef _WIN32
    SecureZeroMemory(ptr, size);
#else
    volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
    while (size--) {
        *p++ = 0;
    }
#endif
}

}  // namespace

// SecureMemoryPool implementation
class SecureMemoryPool::Impl {
public:
    std::unique_ptr<std::vector<unsigned char>> getBuffer(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Try to find a suitable buffer in the pool
        auto it = std::find_if(pool_.begin(), pool_.end(),
            [size](const std::unique_ptr<std::vector<unsigned char>>& buf) {
                return buf && buf->capacity() >= size;
            });

        if (it != pool_.end()) {
            auto buffer = std::move(*it);
            pool_.erase(it);
            buffer->resize(size);
            return buffer;
        }

        // Create new buffer
        auto buffer = std::make_unique<std::vector<unsigned char>>();
        buffer->reserve(std::max(size, static_cast<size_t>(1024))); // Minimum 1KB
        buffer->resize(size);
        return buffer;
    }

    void returnBuffer(std::unique_ptr<std::vector<unsigned char>> buffer) {
        if (!buffer) return;

        std::lock_guard<std::mutex> lock(mutex_);

        // Clear the buffer securely
        secure_zero(buffer->data(), buffer->size());

        // Keep pool size reasonable
        if (pool_.size() < 10) {
            pool_.push_back(std::move(buffer));
        }
        // Otherwise let it be destroyed
    }

private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<std::vector<unsigned char>>> pool_;
};

SecureMemoryPool& SecureMemoryPool::getInstance() {
    static SecureMemoryPool instance;
    if (!instance.pImpl) {
        instance.pImpl = std::make_unique<Impl>();
    }
    return instance;
}

std::unique_ptr<std::vector<unsigned char>> SecureMemoryPool::getBuffer(size_t size) {
    return pImpl->getBuffer(size);
}

void SecureMemoryPool::returnBuffer(std::unique_ptr<std::vector<unsigned char>> buffer) {
    pImpl->returnBuffer(std::move(buffer));
}

// Hardware capability detection
HardwareCapabilities Encryption::detectHardwareCapabilities() {
    HardwareCapabilities caps;

#if defined(__x86_64__) || defined(__i386__)
    unsigned int eax, ebx, ecx, edx;

    // Check for AES-NI (CPUID.01H:ECX.AES[bit 25])
    __cpuid(1, eax, ebx, ecx, edx);
    caps.aesni_available = (ecx & (1 << 25)) != 0;
    caps.rdrand_available = (ecx & (1 << 30)) != 0;

    // Check for AVX2 (CPUID.07H:EBX.AVX2[bit 5])
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    caps.avx2_available = (ebx & (1 << 5)) != 0;
    caps.sha_available = (ebx & (1 << 29)) != 0;

#elif defined(_WIN32)
    int cpuInfo[4];

    // Check for AES-NI
    __cpuid(cpuInfo, 1);
    caps.aesni_available = (cpuInfo[2] & (1 << 25)) != 0;
    caps.rdrand_available = (cpuInfo[2] & (1 << 30)) != 0;

    // Check for AVX2
    __cpuidex(cpuInfo, 7, 0);
    caps.avx2_available = (cpuInfo[1] & (1 << 5)) != 0;
    caps.sha_available = (cpuInfo[1] & (1 << 29)) != 0;
#endif

    return caps;
}

void Encryption::initialize() {
    if (!s_initialized) {
        s_hwCapabilities = detectHardwareCapabilities();
        s_initialized = true;

        spdlog::info("Encryption module initialized. Hardware capabilities: "
                    "AES-NI={}, AVX2={}, SHA={}, RDRAND={}",
                    s_hwCapabilities.aesni_available,
                    s_hwCapabilities.avx2_available,
                    s_hwCapabilities.sha_available,
                    s_hwCapabilities.rdrand_available);
    }
}

const HardwareCapabilities& Encryption::getHardwareCapabilities() {
    if (!s_initialized) {
        initialize();
    }
    return s_hwCapabilities;
}

Result<std::vector<unsigned char>> Encryption::deriveKey(
    std::string_view password,
    std::string_view salt,
    int iterations,
    int key_len) {

    if (password.empty() || salt.empty() || key_len <= 0 || iterations <= 0) {
        return Result<std::vector<unsigned char>>::Failure(
            ErrorCode::InvalidArgument, std::string("Invalid parameters for key derivation"));
    }

    auto buffer = SecureMemoryPool::getInstance().getBuffer(key_len);

    if (PKCS5_PBKDF2_HMAC(password.data(), password.length(),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          salt.length(), iterations, EVP_sha256(), key_len,
                          buffer->data()) == 0) {
        spdlog::error("PBKDF2 key derivation failed.");
        return Result<std::vector<unsigned char>>::Failure(
            ErrorCode::KeyDerivationFailed, std::string("PBKDF2 key derivation failed"));
    }

    std::vector<unsigned char> key(buffer->begin(), buffer->end());
    SecureMemoryPool::getInstance().returnBuffer(std::move(buffer));

    return Result<std::vector<unsigned char>>(std::move(key));
}

// Legacy method for backward compatibility
std::vector<unsigned char> Encryption::derive_key(std::string_view password,
                                                  std::string_view salt,
                                                  int key_len) {
    auto result = deriveKey(password, salt, 100000, key_len);
    if (result.isError()) {
        spdlog::error("Legacy derive_key failed: {}", result.errorMessage());
        THROW_RUNTIME_ERROR("PBKDF2 key derivation failed.");
    }
    return result.value();
}

Result<std::vector<unsigned char>> Encryption::encrypt(
    std::string_view plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad,
    const EncryptionOptions& options) {

    // Dispatch to appropriate encryption method
    switch (options.encryptionMethod) {
        case EncryptionOptions::Method::AES_GCM:
        case EncryptionOptions::Method::AES_256_GCM:
        case EncryptionOptions::Method::AES_128_GCM:
            return encryptAESGCM(plaintext, key, iv, aad);
        case EncryptionOptions::Method::AES_CBC:
            return encryptAESCBC(plaintext, key, iv);
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return encryptChaCha20Poly1305(plaintext, key, iv, aad);
        default:
            return Result<std::vector<unsigned char>>::Failure(
                ErrorCode::InvalidArgument, std::string("Unsupported encryption method"));
    }
}

Result<std::vector<unsigned char>> Encryption::encryptAESGCM(
    std::string_view plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad) {
    CipherContext ctx;
    int len;
    int ciphertext_len;
    std::vector<unsigned char> ciphertext(plaintext.length() +
                                          16);  // 16 for GCM tag

    if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                                nullptr)) {
        return Result<std::vector<unsigned char>>::Failure("EncryptInit failed.");
    }
    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(),
                                 nullptr)) {
        return Result<std::vector<unsigned char>>::Failure(
            "Setting IV length failed.");
    }
    if (1 != EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(),
                                iv.data())) {
        return Result<std::vector<unsigned char>>::Failure(
            "EncryptInit with key and IV failed.");
    }
    if (1 !=
        EVP_EncryptUpdate(ctx.get(), nullptr, &len, aad.data(), aad.size())) {
        return Result<std::vector<unsigned char>>::Failure(
            "EncryptUpdate for AAD failed.");
    }
    if (1 != EVP_EncryptUpdate(
                 ctx.get(), ciphertext.data(), &len,
                 reinterpret_cast<const unsigned char*>(plaintext.data()),
                 plaintext.size())) {
        return Result<std::vector<unsigned char>>::Failure(
            "EncryptUpdate for plaintext failed.");
    }
    ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len)) {
        return Result<std::vector<unsigned char>>::Failure(
            "EncryptFinal failed.");
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    std::vector<unsigned char> tag(16);
    if (1 !=
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data())) {
        return Result<std::vector<unsigned char>>::Failure(
            "Getting GCM tag failed.");
    }

    // Append tag to ciphertext
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());

    return Result(std::move(ciphertext));
}

Result<std::string> Encryption::decrypt(
    const std::vector<unsigned char>& ciphertext_with_tag,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad,
    const EncryptionOptions& options) {

    // Dispatch to appropriate decryption method
    switch (options.encryptionMethod) {
        case EncryptionOptions::Method::AES_GCM:
        case EncryptionOptions::Method::AES_256_GCM:
        case EncryptionOptions::Method::AES_128_GCM:
            return decryptAESGCM(ciphertext_with_tag, key, iv, aad);
        case EncryptionOptions::Method::AES_CBC:
            return decryptAESCBC(ciphertext_with_tag, key, iv);
        case EncryptionOptions::Method::CHACHA20_POLY1305:
            return decryptChaCha20Poly1305(ciphertext_with_tag, key, iv, aad);
        default:
            return Result<std::string>::Failure(
                ErrorCode::InvalidArgument, std::string("Unsupported encryption method"));
    }
}

Result<std::string> Encryption::decryptAESGCM(
    const std::vector<unsigned char>& ciphertext_with_tag,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad) {
    if (ciphertext_with_tag.size() < 16) {
        return Result<std::string>(
            "Invalid ciphertext: too short to contain a tag.");
    }

    std::vector<unsigned char> tag(ciphertext_with_tag.end() - 16,
                                   ciphertext_with_tag.end());
    std::vector<unsigned char> ciphertext(ciphertext_with_tag.begin(),
                                          ciphertext_with_tag.end() - 16);

    CipherContext ctx;
    int len;
    int plaintext_len;
    std::string plaintext;
    plaintext.resize(ciphertext.size());

    if (1 != EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                                nullptr)) {
        return Result<std::string>("DecryptInit failed.");
    }
    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(),
                                 nullptr)) {
        return Result<std::string>("Setting IV length failed.");
    }
    if (1 != EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(),
                                iv.data())) {
        return Result<std::string>("DecryptInit with key and IV failed.");
    }
    if (1 !=
        EVP_DecryptUpdate(ctx.get(), nullptr, &len, aad.data(), aad.size())) {
        return Result<std::string>("DecryptUpdate for AAD failed.");
    }
    if (1 != EVP_DecryptUpdate(ctx.get(),
                               reinterpret_cast<unsigned char*>(&plaintext[0]),
                               &len, ciphertext.data(), ciphertext.size())) {
        return Result<std::string>("DecryptUpdate for ciphertext failed.");
    }
    plaintext_len = len;

    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(),
                                 (void*)tag.data())) {
        return Result<std::string>("Setting GCM tag failed.");
    }

    int ret = EVP_DecryptFinal_ex(
        ctx.get(), reinterpret_cast<unsigned char*>(&plaintext[0]) + len, &len);

    if (ret > 0) {
        plaintext_len += len;
        plaintext.resize(plaintext_len);
        return Result(std::move(plaintext));
    } else {
        return Result<std::string>(
            "Decryption failed: GCM tag verification failed.");
    }
}

Result<std::vector<unsigned char>> Encryption::randomBytes(int len, bool use_hardware_rng) {
    if (len <= 0) {
        return Result<std::vector<unsigned char>>::Failure(
            ErrorCode::InvalidArgument, std::string("Invalid length for random bytes"));
    }

    auto buffer = SecureMemoryPool::getInstance().getBuffer(len);

    int result = 0;
    if (use_hardware_rng && getHardwareCapabilities().rdrand_available) {
        // Try hardware RNG first
        result = RAND_bytes(buffer->data(), len);
    } else {
        // Use OpenSSL's PRNG
        result = RAND_bytes(buffer->data(), len);
    }

    if (result != 1) {
        spdlog::error("Failed to generate random bytes.");
        return Result<std::vector<unsigned char>>::Failure(
            ErrorCode::PlatformError, std::string("Failed to generate random bytes"));
    }

    std::vector<unsigned char> bytes(buffer->begin(), buffer->end());
    SecureMemoryPool::getInstance().returnBuffer(std::move(buffer));

    return Result<std::vector<unsigned char>>(std::move(bytes));
}

// Legacy method for backward compatibility
std::vector<unsigned char> Encryption::random_bytes(int len) {
    auto result = randomBytes(len, true);
    if (result.isError()) {
        spdlog::error("Legacy random_bytes failed: {}", result.errorMessage());
        THROW_RUNTIME_ERROR("Failed to generate random bytes.");
    }
    return result.value();
}

// Placeholder implementations for additional encryption methods
Result<std::vector<unsigned char>> Encryption::encryptAESCBC(
    std::string_view plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv) {
    // For now, fall back to AES-GCM
    std::vector<unsigned char> empty_aad;
    return encryptAESGCM(plaintext, key, iv, empty_aad);
}

Result<std::vector<unsigned char>> Encryption::encryptChaCha20Poly1305(
    std::string_view plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad) {
    // For now, fall back to AES-GCM
    return encryptAESGCM(plaintext, key, iv, aad);
}

Result<std::string> Encryption::decryptAESCBC(
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv) {
    // For now, fall back to AES-GCM
    std::vector<unsigned char> empty_aad;
    return decryptAESGCM(ciphertext, key, iv, empty_aad);
}

Result<std::string> Encryption::decryptChaCha20Poly1305(
    const std::vector<unsigned char>& ciphertext_with_tag,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad) {
    // For now, fall back to AES-GCM
    return decryptAESGCM(ciphertext_with_tag, key, iv, aad);
}

const EVP_CIPHER* Encryption::getCipher(EncryptionOptions::Method method, int keySize) {
    switch (method) {
        case EncryptionOptions::Method::AES_128_GCM:
            return EVP_aes_128_gcm();
        case EncryptionOptions::Method::AES_GCM:
        case EncryptionOptions::Method::AES_256_GCM:
            return EVP_aes_256_gcm();
        case EncryptionOptions::Method::AES_CBC:
            return keySize == 16 ? EVP_aes_128_cbc() : EVP_aes_256_cbc();
        default:
            return EVP_aes_256_gcm();
    }
}

}  // namespace atom::secret
