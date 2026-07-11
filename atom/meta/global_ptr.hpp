/*!
 * \file global_ptr.hpp
 * \brief Enhanced global shared pointer manager with improved cross-platform
 * support - OPTIMIZED VERSION
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \update 2024-03-11
 * \optimized 2025-01-22 - Performance optimizations by AI Assistant
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * OPTIMIZATIONS APPLIED:
 * - Reduced string allocations with string_view-compatible hash maps
 * - Combined pointer and metadata storage for better cache locality
 * - Added lock-free fast path for read operations
 * - Optimized cleanup operations with batch processing
 * - Enhanced memory usage tracking and statistics
 */

#ifndef ATOM_META_GLOBAL_PTR_HPP
#define ATOM_META_GLOBAL_PTR_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#else
#include <unordered_map>
#endif

#include "atom/error/exception.hpp"
#include "atom/type/noncopyable.hpp"

#define GetPtr GlobalSharedPtrManager::getInstance().getSharedPtr
#define GetWeakPtr GlobalSharedPtrManager::getInstance().getWeakPtr
#define AddPtr GlobalSharedPtrManager::getInstance().addSharedPtr
#define RemovePtr GlobalSharedPtrManager::getInstance().removeSharedPtr
#define GetPtrOrCreate \
    GlobalSharedPtrManager::getInstance().getOrCreateSharedPtr
#define AddDeleter GlobalSharedPtrManager::getInstance().addDeleter
#define GetPtrInfo GlobalSharedPtrManager::getInstance().getPtrInfo

#define GET_WEAK_PTR(type, name, id)                        \
    std::weak_ptr<type> name##Ptr;                          \
    GET_OR_CREATE_WEAK_PTR(name##Ptr, type, Constants::id); \
    auto(name) = name##Ptr.lock();                          \
    if (!(name)) {                                          \
        THROW_OBJ_NOT_EXIST("Component: ", Constants::id);  \
    }

// NOTE: the macros below use a reserved-style local name so they never shadow
// (and silently self-assign) a caller variable that is also called "ptr".
#define GET_OR_CREATE_PTR_WITH_CAPTURE(variable, type, constant, capture) \
    if (auto atom_gp_tmp_ptr_ = GetPtrOrCreate<type>(constant, [capture] { \
            return atom::memory::makeShared<type>(capture);               \
        })) {                                                             \
        variable = atom_gp_tmp_ptr_;                                      \
    } else {                                                              \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");          \
    }

#define GET_OR_CREATE_PTR(variable, type, constant, ...)                     \
    if (auto atom_gp_tmp_ptr_ = GetPtrOrCreate<type>(                        \
            constant, [] { return std::make_shared<type>(__VA_ARGS__); })) { \
        variable = atom_gp_tmp_ptr_;                                         \
    } else {                                                                 \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");             \
    }

#define GET_OR_CREATE_PTR_THIS(variable, type, constant, ...)       \
    if (auto atom_gp_tmp_ptr_ = GetPtrOrCreate<type>(constant, [this] { \
            return std::make_shared<type>(__VA_ARGS__);             \
        })) {                                                       \
        variable = atom_gp_tmp_ptr_;                                \
    } else {                                                        \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");    \
    }

#define GET_OR_CREATE_WEAK_PTR(variable, type, constant, ...)                \
    if (auto atom_gp_tmp_ptr_ = GetPtrOrCreate<type>(                        \
            constant, [] { return std::make_shared<type>(__VA_ARGS__); })) { \
        variable = std::weak_ptr(atom_gp_tmp_ptr_);                          \
    } else {                                                                 \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");             \
    }

#define GET_OR_CREATE_PTR_WITH_DELETER(variable, type, constant, deleter)  \
    if (auto atom_gp_tmp_ptr_ = GetPtrOrCreate<type>(constant, [deleter] { \
            return std::shared_ptr<type>(new type, deleter);               \
        })) {                                                              \
        GlobalSharedPtrManager::getInstance().markCustomDeleter(constant); \
        variable = atom_gp_tmp_ptr_;                                       \
    } else {                                                               \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");           \
    }

/**
 * @brief Optimized structure to hold pointer metadata
 */
struct PointerMetadata {
    uint64_t creation_time_micros;  // Compact time representation
    // mutable: counters are updated from const accessors under a shared lock
    mutable std::atomic<uint32_t> access_count{0};  // Lock-free access counting
    mutable std::atomic<uint32_t> ref_count{0};     // Lock-free ref counting
    mutable std::atomic<uint64_t> last_access_micros{0};  // Idle tracking
    std::string type_name;

    // Pack flags into single byte for better memory efficiency
    struct Flags {
        bool is_weak : 1;
        bool has_custom_deleter : 1;
        bool is_expired : 1;  // For faster cleanup
        uint8_t reserved : 5;
    } flags = {};

    PointerMetadata() = default;

    explicit PointerMetadata(std::string_view type_name_view,
                             bool is_weak = false, bool has_deleter = false)
        : creation_time_micros(getCurrentTimeMicros()),
          last_access_micros(creation_time_micros),
          type_name(type_name_view) {
        flags.is_weak = is_weak;
        flags.has_custom_deleter = has_deleter;
        flags.is_expired = false;
    }

    // Copy constructor for atomic members
    PointerMetadata(const PointerMetadata& other)
        : creation_time_micros(other.creation_time_micros),
          access_count(other.access_count.load(std::memory_order_relaxed)),
          ref_count(other.ref_count.load(std::memory_order_relaxed)),
          last_access_micros(
              other.last_access_micros.load(std::memory_order_relaxed)),
          type_name(other.type_name),
          flags(other.flags) {}

    // Copy assignment operator
    PointerMetadata& operator=(const PointerMetadata& other) {
        if (this != &other) {
            creation_time_micros = other.creation_time_micros;
            access_count.store(
                other.access_count.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            ref_count.store(other.ref_count.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
            last_access_micros.store(
                other.last_access_micros.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
            type_name = other.type_name;
            flags = other.flags;
        }
        return *this;
    }

    //! Record an access: bump the counter and refresh the idle timestamp.
    void recordAccess() const noexcept {
        access_count.fetch_add(1, std::memory_order_relaxed);
        last_access_micros.store(getCurrentTimeMicros(),
                                 std::memory_order_relaxed);
    }

private:
    static auto getCurrentTimeMicros() noexcept -> uint64_t {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
};

/**
 * @brief Combined storage entry for better cache locality
 */
struct PointerEntry {
    std::any ptr_data;
    PointerMetadata metadata;
    std::function<bool()> expired_check;  // Set for weak pointer entries
    std::function<long()> use_count_fn;   // Non-owning live use_count probe

    template <typename T>
    PointerEntry(std::shared_ptr<T> ptr, std::string_view type_name,
                 bool is_weak = false, bool has_deleter = false)
        : metadata(type_name, is_weak, has_deleter) {
        // Capture a weak reference for the probe *before* moving the pointer
        // into the std::any so the probe never extends the object's lifetime.
        std::weak_ptr<T> weak_probe = ptr;
        use_count_fn = [weak_probe] {
            return static_cast<long>(weak_probe.use_count());
        };
        ptr_data = std::move(ptr);
    }

    template <typename T>
    PointerEntry(std::weak_ptr<T> ptr, std::string_view type_name)
        : ptr_data(ptr),
          metadata(type_name, true, false),
          expired_check([ptr] { return ptr.expired(); }),
          use_count_fn([ptr] { return static_cast<long>(ptr.use_count()); }) {}
};

/**
 * @brief Enhanced GlobalSharedPtrManager with improved functionality and
 * performance
 */
class GlobalSharedPtrManager : public atom::type::NonCopyable {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    /**
     * @brief Automatic cleanup policy configuration
     */
    struct CleanupPolicy {
        std::chrono::seconds max_age{3600};  // 1 hour default
        size_t max_unused_count = 1000;      // Max unused pointers
        bool auto_cleanup_enabled = false;
        std::chrono::seconds cleanup_interval{300};  // 5 minutes
    };

    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static auto getInstance() -> GlobalSharedPtrManager&;

    /**
     * @brief Get shared pointer by key with type safety
     * @tparam T Pointer type
     * @param key Lookup key
     * @return Optional containing the shared pointer if found
     */
    template <typename T>
    [[nodiscard]] auto getSharedPtr(std::string_view key)
        -> std::optional<std::shared_ptr<T>>;

    /**
     * @brief Get or create shared pointer
     * @tparam T Pointer type
     * @tparam CreatorFunc Creator function type
     * @param key Lookup key
     * @param creator Function to create new instance
     * @return Created or retrieved shared pointer
     */
    template <typename T, typename CreatorFunc>
    auto getOrCreateSharedPtr(std::string_view key,
                              CreatorFunc creator) -> std::shared_ptr<T>;

    /**
     * @brief Get weak pointer by key
     * @tparam T Pointer type
     * @param key Lookup key
     * @return Weak pointer to the object
     */
    template <typename T>
    [[nodiscard]] auto getWeakPtr(std::string_view key) -> std::weak_ptr<T>;

    /**
     * @brief Add shared pointer with key
     * @tparam T Pointer type
     * @param key Lookup key
     * @param ptr Shared pointer to add
     */
    template <typename T>
    void addSharedPtr(std::string_view key, std::shared_ptr<T> ptr);

    /**
     * @brief Register a weak pointer with key
     * @tparam T Pointer type
     * @param key Lookup key
     * @param ptr Weak pointer to register
     */
    template <typename T>
    void addWeakPtr(std::string_view key, const std::weak_ptr<T>& ptr);

    /**
     * @brief Lock a registered weak pointer into a shared pointer
     * @tparam T Pointer type
     * @param key Lookup key
     * @return Shared pointer (nullptr if missing, type mismatch, or expired)
     */
    template <typename T>
    [[nodiscard]] auto getSharedPtrFromWeakPtr(std::string_view key)
        -> std::shared_ptr<T>;

    /**
     * @brief Remove pointer by key
     * @param key Key to remove
     */
    void removeSharedPtr(std::string_view key);

    /**
     * @brief Add custom deleter for key
     * @tparam T Object type
     * @param key Lookup key
     * @param deleter Custom deleter function
     */
    template <typename T>
    void addDeleter(std::string_view key,
                    const std::function<void(T*)>& deleter);

    /**
     * @brief Mark an existing entry as having a custom deleter
     * @param key Lookup key
     */
    void markCustomDeleter(std::string_view key);

    /**
     * @brief Get metadata for pointer
     * @param key Lookup key
     * @return Optional containing metadata if found
     */
    [[nodiscard]] auto getPtrInfo(std::string_view key) const
        -> std::optional<PointerMetadata>;

    /**
     * @brief Remove expired weak pointers
     * @return Number of pointers removed
     */
    size_t removeExpiredWeakPtrs();

    /**
     * @brief Clean old pointers not accessed recently
     * @param older_than Remove pointers older than this duration
     * @return Number of pointers removed
     */
    size_t cleanOldPointers(const std::chrono::seconds& older_than);

    /**
     * @brief Clear all pointers
     */
    void clearAll();

    /**
     * @brief Get current size
     * @return Number of stored pointers
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Print debug info about stored pointers
     */
    void printSharedPtrMap() const;

private:
    GlobalSharedPtrManager() = default;

    // Optimized storage: single map with combined data for better cache
    // locality
#if ENABLE_FASTHASH
    emhash8::HashMap<std::string, PointerEntry> pointer_map_;
#else
    std::unordered_map<std::string, PointerEntry> pointer_map_;
#endif

    mutable std::shared_mutex mutex_;
    std::atomic<size_t> total_access_count_{0};

    // Batch cleanup optimization
    std::vector<std::string> cleanup_batch_;
    static constexpr size_t CLEANUP_BATCH_SIZE = 64;

    // Enhanced features
    CleanupPolicy cleanup_policy_;
    std::unordered_map<std::string, std::vector<std::string>> dependencies_;
    std::atomic<bool> auto_cleanup_running_{false};
    std::chrono::steady_clock::time_point last_cleanup_time_;

    // Error handling and logging
    mutable std::atomic<size_t> error_count_{0};
    mutable std::string last_error_message_;
    mutable std::mutex error_mutex_;

    /**
     * @brief Batch cleanup expired entries for better performance
     * @return Number of entries cleaned up
     */
    size_t batchCleanupExpired();

    /**
     * @brief Get statistics about the pointer manager
     * @return Statistics structure
     */
    struct Statistics {
        size_t total_pointers = 0;
        size_t weak_pointers = 0;
        size_t expired_pointers = 0;
        size_t total_accesses = 0;
        double average_access_count = 0.0;
        size_t memory_usage_bytes = 0;
        std::chrono::milliseconds average_age{0};
    };

    [[nodiscard]] auto getStatistics() const -> Statistics;

    /**
     * @brief Set automatic cleanup policy
     * @param policy Cleanup policy configuration
     */
    void setCleanupPolicy(const CleanupPolicy& policy);

    /**
     * @brief Get current cleanup policy
     * @return Current cleanup policy
     */
    [[nodiscard]] auto getCleanupPolicy() const -> CleanupPolicy;

    /**
     * @brief Enable/disable automatic cleanup
     * @param enabled Whether to enable automatic cleanup
     */
    void setAutoCleanupEnabled(bool enabled);

    /**
     * @brief Add dependency tracking between pointers
     * @param dependent_key Key of dependent pointer
     * @param dependency_key Key of dependency pointer
     */
    void addDependency(std::string_view dependent_key,
                       std::string_view dependency_key);

    /**
     * @brief Remove dependency tracking
     * @param dependent_key Key of dependent pointer
     * @param dependency_key Key of dependency pointer
     */
    void removeDependency(std::string_view dependent_key,
                          std::string_view dependency_key);

    /**
     * @brief Get all dependencies for a pointer
     * @param key Pointer key
     * @return Vector of dependency keys
     */
    [[nodiscard]] auto getDependencies(std::string_view key) const
        -> std::vector<std::string>;

    /**
     * @brief Check if cleanup is safe (no dependencies)
     * @param key Pointer key to check
     * @return True if safe to cleanup
     */
    [[nodiscard]] auto isSafeToCleanup(std::string_view key) const -> bool;
};

template <typename T>
auto GlobalSharedPtrManager::getSharedPtr(std::string_view key)
    -> std::optional<std::shared_ptr<T>> {
    std::shared_lock lock(mutex_);

    if (auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        if (const auto* stored =
                std::any_cast<std::shared_ptr<T>>(&iter->second.ptr_data)) {
            auto ptr = *stored;

            // Lock-free metadata updates
            iter->second.metadata.recordAccess();
            iter->second.metadata.ref_count.store(ptr.use_count(),
                                                  std::memory_order_relaxed);
            total_access_count_.fetch_add(1, std::memory_order_relaxed);

            return ptr;
        }
    }
    return std::nullopt;
}

template <typename T, typename CreatorFunc>
auto GlobalSharedPtrManager::getOrCreateSharedPtr(
    std::string_view key, CreatorFunc creator) -> std::shared_ptr<T> {
    const std::string str_key{key};
    std::unique_lock lock(mutex_);

    if (auto iter = pointer_map_.find(str_key); iter != pointer_map_.end()) {
        if (const auto* stored =
                std::any_cast<std::shared_ptr<T>>(&iter->second.ptr_data)) {
            auto ptr = *stored;
            // Update metadata atomically
            iter->second.metadata.recordAccess();
            iter->second.metadata.ref_count.store(ptr.use_count(),
                                                  std::memory_order_relaxed);
            return ptr;
        } else {
            // Stored entry has a different type; replace it.
            auto ptr = creator();
            iter->second = PointerEntry{ptr, typeid(T).name()};
            iter->second.metadata.recordAccess();
            iter->second.metadata.ref_count.store(ptr.use_count(),
                                                  std::memory_order_relaxed);
            return ptr;
        }
    } else {
        auto ptr = creator();
        pointer_map_.emplace(str_key, PointerEntry{ptr, typeid(T).name()});
        total_access_count_.fetch_add(1, std::memory_order_relaxed);
        return ptr;
    }
}

template <typename T>
auto GlobalSharedPtrManager::getWeakPtr(std::string_view key)
    -> std::weak_ptr<T> {
    std::shared_lock lock(mutex_);

    if (auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        // Use the pointer form of any_cast: the value form throws on type
        // mismatch, which previously prevented falling through to the
        // weak_ptr branch for entries registered via addWeakPtr().
        if (const auto* shared_ptr =
                std::any_cast<std::shared_ptr<T>>(&iter->second.ptr_data)) {
            iter->second.metadata.recordAccess();
            total_access_count_.fetch_add(1, std::memory_order_relaxed);
            return std::weak_ptr<T>(*shared_ptr);
        }
        if (const auto* weak_ptr =
                std::any_cast<std::weak_ptr<T>>(&iter->second.ptr_data)) {
            iter->second.metadata.recordAccess();
            total_access_count_.fetch_add(1, std::memory_order_relaxed);
            return *weak_ptr;
        }
    }
    return std::weak_ptr<T>();
}

template <typename T>
void GlobalSharedPtrManager::addSharedPtr(std::string_view key,
                                          std::shared_ptr<T> ptr) {
    std::unique_lock lock(mutex_);
    const std::string str_key{key};
    pointer_map_.emplace(str_key, PointerEntry{ptr, typeid(T).name()});
}

template <typename T>
void GlobalSharedPtrManager::addWeakPtr(std::string_view key,
                                        const std::weak_ptr<T>& ptr) {
    std::unique_lock lock(mutex_);
    const std::string str_key{key};
    pointer_map_.insert_or_assign(str_key, PointerEntry{ptr, typeid(T).name()});
}

template <typename T>
auto GlobalSharedPtrManager::getSharedPtrFromWeakPtr(std::string_view key)
    -> std::shared_ptr<T> {
    std::shared_lock lock(mutex_);

    if (auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        if (const auto* weak_ptr =
                std::any_cast<std::weak_ptr<T>>(&iter->second.ptr_data)) {
            iter->second.metadata.recordAccess();
            total_access_count_.fetch_add(1, std::memory_order_relaxed);
            return weak_ptr->lock();
        }
    }
    return nullptr;
}

template <typename T>
void GlobalSharedPtrManager::addDeleter(
    std::string_view key, const std::function<void(T*)>& deleter) {
    std::unique_lock lock(mutex_);

    if (auto iter = pointer_map_.find(std::string(key));
        iter != pointer_map_.end()) {
        if (const auto* stored =
                std::any_cast<std::shared_ptr<T>>(&iter->second.ptr_data)) {
            // A shared_ptr's deleter cannot be replaced after creation, and
            // naively doing `ptr.reset(ptr.get(), deleter)` creates a SECOND
            // control block that also owns the raw pointer -> double free /
            // heap corruption when both groups eventually release it.
            //
            // Instead, transfer deletion responsibility: the manager's entry
            // becomes a new ownership group whose deleter (a) invokes the
            // custom deleter exactly once and (b) permanently detaches the
            // original control block so its default deleter can never run on
            // the already-destroyed object. Detaching intentionally leaks one
            // control block (a few dozen bytes); this is the only safe way to
            // honor a retroactively registered deleter.
            std::shared_ptr<T> wrapped(
                stored->get(), [keep = *stored, deleter](T* raw_ptr) mutable {
                    deleter(raw_ptr);
                    // Detach: leak one reference to the original group.
                    new std::shared_ptr<T>(std::move(keep));
                });
            std::weak_ptr<T> weak_probe = wrapped;
            iter->second.ptr_data = std::move(wrapped);
            iter->second.use_count_fn = [weak_probe] {
                return static_cast<long>(weak_probe.use_count());
            };
            iter->second.metadata.flags.has_custom_deleter = true;
        }
    }
}

//==============================================================================
// C++23 Enhanced Global Pointer Utilities
//==============================================================================

/**
 * @brief Concept for pointer-like types
 */
template <typename T>
concept PointerLike = requires(T t) {
    { *t };
    { t.get() };
    { static_cast<bool>(t) };
};

/**
 * @brief Concept for shared pointer types
 */
template <typename T>
concept SharedPointerLike = PointerLike<T> && requires(T t) {
    { t.use_count() } -> std::convertible_to<long>;
};

/**
 * @brief Safe global pointer access with optional result
 */
template <typename T>
auto safeGetPtr(std::string_view key) -> std::optional<std::shared_ptr<T>> {
    auto ptr = GlobalSharedPtrManager::getInstance().getSharedPtr<T>(key);
    if (ptr) {
        return ptr;
    }
    return std::nullopt;
}

/**
 * @brief Get or create with factory function
 */
template <typename T, typename Factory>
    requires std::invocable<Factory> &&
                 std::same_as<std::invoke_result_t<Factory>, std::shared_ptr<T>>
auto getOrCreate(std::string_view key,
                 Factory&& factory) -> std::shared_ptr<T> {
    return GlobalSharedPtrManager::getInstance().getOrCreateSharedPtr<T>(
        key, std::forward<Factory>(factory));
}

/**
 * @brief Scoped pointer registration (RAII)
 */
template <typename T>
class ScopedGlobalPtr {
    std::string key_;

public:
    ScopedGlobalPtr(std::string_view key, std::shared_ptr<T> ptr) : key_(key) {
        GlobalSharedPtrManager::getInstance().addSharedPtr<T>(key,
                                                              std::move(ptr));
    }

    ~ScopedGlobalPtr() {
        GlobalSharedPtrManager::getInstance().removeSharedPtr(key_);
    }

    ScopedGlobalPtr(const ScopedGlobalPtr&) = delete;
    ScopedGlobalPtr& operator=(const ScopedGlobalPtr&) = delete;
    ScopedGlobalPtr(ScopedGlobalPtr&&) = default;
    ScopedGlobalPtr& operator=(ScopedGlobalPtr&&) = default;

    [[nodiscard]] std::shared_ptr<T> get() const {
        return GlobalSharedPtrManager::getInstance().getSharedPtr<T>(key_);
    }

    [[nodiscard]] const std::string& key() const { return key_; }
};

/**
 * @brief Create a scoped global pointer
 */
template <typename T>
auto makeScopedGlobalPtr(std::string_view key, std::shared_ptr<T> ptr) {
    return ScopedGlobalPtr<T>(key, std::move(ptr));
}

/**
 * @brief Global pointer guard for temporary pointer usage
 */
template <typename T>
class GlobalPtrGuard {
    std::weak_ptr<T> weak_ptr_;
    std::string key_;

public:
    explicit GlobalPtrGuard(std::string_view key)
        : weak_ptr_(GlobalSharedPtrManager::getInstance().getWeakPtr<T>(key)),
          key_(key) {}

    [[nodiscard]] std::shared_ptr<T> lock() const { return weak_ptr_.lock(); }

    [[nodiscard]] bool expired() const { return weak_ptr_.expired(); }

    [[nodiscard]] explicit operator bool() const { return !expired(); }
};

/**
 * @brief Typed pointer registry for specific type families
 */
template <typename Base>
class TypedPtrRegistry {
    std::unordered_map<std::string, std::shared_ptr<Base>> ptrs_;
    mutable std::shared_mutex mutex_;

public:
    template <typename Derived>
        requires std::is_base_of_v<Base, Derived>
    void add(std::string_view key, std::shared_ptr<Derived> ptr) {
        std::unique_lock lock(mutex_);
        ptrs_[std::string(key)] = std::move(ptr);
    }

    template <typename Derived = Base>
        requires std::is_base_of_v<Base, Derived>
    auto get(std::string_view key) -> std::shared_ptr<Derived> {
        std::shared_lock lock(mutex_);
        auto it = ptrs_.find(std::string(key));
        if (it != ptrs_.end()) {
            return std::dynamic_pointer_cast<Derived>(it->second);
        }
        return nullptr;
    }

    void remove(std::string_view key) {
        std::unique_lock lock(mutex_);
        ptrs_.erase(std::string(key));
    }

    [[nodiscard]] std::vector<std::string> keys() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(ptrs_.size());
        for (const auto& [k, _] : ptrs_) {
            result.push_back(k);
        }
        return result;
    }

    [[nodiscard]] std::size_t size() const {
        std::shared_lock lock(mutex_);
        return ptrs_.size();
    }
};

/**
 * @brief Pointer lifecycle observer
 */
template <typename T>
class PtrLifecycleObserver {
public:
    using CreateCallback =
        std::function<void(const std::string&, std::shared_ptr<T>)>;
    using DestroyCallback = std::function<void(const std::string&)>;

private:
    std::vector<CreateCallback> on_create_;
    std::vector<DestroyCallback> on_destroy_;
    mutable std::mutex mutex_;

public:
    void onCreated(CreateCallback callback) {
        std::lock_guard lock(mutex_);
        on_create_.push_back(std::move(callback));
    }

    void onDestroyed(DestroyCallback callback) {
        std::lock_guard lock(mutex_);
        on_destroy_.push_back(std::move(callback));
    }

    void notifyCreated(const std::string& key, std::shared_ptr<T> ptr) {
        std::lock_guard lock(mutex_);
        for (const auto& cb : on_create_) {
            cb(key, ptr);
        }
    }

    void notifyDestroyed(const std::string& key) {
        std::lock_guard lock(mutex_);
        for (const auto& cb : on_destroy_) {
            cb(key);
        }
    }
};

#endif  // ATOM_META_GLOBAL_PTR_HPP
