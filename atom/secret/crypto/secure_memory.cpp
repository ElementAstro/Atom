#include "secure_memory.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if __has_include(<openssl/rand.h>)
#include <openssl/rand.h>
#define HAS_OPENSSL 1
#endif

namespace atom::secret {

void SecureMemory::secureClear(void* ptr, size_t size) noexcept {
    if (!ptr || size == 0) {
        return;
    }

#ifdef HAS_OPENSSL
    // First pass: overwrite with random data
    if (RAND_bytes(static_cast<unsigned char*>(ptr), static_cast<int>(size)) !=
        1) {
        // Fallback to deterministic pattern if random fails
        std::memset(ptr, 0xAA, size);
        std::memset(ptr, 0x55, size);
    }
#else
    // Fallback pattern without OpenSSL
    std::memset(ptr, 0xAA, size);
    std::memset(ptr, 0x55, size);
#endif

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
    (void)ptr;
    (void)size;
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
    (void)ptr;
    (void)size;
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

    if (ptr) {
        // Try to lock memory, but continue even if it fails
        lockMemory(ptr, size);
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

bool SecureMemory::isMemoryLockingAvailable() noexcept {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}

size_t SecureMemory::getPageSize() noexcept {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#elif defined(__linux__) || defined(__APPLE__)
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
    return 4096;  // Default assumption
#endif
}

size_t SecureMemory::alignToPage(size_t size) noexcept {
    size_t pageSize = getPageSize();
    return (size + pageSize - 1) & ~(pageSize - 1);
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

template <typename T>
void SecureBuffer<T>::fill(const T& value) noexcept {
    if (data_) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = value;
        }
    }
}

template <typename T>
void SecureBuffer<T>::zero() noexcept {
    if (data_) {
        std::memset(data_, 0, size_ * sizeof(T));
    }
}

// Explicit template instantiations
template class SecureBuffer<uint8_t>;
template class SecureBuffer<char>;

}  // namespace atom::secret
