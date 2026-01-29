#ifndef ATOM_MEMORY_UTILS_HPP
#define ATOM_MEMORY_UTILS_HPP

#include <immintrin.h>  // For memory prefetching
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>
#include <vector>

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

namespace atom::memory {

/**
 * @brief Enhanced memory management configuration
 */
struct Config {
    static constexpr size_t DefaultAlignment = alignof(std::max_align_t);
    static constexpr size_t CacheLineSize = CACHE_LINE_SIZE;
    static constexpr size_t PageSize = 4096;                 // Common page size
    static constexpr size_t HugePageSize = 2 * 1024 * 1024;  // 2MB huge pages

    static constexpr bool EnableMemoryTracking =
#ifdef ATOM_MEMORY_TRACKING
        true;
#else
        false;
#endif

    static constexpr bool EnableMemoryPrefetching =
#ifdef ATOM_MEMORY_PREFETCH
        true;
#else
        true;  // Enable by default
#endif

    static constexpr bool EnableCacheOptimization =
#ifdef ATOM_CACHE_OPTIMIZATION
        true;
#else
        true;  // Enable by default
#endif

    static constexpr bool EnableNUMAOptimization =
#ifdef ATOM_NUMA_OPTIMIZATION
        true;
#else
        false;  // Disable by default (requires NUMA support)
#endif
};

template <typename T, typename... Args>
struct IsConstructible {
    static constexpr bool value = std::is_constructible_v<T, Args...>;
};

template <typename T, typename... Args>
using ConstructorArguments_t =
    std::enable_if_t<IsConstructible<T, Args...>::value, std::shared_ptr<T>>;

template <typename T, typename... Args>
using UniqueConstructorArguments_t =
    std::enable_if_t<IsConstructible<T, Args...>::value, std::unique_ptr<T>>;

/**
 * @brief Advanced memory alignment utilities
 */
namespace alignment {

/**
 * @brief Check if a pointer is aligned to the specified boundary
 */
template <size_t Alignment>
constexpr bool isAligned(const void* ptr) noexcept {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be a power of 2");
    return (reinterpret_cast<uintptr_t>(ptr) & (Alignment - 1)) == 0;
}

/**
 * @brief Align a value up to the next boundary
 */
template <size_t Alignment>
constexpr size_t alignUp(size_t value) noexcept {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be a power of 2");
    return (value + Alignment - 1) & ~(Alignment - 1);
}

/**
 * @brief Align a value down to the previous boundary
 */
template <size_t Alignment>
constexpr size_t alignDown(size_t value) noexcept {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be a power of 2");
    return value & ~(Alignment - 1);
}

/**
 * @brief Calculate padding needed for alignment
 */
template <size_t Alignment>
constexpr size_t alignmentPadding(const void* ptr) noexcept {
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be a power of 2");
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return (Alignment - (addr & (Alignment - 1))) & (Alignment - 1);
}

/**
 * @brief Aligned memory allocator with custom alignment
 */
template <size_t Alignment>
class AlignedAllocator {
public:
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "Alignment must be a power of 2");
    static_assert(Alignment >= sizeof(void*),
                  "Alignment must be at least pointer size");

    static void* allocate(size_t size) {
        if (size == 0)
            return nullptr;

        size_t total_size = size + Alignment + sizeof(void*);
        void* raw_ptr = std::malloc(total_size);
        if (!raw_ptr)
            return nullptr;

        // Calculate aligned address
        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
        uintptr_t aligned_addr = alignUp<Alignment>(raw_addr + sizeof(void*));

        // Store original pointer before aligned memory
        void** stored_ptr =
            reinterpret_cast<void**>(aligned_addr - sizeof(void*));
        *stored_ptr = raw_ptr;

        return reinterpret_cast<void*>(aligned_addr);
    }

    static void deallocate(void* ptr) noexcept {
        if (!ptr)
            return;

        // Retrieve original pointer
        void** stored_ptr =
            reinterpret_cast<void**>(static_cast<char*>(ptr) - sizeof(void*));
        std::free(*stored_ptr);
    }
};

/**
 * @brief Cache-line aligned allocator
 */
using CacheAlignedAllocator = AlignedAllocator<Config::CacheLineSize>;

/**
 * @brief Page-aligned allocator
 */
using PageAlignedAllocator = AlignedAllocator<Config::PageSize>;

}  // namespace alignment

/**
 * @brief Advanced smart pointer utilities and helpers
 */
namespace smart_ptr {

/**
 * @brief Observer pointer (non-owning smart pointer)
 */
template <typename T>
class ObserverPtr {
private:
    T* ptr_;

public:
    ObserverPtr() noexcept : ptr_(nullptr) {}
    explicit ObserverPtr(T* p) noexcept : ptr_(p) {}

    template <typename U>
    ObserverPtr(const std::unique_ptr<U>& p) noexcept : ptr_(p.get()) {}

    template <typename U>
    ObserverPtr(const std::shared_ptr<U>& p) noexcept : ptr_(p.get()) {}

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    void reset(T* p = nullptr) noexcept { ptr_ = p; }
    T* release() noexcept {
        T* result = ptr_;
        ptr_ = nullptr;
        return result;
    }
};

/**
 * @brief Weak reference implementation with enhanced features
 */
template <typename T>
class WeakRef {
private:
    std::weak_ptr<T> weak_ptr_;

public:
    WeakRef() = default;

    template <typename U>
    WeakRef(const std::shared_ptr<U>& shared) : weak_ptr_(shared) {}

    std::shared_ptr<T> lock() const noexcept { return weak_ptr_.lock(); }

    bool expired() const noexcept { return weak_ptr_.expired(); }

    void reset() noexcept { weak_ptr_.reset(); }

    size_t use_count() const noexcept { return weak_ptr_.use_count(); }

    // Enhanced functionality
    template <typename F>
    auto withLocked(F&& func) const -> decltype(func(*lock())) {
        if (auto locked = lock()) {
            return func(*locked);
        }
        throw std::runtime_error("WeakRef expired");
    }

    template <typename F>
    bool tryWithLocked(F&& func) const noexcept {
        if (auto locked = lock()) {
            try {
                func(*locked);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};

/**
 * @brief Scoped resource manager with custom deleter
 */
template <typename T, typename Deleter = std::default_delete<T>>
class ScopedResource {
private:
    T* resource_;
    Deleter deleter_;
    bool released_;

public:
    explicit ScopedResource(T* resource, Deleter deleter = Deleter{})
        : resource_(resource), deleter_(std::move(deleter)), released_(false) {}

    ~ScopedResource() {
        if (!released_ && resource_) {
            deleter_(resource_);
        }
    }

    // Non-copyable
    ScopedResource(const ScopedResource&) = delete;
    ScopedResource& operator=(const ScopedResource&) = delete;

    // Movable
    ScopedResource(ScopedResource&& other) noexcept
        : resource_(other.resource_),
          deleter_(std::move(other.deleter_)),
          released_(other.released_) {
        other.released_ = true;
    }

    ScopedResource& operator=(ScopedResource&& other) noexcept {
        if (this != &other) {
            if (!released_ && resource_) {
                deleter_(resource_);
            }
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            released_ = other.released_;
            other.released_ = true;
        }
        return *this;
    }

    T* get() const noexcept { return resource_; }
    T& operator*() const noexcept { return *resource_; }
    T* operator->() const noexcept { return resource_; }
    explicit operator bool() const noexcept {
        return resource_ != nullptr && !released_;
    }

    T* release() noexcept {
        released_ = true;
        return resource_;
    }

    void reset(T* new_resource = nullptr) {
        if (!released_ && resource_) {
            deleter_(resource_);
        }
        resource_ = new_resource;
        released_ = false;
    }
};

}  // namespace smart_ptr

/**
 * @brief Creates a std::shared_ptr object and validates constructor arguments
 * @return shared_ptr to type T
 */
template <typename T, typename... Args>
auto makeShared(Args&&... args) -> ConstructorArguments_t<T, Args...> {
    static_assert(IsConstructible<T, Args...>::value,
                  "Arguments do not match any constructor of the type T");
    return std::make_shared<T>(std::forward<Args>(args)...);
}

/**
 * @brief Creates a std::unique_ptr object and validates constructor arguments
 * @return unique_ptr to type T
 */
template <typename T, typename... Args>
auto makeUnique(Args&&... args) -> UniqueConstructorArguments_t<T, Args...> {
    static_assert(IsConstructible<T, Args...>::value,
                  "Arguments do not match any constructor of the type T");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/**
 * @brief Creates a shared_ptr with custom deleter
 */
template <typename T, typename Deleter, typename... Args>
auto makeSharedWithDeleter(Deleter&& deleter, Args&&... args)
    -> std::enable_if_t<IsConstructible<T, Args...>::value,
                        std::shared_ptr<T>> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::shared_ptr<T>(new T(std::forward<Args>(args)...),
                                  std::forward<Deleter>(deleter));
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates a unique_ptr with custom deleter
 */
template <typename T, typename Deleter, typename... Args>
auto makeUniqueWithDeleter(Deleter&& deleter, Args&&... args)
    -> std::enable_if_t<IsConstructible<T, Args...>::value,
                        std::unique_ptr<T, std::decay_t<Deleter>>> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::unique_ptr<T, std::decay_t<Deleter>>(
            new T(std::forward<Args>(args)...), std::forward<Deleter>(deleter));
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

/**
 * @brief Creates an array type shared_ptr
 */
template <typename T>
std::shared_ptr<T[]> makeSharedArray(size_t size) {
    return std::shared_ptr<T[]>(new T[size]());
}

/**
 * @brief Creates an array type unique_ptr
 */
template <typename T>
std::unique_ptr<T[]> makeUniqueArray(size_t size) {
    return std::make_unique<T[]>(size);
}

/**
 * @brief Thread-safe singleton template
 */
template <typename T>
class ThreadSafeSingleton {
public:
    static std::shared_ptr<T> getInstance() {
        std::shared_ptr<T> instance = instance_weak_.lock();
        if (!instance) {
            std::lock_guard<std::mutex> lock(mutex_);
            instance = instance_weak_.lock();
            if (!instance) {
                instance = std::make_shared<T>();
                instance_weak_ = instance;
            }
        }
        return instance;
    }

private:
    static std::weak_ptr<T> instance_weak_;
    static std::mutex mutex_;
};

template <typename T>
std::weak_ptr<T> ThreadSafeSingleton<T>::instance_weak_;

template <typename T>
std::mutex ThreadSafeSingleton<T>::mutex_;

/**
 * @brief Check weak reference and lock it
 * @return If weak reference is valid, returns the locked shared_ptr, otherwise
 * returns nullptr
 */
template <typename T>
std::shared_ptr<T> lockWeak(const std::weak_ptr<T>& weak) {
    return weak.lock();
}

/**
 * @brief Check weak reference and lock it, create new object if invalid
 */
template <typename T, typename... Args>
std::shared_ptr<T> lockWeakOrCreate(std::weak_ptr<T>& weak, Args&&... args) {
    auto ptr = weak.lock();
    if (!ptr) {
        ptr = std::make_shared<T>(std::forward<Args>(args)...);
        weak = ptr;
    }
    return ptr;
}

/**
 * @brief Memory prefetching and cache optimization utilities
 */
namespace cache {

/**
 * @brief Prefetch memory for reading
 */
inline void prefetchRead(const void* addr) noexcept {
    if constexpr (Config::EnableMemoryPrefetching) {
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
    }
}

/**
 * @brief Prefetch memory for writing
 */
inline void prefetchWrite(const void* addr) noexcept {
    if constexpr (Config::EnableMemoryPrefetching) {
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
    }
}

/**
 * @brief Prefetch multiple cache lines
 */
inline void prefetchRange(const void* start, size_t size) noexcept {
    if constexpr (Config::EnableMemoryPrefetching) {
        const char* addr = static_cast<const char*>(start);
        const char* end = addr + size;

        for (const char* ptr = addr; ptr < end; ptr += Config::CacheLineSize) {
            _mm_prefetch(ptr, _MM_HINT_T0);
        }
    }
}

/**
 * @brief Cache-friendly memory copy
 */
inline void cacheFriendlyMemcpy(void* dest, const void* src,
                                size_t size) noexcept {
    if constexpr (Config::EnableCacheOptimization) {
        // Prefetch source data
        prefetchRange(src, size);

        // Use standard memcpy (optimized by compiler/runtime)
        std::memcpy(dest, src, size);

        // Flush destination from cache if it's a large copy
        if (size > Config::CacheLineSize * 4) {
            const char* dest_addr = static_cast<const char*>(dest);
            for (size_t offset = 0; offset < size;
                 offset += Config::CacheLineSize) {
                _mm_clflush(dest_addr + offset);
            }
        }
    } else {
        std::memcpy(dest, src, size);
    }
}

/**
 * @brief Cache-aligned memory allocator
 */
template <typename T>
class CacheAlignedAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = CacheAlignedAllocator<U>;
    };

    CacheAlignedAllocator() = default;

    template <typename U>
    CacheAlignedAllocator(const CacheAlignedAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n == 0)
            return nullptr;

        size_type size = n * sizeof(T);
        void* ptr = alignment::CacheAlignedAllocator::allocate(size);

        if (!ptr) {
            throw std::bad_alloc();
        }

        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) noexcept {
        alignment::CacheAlignedAllocator::deallocate(p);
    }

    template <typename U>
    bool operator==(const CacheAlignedAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const CacheAlignedAllocator<U>&) const noexcept {
        return false;
    }
};

}  // namespace cache

/**
 * @brief RAII helpers and resource management utilities
 */
namespace raii {

/**
 * @brief Scope guard for automatic cleanup
 */
template <typename F>
class ScopeGuard {
private:
    F cleanup_;
    bool dismissed_;

public:
    explicit ScopeGuard(F&& cleanup)
        : cleanup_(std::forward<F>(cleanup)), dismissed_(false) {}

    ~ScopeGuard() {
        if (!dismissed_) {
            cleanup_();
        }
    }

    void dismiss() noexcept { dismissed_ = true; }

    // Non-copyable, non-movable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;
};

/**
 * @brief Create a scope guard
 */
template <typename F>
auto makeScopeGuard(F&& cleanup) {
    return ScopeGuard<F>(std::forward<F>(cleanup));
}

/**
 * @brief RAII wrapper for C-style resources
 */
template <typename T, typename Deleter>
class ResourceWrapper {
private:
    T resource_;
    Deleter deleter_;
    bool valid_;

public:
    ResourceWrapper(T resource, Deleter deleter)
        : resource_(resource), deleter_(deleter), valid_(true) {}

    ~ResourceWrapper() {
        if (valid_) {
            deleter_(resource_);
        }
    }

    // Non-copyable
    ResourceWrapper(const ResourceWrapper&) = delete;
    ResourceWrapper& operator=(const ResourceWrapper&) = delete;

    // Movable
    ResourceWrapper(ResourceWrapper&& other) noexcept
        : resource_(other.resource_),
          deleter_(std::move(other.deleter_)),
          valid_(other.valid_) {
        other.valid_ = false;
    }

    ResourceWrapper& operator=(ResourceWrapper&& other) noexcept {
        if (this != &other) {
            if (valid_) {
                deleter_(resource_);
            }
            resource_ = other.resource_;
            deleter_ = std::move(other.deleter_);
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }

    T get() const noexcept { return resource_; }
    T operator*() const noexcept { return resource_; }
    explicit operator bool() const noexcept { return valid_; }

    T release() noexcept {
        valid_ = false;
        return resource_;
    }
};

/**
 * @brief Create a resource wrapper
 */
template <typename T, typename Deleter>
auto makeResourceWrapper(T resource, Deleter deleter) {
    return ResourceWrapper<T, Deleter>(resource, deleter);
}

}  // namespace raii

}  // namespace atom::memory

#endif  // ATOM_MEMORY_UTILS_HPP
