#ifndef ATOM_SECRET_CRYPTO_SECURE_MEMORY_HPP
#define ATOM_SECRET_CRYPTO_SECURE_MEMORY_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace atom::secret {

/**
 * @brief Secure memory management utilities for sensitive data.
 *
 * This class provides functions for securely handling sensitive data in memory,
 * including secure clearing, memory locking to prevent swapping, and secure
 * allocation.
 */
class SecureMemory {
public:
    /**
     * @brief Securely clears memory by overwriting with random data then zeros.
     *
     * This function performs multiple passes to ensure data is thoroughly
     * overwritten, making recovery difficult even with advanced techniques.
     *
     * @param ptr Pointer to memory to clear.
     * @param size Size of memory to clear in bytes.
     */
    static void secureClear(void* ptr, size_t size) noexcept;

    /**
     * @brief Securely clears a string's contents.
     *
     * Overwrites the string's internal buffer before clearing and shrinking.
     *
     * @param str String to clear.
     */
    static void secureClear(std::string& str) noexcept;

    /**
     * @brief Securely clears a vector's contents.
     *
     * Overwrites the vector's internal buffer before clearing and shrinking.
     *
     * @tparam T Type of vector elements.
     * @param vec Vector to clear.
     */
    template <typename T>
    static void secureClear(std::vector<T>& vec) noexcept;

    /**
     * @brief Locks memory pages to prevent swapping to disk.
     *
     * Locked memory will not be written to swap space, which is important
     * for sensitive data like encryption keys.
     *
     * @param ptr Pointer to memory to lock.
     * @param size Size of memory to lock in bytes.
     * @return True if successful, false otherwise.
     */
    static bool lockMemory(void* ptr, size_t size) noexcept;

    /**
     * @brief Unlocks previously locked memory pages.
     *
     * @param ptr Pointer to memory to unlock.
     * @param size Size of memory to unlock in bytes.
     * @return True if successful, false otherwise.
     */
    static bool unlockMemory(void* ptr, size_t size) noexcept;

    /**
     * @brief Allocates secure memory that won't be swapped to disk.
     *
     * The allocated memory is aligned and locked to prevent swapping.
     *
     * @param size Size of memory to allocate in bytes.
     * @return Pointer to allocated memory or nullptr on failure.
     */
    static void* allocateSecure(size_t size) noexcept;

    /**
     * @brief Frees secure memory allocated with allocateSecure.
     *
     * The memory is securely cleared before being freed.
     *
     * @param ptr Pointer to memory to free.
     * @param size Size of memory to free in bytes.
     */
    static void freeSecure(void* ptr, size_t size) noexcept;

    /**
     * @brief Checks if memory locking is available on this platform.
     * @return True if memory locking is supported.
     */
    static bool isMemoryLockingAvailable() noexcept;

    /**
     * @brief Gets the system page size.
     * @return Page size in bytes.
     */
    static size_t getPageSize() noexcept;

    /**
     * @brief Aligns a size to the system page boundary.
     * @param size Size to align.
     * @return Aligned size.
     */
    static size_t alignToPage(size_t size) noexcept;
};

/**
 * @brief RAII wrapper for secure memory allocation.
 *
 * This template class provides automatic secure memory management with
 * RAII semantics. Memory is automatically locked on allocation and
 * securely cleared on destruction.
 *
 * @tparam T Type of elements to store.
 */
template <typename T>
class SecureBuffer {
private:
    T* data_;
    size_t size_;

public:
    /**
     * @brief Constructs a secure buffer of the specified size.
     * @param size Number of elements to allocate.
     */
    explicit SecureBuffer(size_t size);

    /**
     * @brief Destructor that securely clears and frees memory.
     */
    ~SecureBuffer();

    // Disable copy construction and assignment
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Enable move construction and assignment
    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;

    /**
     * @brief Gets pointer to the buffer data.
     * @return Pointer to buffer data.
     */
    T* data() noexcept { return data_; }

    /**
     * @brief Gets const pointer to the buffer data.
     * @return Const pointer to buffer data.
     */
    const T* data() const noexcept { return data_; }

    /**
     * @brief Gets the buffer size.
     * @return Number of elements in the buffer.
     */
    size_t size() const noexcept { return size_; }

    /**
     * @brief Gets the buffer size in bytes.
     * @return Size in bytes.
     */
    size_t sizeBytes() const noexcept { return size_ * sizeof(T); }

    /**
     * @brief Array access operator.
     * @param index Index of element to access.
     * @return Reference to element at index.
     */
    T& operator[](size_t index) noexcept { return data_[index]; }

    /**
     * @brief Const array access operator.
     * @param index Index of element to access.
     * @return Const reference to element at index.
     */
    const T& operator[](size_t index) const noexcept { return data_[index]; }

    /**
     * @brief Checks if the buffer is valid.
     * @return True if buffer is allocated, false otherwise.
     */
    bool isValid() const noexcept { return data_ != nullptr; }

    /**
     * @brief Implicit conversion to bool.
     * @return True if buffer is valid.
     */
    explicit operator bool() const noexcept { return isValid(); }

    /**
     * @brief Gets iterator to beginning.
     * @return Pointer to first element.
     */
    T* begin() noexcept { return data_; }

    /**
     * @brief Gets const iterator to beginning.
     * @return Const pointer to first element.
     */
    const T* begin() const noexcept { return data_; }

    /**
     * @brief Gets iterator to end.
     * @return Pointer past last element.
     */
    T* end() noexcept { return data_ + size_; }

    /**
     * @brief Gets const iterator to end.
     * @return Const pointer past last element.
     */
    const T* end() const noexcept { return data_ + size_; }

    /**
     * @brief Fills the buffer with a value.
     * @param value Value to fill with.
     */
    void fill(const T& value) noexcept;

    /**
     * @brief Zeros the buffer contents.
     */
    void zero() noexcept;
};

/**
 * @brief Secure string class that automatically clears on destruction.
 */
class SecureString {
private:
    std::string data_;

public:
    SecureString() = default;
    explicit SecureString(const std::string& str) : data_(str) {}
    explicit SecureString(std::string&& str) : data_(std::move(str)) {}
    explicit SecureString(const char* str) : data_(str) {}
    SecureString(const char* str, size_t len) : data_(str, len) {}

    ~SecureString() { SecureMemory::secureClear(data_); }

    // Disable copy
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;

    // Enable move
    SecureString(SecureString&& other) noexcept
        : data_(std::move(other.data_)) {}
    SecureString& operator=(SecureString&& other) noexcept {
        if (this != &other) {
            SecureMemory::secureClear(data_);
            data_ = std::move(other.data_);
        }
        return *this;
    }

    const std::string& str() const noexcept { return data_; }
    const char* c_str() const noexcept { return data_.c_str(); }
    size_t size() const noexcept { return data_.size(); }
    size_t length() const noexcept { return data_.length(); }
    bool empty() const noexcept { return data_.empty(); }

    void clear() { SecureMemory::secureClear(data_); }

    void append(const std::string& str) { data_.append(str); }
    void append(const char* str) { data_.append(str); }
    void append(char c) { data_.push_back(c); }

    char operator[](size_t index) const { return data_[index]; }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CRYPTO_SECURE_MEMORY_HPP
