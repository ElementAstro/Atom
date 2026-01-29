# atom/secret - Security and Cryptography Module

> **Module Version:** 1.0.0
> **Documentation Version:** 1.0.0
> **Last Updated:** 2025-01-15

---

## Navigation

[Root Directory](../../CLAUDE.md) > **secret**

---

## Module Overview

The **atom::secret** module provides comprehensive security and cryptography utilities for the Atom framework. It offers password management, encryption, secure storage, and one-time password (OTP) functionality.

### Key Features

- **Password Management**: Secure password storage with master password protection
- **Encryption**: AES-256-GCM encryption with key derivation
- **Secure Memory**: Memory that is zeroed on destruction
- **Hash Functions**: SHA-256, SHA-512, bcrypt support
- **HMAC**: Message authentication codes
- **OTP Support**: TOTP and HOTP for two-factor authentication
- **Password Generation**: Secure random password generation
- **Password Validation**: Strength checking and breach detection
- **Secure Storage**: Encrypted file-based password vault
- **Audit Logging**: Track all password access operations

---

## Directory Structure

```
atom/secret/
├── core/              # Core types and utilities
│   ├── error_codes.hpp
│   ├── types.hpp
│   ├── result.hpp
│   └── utils.hpp
├── crypto/            # Cryptographic functions
│   ├── encryption.hpp
│   ├── encryption.cpp
│   ├── hash.hpp
│   ├── hash.cpp
│   ├── hmac.hpp
│   ├── hmac.cpp
│   ├── key_derivation.hpp
│   ├── key_derivation.cpp
│   └── secure_memory.hpp
│   └── secure_memory.cpp
├── password/          # Password utilities
│   ├── entry.hpp
│   ├── generator.hpp
│   ├── generator.cpp
│   ├── validator.hpp
│   └── validator.cpp
│   └── breach_checker.hpp
│   └── breach_checker.cpp
├── otp/               # One-time passwords
│   ├── totp.hpp
│   ├── totp.cpp
│   ├── hotp.hpp
│   └── hotp.cpp
├── storage/           # Secure storage
│   ├── storage.hpp
│   ├── storage.cpp
│   ├── file_storage.hpp
│   ├── file_storage.cpp
│   └── backup.hpp
│   └── backup.cpp
├── manager/           # Password manager
│   ├── password_manager.hpp
│   ├── password_manager.cpp
│   ├── session.hpp
│   ├── session.cpp
│   └── audit_log.hpp
│   └── audit_log.cpp
└── serialization/     # JSON serialization
    ├── json.hpp
    └── json.cpp
```

---

## Core Components

### Password Manager

```cpp
#include "atom/secret/manager/password_manager.hpp"

using namespace atom::secret;

// Create password manager
PasswordManager manager(
    std::make_unique<FileStorage>("vault.enc"),
    PasswordManagerSettings::defaults()
);

// Initialize with master password
manager.initialize("myMasterPassword123!");

// Add password entry
PasswordEntry entry;
entry.title = "Gmail";
entry.username = "user@gmail.com";
entry.password = "myPassword";
entry.url = "https://gmail.com";
entry.category = PasswordCategory::Email;
manager.addEntry(entry);

// Search entries
SearchFilter filter;
filter.query = "gmail";
auto results = manager.search(filter);

// Export with encryption
auto json = manager.exportToJson("exportPassword");

// Lock when done
manager.lock();
```

### Encryption

```cpp
#include "atom/secret/crypto/encryption.hpp"

using namespace atom::secret;

// Generate key
auto key = Encryption::generateKey();

// Encrypt data
std::string plaintext = "Sensitive data";
auto encrypted = Encryption::encrypt(plaintext, key);

// Decrypt data
auto decrypted = Encryption::decrypt(encrypted, key);

// Encrypt with custom parameters
EncryptionParams params;
params.algorithm = EncryptionAlgorithm::AES_256_GCM;
params.keyDerivation = KeyDerivationAlgorithm::Argon2id;

auto encrypted2 = Encryption::encrypt(plaintext, key, params);
```

### Secure Memory

```cpp
#include "atom/secret/crypto/secure_memory.hpp"

using namespace atom::secret;

// Allocate secure memory
SecureBuffer buffer(1024);  // 1024 bytes
std::strcpy(buffer.data(), "Secret data");

// Buffer is automatically zeroed on destruction

// Secure string
SecureString secret("Sensitive password");
// Automatically zeroed on destruction
```

### Password Generation

```cpp
#include "atom/secret/password/generator.hpp"

using namespace atom::secret;

// Generate password with custom settings
PasswordGenerator generator;
generator.setLength(16);
generator.setIncludeUppercase(true);
generator.setIncludeLowercase(true);
generator.setIncludeDigits(true);
generator.setIncludeSymbols(true);

std::string password = generator.generate();
// Example: "aB3$xY9@kL2&mN4"
```

### Password Validation

```cpp
#include "atom/secret/password/validator.hpp"

using namespace atom::secret;

PasswordValidator validator;

// Set requirements
validator.setMinLength(12);
validator.setRequireUppercase(true);
validator.setRequireLowercase(true);
validator.setRequireDigits(true);
validator.setRequireSymbols(true);

// Validate password
auto result = validator.validate("MyPass123!");
if (!result.isValid()) {
    std::cerr << "Validation failed: " << result.getErrorMessage() << "\n";
}
```

### OTP (One-Time Passwords)

```cpp
#include "atom/secret/otp/totp.hpp"

using namespace atom::secret;

// Generate TOTP (Time-based OTP)
TOTP totp("JBSWY3DPEHPK3PXP",  // Base32 secret
          30,                    // 30 second time step
          6                      // 6 digit code
);

std::string code = totp.generate();
std::cout << "Current TOTP: " << code << "\n";

// Verify TOTP
bool valid = totp.verify(code);
```

---

## Public Interfaces

### PasswordManager Class

```cpp
class PasswordManager {
public:
    explicit PasswordManager(std::unique_ptr<SecureStorage> storage,
                             const PasswordManagerSettings& settings);

    // Initialization
    Result<void> initialize(std::string_view masterPassword);
    Result<void> unlock(std::string_view masterPassword);
    void lock();
    bool isUnlocked() const;

    // Entry management
    Result<std::string> addEntry(const PasswordEntry& entry);
    Result<void> updateEntry(const PasswordEntry& entry);
    Result<void> deleteEntry(const std::string& entryId);
    Result<PasswordEntry> getEntry(const std::string& entryId);
    Result<std::vector<PasswordEntry>> getAllEntries();
    Result<std::vector<PasswordEntry>> search(const SearchFilter& filter);

    // Password operations
    Result<std::string> getPassword(const std::string& entryId);
    Result<void> updatePassword(const std::string& entryId,
                                const std::string& newPassword);

    // Import/Export
    Result<std::string> exportToJson(const std::string& password = "");
    Result<int> importFromJson(const std::string& json,
                              const std::string& password = "",
                              bool overwrite = false);

    // Settings and session
    const PasswordManagerSettings& getSettings() const;
    void updateSettings(const PasswordManagerSettings& settings);
    SessionManager& getSession();
    AuditLog* getAuditLog();
};
```

### Encryption Class

```cpp
class Encryption {
public:
    // Key management
    static std::vector<std::byte> generateKey(
        size_t keySize = 32);  // 256-bit default

    // Encryption/decryption
    static std::vector<std::byte> encrypt(std::span<const std::byte> plaintext,
                                          std::span<const std::byte> key,
                                          const EncryptionParams& params = {});
    static std::vector<std::byte> decrypt(std::span<const std::byte> ciphertext,
                                          std::span<const std::byte> key,
                                          const EncryptionParams& params = {});

    // Utility functions
    static std::vector<std::byte> generateIV();
    static std::vector<std::byte> generateSalt();
};
```

### PasswordGenerator Class

```cpp
class PasswordGenerator {
public:
    PasswordGenerator();

    // Configuration
    void setLength(size_t length);
    void setIncludeUppercase(bool include);
    void setIncludeLowercase(bool include);
    void setIncludeDigits(bool include);
    void setIncludeSymbols(bool include);
    void setExcludeAmbiguous(bool exclude);
    void setCustomCharset(const std::string& charset);

    // Generation
    std::string generate() const;
    std::vector<std::string> generateBatch(size_t count) const;

    // Passphrase generation
    std::string generatePassphrase(size_t wordCount = 5) const;
};
```

### PasswordValidator Class

```cpp
class PasswordValidator {
public:
    PasswordValidator();

    // Requirements
    void setMinLength(size_t length);
    void setMaxLength(size_t length);
    void setRequireUppercase(bool require);
    void setRequireLowercase(bool require);
    void setRequireDigits(bool require);
    void setRequireSymbols(bool require);

    // Validation
    ValidationResult validate(std::string_view password) const;
    int calculateStrength(std::string_view password) const;  // 0-100

    // Breach checking (requires internet)
    std::future<bool> checkBreachAsync(std::string_view password) const;
};
```

---

## Dependencies

### Required Dependencies

- **atom::algorithm**: Cryptographic algorithms
- **atom::io**: I/O operations
- **atom::type**: Type utilities
- **atom::utils**: Utility functions
- **OpenSSL**: Cryptographic operations

### Optional Dependencies

- **spdlog**: Enhanced logging
- **libsecret**: Linux keyring integration
- **glib-2.0**: Linux keyring support

### Platform-Specific Libraries

**Linux:**

- libsecret-1 (for system keyring integration)

---

## Build Configuration

### CMake Options

```cmake
# Build secret module
-DBUILD_SECRET=ON

# Requires OpenSSL for cryptography
# On Linux: sudo apt-get install libssl-dev
# On macOS: brew install openssl
```

### OpenSSL Detection

The module automatically finds OpenSSL via `find_package(OpenSSL)`. Ensure OpenSSL is installed on your system.

---

## Usage Examples

### Simple Password Storage

```cpp
#include "atom/secret/manager/password_manager.hpp"

using namespace atom::secret;

// Create and initialize manager
PasswordManager manager(std::make_unique<FileStorage>("vault.enc"));
manager.initialize("masterPassword123!");

// Add entry
PasswordEntry entry;
entry.title = "My Service";
entry.username = "user@example.com";
entry.password = "SecurePass123!";
entry.url = "https://example.com";
manager.addEntry(entry);

// Retrieve password
auto password = manager.getPassword(entry.title);
std::cout << "Password: " << *password << "\n";
```

### Encryption of Sensitive Data

```cpp
#include "atom/secret/crypto/encryption.hpp"
#include "atom/secret/crypto/key_derivation.hpp"

using namespace atom::secret;

// Derive key from password
auto salt = Encryption::generateSalt();
auto key = KeyDerivation::deriveKey("userPassword", salt, 100000);

// Encrypt sensitive data
std::string data = "Sensitive information";
auto encrypted = Encryption::encrypt(
    std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()),
    key
);

// Save encrypted data to file
std::ofstream out("encrypted.dat", std::ios::binary);
out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
```

### TOTP Authentication

```cpp
#include "atom/secret/otp/totp.hpp"

using namespace atom::secret;

// Create TOTP with shared secret
TOTP totp("JBSWY3DPEHPK3PXP");  // Base32 encoded secret

// Generate current code
std::string code = totp.generate();
std::cout << "Enter this code: " << code << "\n";

// Verify user input
std::string userInput;
std::cin >> userInput;

if (totp.verify(userInput)) {
    std::cout << "Authentication successful!\n";
} else {
    std::cout << "Invalid code!\n";
}
```

---

## Security Considerations

### Memory Security

```cpp
// Good: Use SecureMemory for sensitive data
SecureBuffer buffer(256);
std::strcpy(buffer.data(), sensitiveData);
// Automatically zeroed on destruction

// Bad: Regular std::string leaves data in memory
std::string data = sensitiveData;  // May be paged to disk!
```

### Key Derivation

```cpp
// Good: Use Argon2id with sufficient iterations
auto key = KeyDerivation::deriveKey(
    "password",
    salt,
    100000,  // High iteration count
    KeyDerivationAlgorithm::Argon2id
);

// Bad: Fast/weak key derivation
auto weakKey = KeyDerivation::deriveKey(
    "password",
    salt,
    1000,  // Too few iterations
    KeyDerivationAlgorithm::PBKDF2  // Less secure
);
```

### Password Storage

```cpp
// Good: Never store plaintext passwords
manager.addEntry(entry);  // Encrypted with master password

// Bad: Storing passwords in plaintext
std::ofstream out("passwords.txt");
out << entry.password << "\n";  // Security risk!
```

---

## Testing

### Test Organization

Tests are located in `tests/secret/`:

- `test_encryption.cpp`: Encryption/decryption tests
- `test_password.cpp`: Password generation/validation tests
- `test_otp.cpp`: OTP tests
- `test_manager.cpp`: Password manager tests
- `integration_test.cpp`: Integration tests

### Running Tests

```bash
# Build tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run secret tests
ctest -R secret_ --output-on-failure
```

---

## Best Practices

### DO

- Always use SecureMemory for sensitive data
- Use strong master passwords with sufficient entropy
- Enable audit logging in production
- Regularly backup encrypted vaults
- Use TOTP for two-factor authentication
- Set appropriate password policies

### DON'T

- Store passwords in plaintext
- Use weak or default passwords
- Share master passwords
- Disable encryption for convenience
- Skip security validation

---

## Related Modules

- **atom::algorithm**: Cryptographic algorithms
- **atom::io**: File I/O for secure storage
- **atom::utils**: Random number generation

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented crypto, password, OTP, manager components
- Added security considerations and best practices

---

**Maintained By:** Atom Framework Team
