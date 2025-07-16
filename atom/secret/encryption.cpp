#include "encryption.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <spdlog/spdlog.h>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::secret {

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
}  // namespace

std::vector<unsigned char> Encryption::derive_key(std::string_view password,
                                                  std::string_view salt,
                                                  int key_len) {
    std::vector<unsigned char> key(key_len);
    if (PKCS5_PBKDF2_HMAC(password.data(), password.length(),
                          reinterpret_cast<const unsigned char*>(salt.data()),
                          salt.length(), 100000, EVP_sha256(), key_len,
                          key.data()) == 0) {
        spdlog::error("PBKDF2 key derivation failed.");
        THROW_RUNTIME_ERROR("PBKDF2 key derivation failed.");
    }
    return key;
}

Result<std::vector<unsigned char>> Encryption::encrypt(
    std::string_view plaintext, const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& aad) {
    CipherContext ctx;
    int len;
    int ciphertext_len;
    std::vector<unsigned char> ciphertext(plaintext.length() +
                                          16);  // 16 for GCM tag

    if (1 != EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                                nullptr)) {
        return Result<std::vector<unsigned char>>::Error("EncryptInit failed.");
    }
    if (1 != EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, iv.size(),
                                 nullptr)) {
        return Result<std::vector<unsigned char>>::Error(
            "Setting IV length failed.");
    }
    if (1 != EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(),
                                iv.data())) {
        return Result<std::vector<unsigned char>>::Error(
            "EncryptInit with key and IV failed.");
    }
    if (1 !=
        EVP_EncryptUpdate(ctx.get(), nullptr, &len, aad.data(), aad.size())) {
        return Result<std::vector<unsigned char>>::Error(
            "EncryptUpdate for AAD failed.");
    }
    if (1 != EVP_EncryptUpdate(
                 ctx.get(), ciphertext.data(), &len,
                 reinterpret_cast<const unsigned char*>(plaintext.data()),
                 plaintext.size())) {
        return Result<std::vector<unsigned char>>::Error(
            "EncryptUpdate for plaintext failed.");
    }
    ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len)) {
        return Result<std::vector<unsigned char>>::Error(
            "EncryptFinal failed.");
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    std::vector<unsigned char> tag(16);
    if (1 !=
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data())) {
        return Result<std::vector<unsigned char>>::Error(
            "Getting GCM tag failed.");
    }

    // Append tag to ciphertext
    ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());

    return Result(std::move(ciphertext));
}

Result<std::string> Encryption::decrypt(
    const std::vector<unsigned char>& ciphertext_with_tag,
    const std::vector<unsigned char>& key, const std::vector<unsigned char>& iv,
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

std::vector<unsigned char> Encryption::random_bytes(int len) {
    std::vector<unsigned char> bytes(len);
    if (RAND_bytes(bytes.data(), len) != 1) {
        spdlog::error("Failed to generate random bytes.");
        THROW_RUNTIME_ERROR("Failed to generate random bytes.");
    }
    return bytes;
}

}  // namespace atom::secret
