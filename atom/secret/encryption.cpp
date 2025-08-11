#include "encryption.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>

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

}  // namespace atom::secret
