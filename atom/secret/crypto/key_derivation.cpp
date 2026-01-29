#include "key_derivation.hpp"

#include <chrono>

#include <openssl/evp.h>
#include <openssl/rand.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#endif

namespace atom::secret {

Result<std::vector<uint8_t>> KeyDerivation::pbkdf2Sha256(
    std::string_view password, const std::vector<uint8_t>& salt, int iterations,
    size_t keyLength) {
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (salt.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Salt cannot be empty");
    }

    if (iterations < 1000) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument,
            "Iteration count too low (minimum 1000)");
    }

    if (keyLength == 0 || keyLength > 1024) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Invalid key length");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    int result = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.length()), salt.data(),
        static_cast<int>(salt.size()), iterations, EVP_sha256(),
        static_cast<int>(keyLength), derivedKey.data());

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed,
            "PBKDF2-SHA256 key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
}

Result<std::vector<uint8_t>> KeyDerivation::pbkdf2Sha512(
    std::string_view password, const std::vector<uint8_t>& salt, int iterations,
    size_t keyLength) {
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (salt.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Salt cannot be empty");
    }

    if (iterations < 1000) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument,
            "Iteration count too low (minimum 1000)");
    }

    if (keyLength == 0 || keyLength > 1024) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Invalid key length");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    int result = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.length()), salt.data(),
        static_cast<int>(salt.size()), iterations, EVP_sha512(),
        static_cast<int>(keyLength), derivedKey.data());

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed,
            "PBKDF2-SHA512 key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
}

Result<std::vector<uint8_t>> KeyDerivation::pbkdf2(
    std::string_view password, const std::vector<uint8_t>& salt,
    const Pbkdf2Params& params, size_t keyLength,
    KeyDerivationAlgorithm algorithm) {
    switch (algorithm) {
        case KeyDerivationAlgorithm::PBKDF2_SHA256:
            return pbkdf2Sha256(password, salt, params.iterations, keyLength);
        case KeyDerivationAlgorithm::PBKDF2_SHA512:
            return pbkdf2Sha512(password, salt, params.iterations, keyLength);
        default:
            return Result<std::vector<uint8_t>>::error(
                ErrorCode::UnsupportedAlgorithm,
                "Unsupported PBKDF2 algorithm");
    }
}

Result<std::vector<uint8_t>> KeyDerivation::argon2id(
    std::string_view password, const std::vector<uint8_t>& salt,
    const Argon2Params& params, size_t keyLength) {
    // Check if Argon2 is available via OpenSSL 3.2+
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (salt.size() < 8) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument, "Salt must be at least 8 bytes");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (!kdf) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::UnsupportedAlgorithm, "Argon2id not available");
    }

    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);

    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Failed to create Argon2 context");
    }

    OSSL_PARAM kdfParams[6];
    kdfParams[0] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(password.data()),
        password.length());
    kdfParams[1] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT, const_cast<uint8_t*>(salt.data()), salt.size());
    kdfParams[2] = OSSL_PARAM_construct_uint32(
        OSSL_KDF_PARAM_ITER, const_cast<uint32_t*>(&params.timeCost));
    kdfParams[3] =
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST,
                                    const_cast<uint32_t*>(&params.memoryCost));
    kdfParams[4] = OSSL_PARAM_construct_uint32(
        OSSL_KDF_PARAM_THREADS, const_cast<uint32_t*>(&params.parallelism));
    kdfParams[5] = OSSL_PARAM_construct_end();

    int result = EVP_KDF_derive(ctx, derivedKey.data(), keyLength, kdfParams);
    EVP_KDF_CTX_free(ctx);

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Argon2id key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
#else
    (void)password;
    (void)salt;
    (void)params;
    (void)keyLength;
    return Result<std::vector<uint8_t>>::error(
        ErrorCode::UnsupportedAlgorithm,
        "Argon2 requires OpenSSL 3.2 or later");
#endif
}

Result<std::vector<uint8_t>> KeyDerivation::argon2i(
    std::string_view password, const std::vector<uint8_t>& salt,
    const Argon2Params& params, size_t keyLength) {
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (salt.size() < 8) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument, "Salt must be at least 8 bytes");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2I", nullptr);
    if (!kdf) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::UnsupportedAlgorithm, "Argon2i not available");
    }

    EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);

    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Failed to create Argon2 context");
    }

    OSSL_PARAM kdfParams[6];
    kdfParams[0] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(password.data()),
        password.length());
    kdfParams[1] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT, const_cast<uint8_t*>(salt.data()), salt.size());
    kdfParams[2] = OSSL_PARAM_construct_uint32(
        OSSL_KDF_PARAM_ITER, const_cast<uint32_t*>(&params.timeCost));
    kdfParams[3] =
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST,
                                    const_cast<uint32_t*>(&params.memoryCost));
    kdfParams[4] = OSSL_PARAM_construct_uint32(
        OSSL_KDF_PARAM_THREADS, const_cast<uint32_t*>(&params.parallelism));
    kdfParams[5] = OSSL_PARAM_construct_end();

    int result = EVP_KDF_derive(ctx, derivedKey.data(), keyLength, kdfParams);
    EVP_KDF_CTX_free(ctx);

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Argon2i key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
#else
    (void)password;
    (void)salt;
    (void)params;
    (void)keyLength;
    return Result<std::vector<uint8_t>>::error(
        ErrorCode::UnsupportedAlgorithm,
        "Argon2 requires OpenSSL 3.2 or later");
#endif
}

Result<std::vector<uint8_t>> KeyDerivation::scrypt(
    std::string_view password, const std::vector<uint8_t>& salt,
    const ScryptParams& params, size_t keyLength) {
    if (password.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Password cannot be empty");
    }

    if (salt.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Salt cannot be empty");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    int result = EVP_PBE_scrypt(password.data(), password.length(), salt.data(),
                                salt.size(), params.n, params.r, params.p,
                                0,  // maxmem (0 = default)
                                derivedKey.data(), keyLength);

    if (result != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "scrypt key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
}

Result<std::vector<uint8_t>> KeyDerivation::generateSalt(size_t length) {
    return generateRandomBytes(length);
}

Result<std::vector<uint8_t>> KeyDerivation::generateKey(size_t length) {
    return generateRandomBytes(length);
}

Result<std::vector<uint8_t>> KeyDerivation::generateRandomBytes(size_t length) {
    if (length == 0 || length > 1024 * 1024) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::InvalidArgument, "Invalid length for random bytes");
    }

    std::vector<uint8_t> bytes(length);

    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::RandomGenerationFailed,
            "Failed to generate random bytes");
    }

    return Result<std::vector<uint8_t>>::success(std::move(bytes));
}

Result<std::vector<uint8_t>> KeyDerivation::hkdf(
    const std::vector<uint8_t>& inputKey, const std::vector<uint8_t>& salt,
    const std::vector<uint8_t>& info, size_t keyLength) {
    if (inputKey.empty()) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Input key cannot be empty");
    }

    if (keyLength == 0 || keyLength > 255 * 32) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidArgument,
                                                   "Invalid output key length");
    }

    std::vector<uint8_t> derivedKey(keyLength);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!ctx) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "Failed to create HKDF context");
    }

    bool success = true;

    if (EVP_PKEY_derive_init(ctx) <= 0) {
        success = false;
    }

    if (success && EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) <= 0) {
        success = false;
    }

    if (success && !salt.empty()) {
        if (EVP_PKEY_CTX_set1_hkdf_salt(ctx, salt.data(),
                                        static_cast<int>(salt.size())) <= 0) {
            success = false;
        }
    }

    if (success &&
        EVP_PKEY_CTX_set1_hkdf_key(ctx, inputKey.data(),
                                   static_cast<int>(inputKey.size())) <= 0) {
        success = false;
    }

    if (success && !info.empty()) {
        if (EVP_PKEY_CTX_add1_hkdf_info(ctx, info.data(),
                                        static_cast<int>(info.size())) <= 0) {
            success = false;
        }
    }

    size_t outLen = keyLength;
    if (success && EVP_PKEY_derive(ctx, derivedKey.data(), &outLen) <= 0) {
        success = false;
    }

    EVP_PKEY_CTX_free(ctx);

    if (!success) {
        return Result<std::vector<uint8_t>>::error(
            ErrorCode::KeyDerivationFailed, "HKDF key derivation failed");
    }

    return Result<std::vector<uint8_t>>::success(std::move(derivedKey));
}

bool KeyDerivation::isArgon2Available() noexcept {
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (kdf) {
        EVP_KDF_free(kdf);
        return true;
    }
#endif
    return false;
}

bool KeyDerivation::isScryptAvailable() noexcept {
    // scrypt is available in OpenSSL 1.1.0+
    return true;
}

int KeyDerivation::benchmarkIterations(int targetMilliseconds,
                                       KeyDerivationAlgorithm algorithm) {
    const std::string testPassword = "benchmark_password_12345";
    std::vector<uint8_t> testSalt(32, 0x42);

    int iterations = 10000;
    int step = 10000;

    for (int attempt = 0; attempt < 10; ++attempt) {
        auto start = std::chrono::high_resolution_clock::now();

        Result<std::vector<uint8_t>> result =
            Result<std::vector<uint8_t>>::error(ErrorCode::Unknown);
        switch (algorithm) {
            case KeyDerivationAlgorithm::PBKDF2_SHA256:
                result = pbkdf2Sha256(testPassword, testSalt, iterations, 32);
                break;
            case KeyDerivationAlgorithm::PBKDF2_SHA512:
                result = pbkdf2Sha512(testPassword, testSalt, iterations, 32);
                break;
            default:
                return iterations;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

        if (duration >= targetMilliseconds) {
            return iterations;
        }

        // Estimate iterations needed
        if (duration > 0) {
            int estimated =
                static_cast<int>(iterations * targetMilliseconds / duration);
            step = (estimated - iterations) / 2;
            if (step < 1000)
                step = 1000;
        }

        iterations += step;
    }

    return iterations;
}

}  // namespace atom::secret
