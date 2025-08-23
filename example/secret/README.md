# Atom Secret Module Examples

This directory contains examples demonstrating the secure storage and cryptographic capabilities of the Atom framework.

## 🚀 Overview

The Atom secret module provides secure storage and cryptographic functionality including:
- **Secure Storage**: Platform-specific encrypted storage for sensitive data
- **Key-Value Storage**: Encrypted key-value pairs with automatic encryption/decryption
- **Cross-Platform**: Unified API across Windows, Linux, and macOS
- **Memory Protection**: Secure memory handling for sensitive data

## 📁 Examples

### ✅ **Basic Test**
**File**: `basic_test.cpp`  
**Status**: Fully functional ✅

A simple test that verifies the secret module can be loaded and basic functionality works.

#### **Features Demonstrated**
- Module initialization and loading
- Basic API availability verification
- Simple success/failure testing

### 🔧 **Secure Storage Example**
**File**: `secure_storage_example.cpp`  
**Status**: Has runtime issues (dependency/platform specific)

Comprehensive demonstration of secure storage capabilities.

#### **Features Demonstrated**
- Encrypted key-value storage
- Platform-specific encryption backends
- Secure data persistence
- Key management and rotation
- Error handling and recovery

### 🔧 **Simple Secret Example**
**File**: `simple_secret_example.cpp`  
**Status**: Has runtime issues (dependency/platform specific)

Simplified demonstration focusing on basic secure storage operations.

#### **Features Demonstrated**
- Basic secret storage and retrieval
- Simple encryption/decryption
- Minimal API usage patterns

## 🛠️ Building and Running

### Build the Examples
```bash
# Configure CMake with examples enabled
cmake -B build -S . -DATOM_EXAMPLE_BUILD_ALL=ON

# Build the secret examples
cmake --build build --target secret_basic_test
cmake --build build --target secret_secure_storage_example
cmake --build build --target secret_simple_secret_example
```

### Run the Examples
```bash
# Run the working basic test
./build/example/secret/secret_basic_test.exe

# Note: Other examples may have runtime issues
# ./build/example/secret/secret_secure_storage_example.exe
# ./build/example/secret/secret_simple_secret_example.exe
```

## 🎯 Key Features (Intended)

### **1. Secure Storage Interface**
```cpp
// Store encrypted data
SecureStorage storage;
storage.store("api_key", "secret_api_key_value");

// Retrieve and decrypt data
auto value = storage.retrieve("api_key");
if (value) {
    std::cout << "Retrieved: " << *value << std::endl;
}
```

### **2. Platform-Specific Encryption**
```cpp
// Automatically uses platform-appropriate encryption:
// - Windows: DPAPI (Data Protection API)
// - macOS: Keychain Services
// - Linux: libsecret or encrypted files
```

### **3. Key-Value Operations**
```cpp
// Store multiple secrets
storage.store("database_password", "super_secret_db_pass");
storage.store("oauth_token", "bearer_token_12345");

// List all stored keys
auto keys = storage.listKeys();
for (const auto& key : keys) {
    std::cout << "Stored key: " << key << std::endl;
}
```

### **4. Secure Memory Handling**
```cpp
// Automatic secure memory cleanup
SecureString password = getPasswordFromUser();
// Memory is automatically zeroed when SecureString is destroyed
```

## 📊 Security Features

### **Encryption Methods**
- **Windows**: DPAPI with user-specific encryption
- **macOS**: Keychain Services with system integration
- **Linux**: libsecret with GNOME Keyring or KDE Wallet
- **Fallback**: AES encryption with platform-specific key derivation

### **Key Management**
- Automatic key generation and rotation
- Platform-specific key storage
- Hardware security module integration (where available)
- Secure key derivation functions

### **Memory Protection**
- Secure memory allocation for sensitive data
- Automatic memory zeroing on deallocation
- Protection against memory dumps
- Stack protection for temporary secrets

## 🔧 Configuration Options

### **Storage Backends**
```cpp
// Configure storage backend
SecureStorageConfig config;
config.backend = SecureStorageBackend::PLATFORM_DEFAULT;
config.encryption = EncryptionMethod::AES_256_GCM;
config.keyDerivation = KeyDerivationMethod::PBKDF2;

SecureStorage storage(config);
```

### **Security Levels**
```cpp
// Different security levels available
enum class SecurityLevel {
    BASIC,      // Basic encryption, suitable for non-critical data
    STANDARD,   // Standard encryption, good for most applications
    HIGH,       // High security, suitable for sensitive applications
    MAXIMUM     // Maximum security, performance impact acceptable
};
```

## 🚨 Current Status and Known Issues

### **Working Components**
- ✅ **Module Loading**: Basic module initialization works
- ✅ **API Structure**: Core API is properly defined
- ✅ **Build System**: Examples build successfully

### **Known Issues**
- ❌ **Runtime Dependencies**: Missing platform-specific dependencies
- ❌ **Initialization**: Module initialization may fail on some platforms
- ❌ **Backend Selection**: Automatic backend selection needs work

### **Platform-Specific Issues**

#### **Windows**
- DPAPI integration may require additional Windows SDK components
- User context requirements for encryption/decryption

#### **Linux**
- libsecret dependency may not be available
- D-Bus requirements for keyring integration
- Fallback encryption implementation needed

#### **macOS**
- Keychain Services integration requires proper entitlements
- Code signing requirements for keychain access

## 🔍 Troubleshooting

### **Common Issues**

1. **Module Loading Failures**
   ```
   Error: Failed to initialize secure storage backend
   ```
   - **Solution**: Check platform-specific dependencies
   - **Workaround**: Use basic test to verify module loading

2. **Permission Denied**
   ```
   Error: Access denied to secure storage
   ```
   - **Solution**: Run with appropriate user permissions
   - **Check**: User keyring/keychain access rights

3. **Missing Dependencies**
   ```
   Error: Backend not available
   ```
   - **Linux**: Install libsecret-1-dev
   - **Windows**: Ensure DPAPI is available
   - **macOS**: Check Keychain Services access

### **Debugging Steps**

1. **Verify Module Loading**
   ```bash
   ./build/example/secret/secret_basic_test.exe
   ```

2. **Check Dependencies**
   ```bash
   # Linux
   ldd ./build/example/secret/secret_secure_storage_example.exe
   
   # Check for libsecret
   pkg-config --exists libsecret-1 && echo "libsecret available"
   ```

3. **Platform-Specific Checks**
   ```bash
   # Windows: Check DPAPI availability
   # macOS: Check Keychain access
   # Linux: Check D-Bus and keyring services
   ```

## 📚 Security Best Practices

### **Data Handling**
- Never store secrets in plain text
- Use secure memory for temporary secret storage
- Implement proper key rotation policies
- Audit secret access and usage

### **Application Integration**
- Initialize secure storage early in application lifecycle
- Handle encryption/decryption errors gracefully
- Implement fallback mechanisms for unavailable backends
- Use appropriate security levels for different data types

### **Development Guidelines**
- Test on all target platforms
- Verify backend availability before use
- Implement comprehensive error handling
- Document security assumptions and requirements

## 🎯 Future Improvements

### **Planned Features**
- Hardware security module (HSM) integration
- Multi-factor authentication support
- Secret sharing and distribution
- Audit logging and compliance features

### **Platform Enhancements**
- Better fallback mechanisms
- Improved error reporting
- Enhanced platform detection
- Simplified configuration

---

**Note**: While the secret module examples currently have runtime issues, the basic test demonstrates that the module structure is sound. The comprehensive examples showcase the intended functionality and serve as a foundation for future development and debugging.
