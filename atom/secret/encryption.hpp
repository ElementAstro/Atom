#ifndef ATOM_SECRET_ENCRYPTION_HPP
#define ATOM_SECRET_ENCRYPTION_HPP

#include <openssl/err.h>

namespace atom::secret {

// Forward declaration for OpenSSL context
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

/**
 * @brief RAII wrapper for OpenSSL EVP_CIPHER_CTX.
 * Ensures the context is properly freed.
 */
class SslCipherContext {
private:
    EVP_CIPHER_CTX* ctx;  ///< Pointer to the OpenSSL cipher context.

public:
    /**
     * @brief Constructs an SslCipherContext, creating a new EVP_CIPHER_CTX.
     * @throws std::runtime_error if context creation fails.
     */
    SslCipherContext();

    /**
     * @brief Destroys the SslCipherContext, freeing the EVP_CIPHER_CTX.
     */
    ~SslCipherContext();

    // Disable copy construction and assignment
    SslCipherContext(const SslCipherContext&) = delete;
    SslCipherContext& operator=(const SslCipherContext&) = delete;

    // Enable move construction and assignment
    SslCipherContext(SslCipherContext&& other) noexcept;
    SslCipherContext& operator=(SslCipherContext&& other) noexcept;

    /**
     * @brief Gets the raw pointer to the EVP_CIPHER_CTX.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    EVP_CIPHER_CTX* get() const noexcept { return ctx; }

    /**
     * @brief Implicit conversion to the raw EVP_CIPHER_CTX pointer.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    operator EVP_CIPHER_CTX*() const noexcept { return ctx; }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_ENCRYPTION_HPP
