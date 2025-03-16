/*!
 * \file global_ptr.hpp
 * \brief Enhanced global shared pointer manager with improved cross-platform
 * support \author Max Qian <lightapt.com> \date 2023-06-17 \update 2024-03-11
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
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
#include <unordered_set>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#else
#include <unordered_map>
#endif

#include "atom/error/exception.hpp"
#include "atom/type/noncopyable.hpp"

#define GetPtr GlobalSharedPtrManager::getInstance().getSharedPtr
#define GetWeakPtr GlobalSharedPtrManager::getInstance().getWeakPtrFromSharedPtr
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

#define GET_OR_CREATE_PTR_IMPL(variable, type, constant, capture, create_expr, \
                               weak_ptr, deleter)                              \
    if (auto ptr = GetPtrOrCreate<type>(constant, capture create_expr)) {      \
        variable = weak_ptr ? std::weak_ptr(ptr) : ptr;                        \
    } else {                                                                   \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");               \
    }

#define GET_OR_CREATE_PTR_WITH_CAPTURE(variable, type, constant, capture) \
    GET_OR_CREATE_PTR_IMPL(                                               \
        variable, type, constant, [capture],                              \
        { return atom::memory::makeShared<type>(capture); }, false, nullptr)

#define GET_OR_CREATE_PTR(variable, type, constant, ...)                     \
    if (auto ptr = GetPtrOrCreate<type>(                                     \
            constant, [] { return std::make_shared<type>(__VA_ARGS__); })) { \
        variable = ptr;                                                      \
    } else {                                                                 \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");             \
    }

#define GET_OR_CREATE_PTR_THIS(variable, type, constant, ...)    \
    if (auto ptr = GetPtrOrCreate<type>(constant, [this] {       \
            return std::make_shared<type>(__VA_ARGS__);          \
        })) {                                                    \
        variable = ptr;                                          \
    } else {                                                     \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type "."); \
    }

#define GET_OR_CREATE_WEAK_PTR(variable, type, constant, ...)                \
    if (auto ptr = GetPtrOrCreate<type>(                                     \
            constant, [] { return std::make_shared<type>(__VA_ARGS__); })) { \
        variable = std::weak_ptr(ptr);                                       \
    } else {                                                                 \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");             \
    }

#define GET_OR_CREATE_PTR_WITH_DELETER(variable, type, constant, deleter) \
    if (auto ptr = GetPtrOrCreate<type>(constant, [deleter] {             \
            return std::shared_ptr<type>(new type, deleter);              \
        })) {                                                             \
        variable = ptr;                                                   \
    } else {                                                              \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");          \
    }

/**
 * @brief Structure to hold pointer metadata
 */
struct PointerMetadata {
    std::chrono::system_clock::time_point creation_time;
    size_t access_count{0};
    size_t ref_count{0};
    std::string type_name;
    bool is_weak{false};
    bool has_custom_deleter{false};
};

/**
 * @brief Enhanced GlobalSharedPtrManager with improved functionality and
 * performance
 */
class GlobalSharedPtrManager : public NonCopyable {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

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
     * @brief Remove pointer by key
     * @param key Key to remove
     */
    void removeSharedPtr(std::string_view key);

    /**
     * @brief Add weak pointer with key
     * @tparam T Pointer type
     * @param key Lookup key
     * @param ptr Weak pointer to add
     */
    template <typename T>
    void addWeakPtr(std::string_view key, const std::weak_ptr<T>& ptr);

    /**
     * @brief Get shared pointer from stored weak pointer
     * @tparam T Pointer type
     * @param key Lookup key
     * @return Shared pointer if weak pointer valid
     */
    template <typename T>
    [[nodiscard]] auto getSharedPtrFromWeakPtr(std::string_view key)
        -> std::shared_ptr<T>;

    /**
     * @brief Get weak pointer from stored shared pointer
     * @tparam T Pointer type
     * @param key Lookup key
     * @return Weak pointer to the object
     */
    template <typename T>
    [[nodiscard]] auto getWeakPtrFromSharedPtr(std::string_view key)
        -> std::weak_ptr<T>;

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
     * @brief Delete object using custom deleter
     * @tparam T Object type
     * @param key Lookup key
     * @param ptr Pointer to delete
     */
    template <typename T>
    void deleteObject(std::string_view key, T* ptr);

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
    // Prevent external construction
    GlobalSharedPtrManager() = default;

#if ENABLE_FASTHASH
    emhash8::HashMap<std::string, std::any> shared_ptr_map_;
    emhash8::HashMap<std::string, PointerMetadata> metadata_map_;
#else
    std::unordered_map<std::string, std::any> shared_ptr_map_;
    std::unordered_map<std::string, PointerMetadata> metadata_map_;
#endif

    mutable std::shared_mutex mutex_;
    std::atomic<size_t> total_access_count_{0};
    std::unordered_set<std::string> expired_keys_;

    /**
     * @brief Update metadata for a key
     * @param key The key to update
     * @param type_name Type name for the pointer
     * @param is_weak Whether pointer is weak
     * @param has_deleter Whether has custom deleter
     */
    void updateMetadata(std::string_view key, const std::string& type_name,
                        bool is_weak = false, bool has_deleter = false);
};

// Template implementations
template <typename T>
auto GlobalSharedPtrManager::getSharedPtr(std::string_view key)
    -> std::optional<std::shared_ptr<T>> {
    std::shared_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            if (auto meta_iter = metadata_map_.find(std::string(key));
                meta_iter != metadata_map_.end()) {
                meta_iter->second.access_count++;
                meta_iter->second.ref_count = ptr.use_count();
            }
            total_access_count_++;
            return ptr;
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

template <typename T, typename CreatorFunc>
auto GlobalSharedPtrManager::getOrCreateSharedPtr(
    std::string_view key, CreatorFunc creator) -> std::shared_ptr<T> {
    std::unique_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            updateMetadata(key, typeid(T).name());
            return ptr;
        } catch (const std::bad_any_cast&) {
            // Key exists but type mismatch - replace
            auto ptr = creator();
            iter->second = ptr;
            updateMetadata(key, typeid(T).name());
            return ptr;
        }
    } else {
        // Create new
        auto ptr = creator();
        shared_ptr_map_[std::string(key)] = ptr;
        updateMetadata(key, typeid(T).name());
        return ptr;
    }
}

template <typename T>
auto GlobalSharedPtrManager::getWeakPtr(std::string_view key)
    -> std::weak_ptr<T> {
    std::shared_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto wptr = std::any_cast<std::weak_ptr<T>>(iter->second);
            if (auto meta_iter = metadata_map_.find(std::string(key));
                meta_iter != metadata_map_.end()) {
                meta_iter->second.access_count++;
            }
            total_access_count_++;
            return wptr;
        } catch (const std::bad_any_cast&) {
            return std::weak_ptr<T>();
        }
    }
    return std::weak_ptr<T>();
}

template <typename T>
void GlobalSharedPtrManager::addSharedPtr(std::string_view key,
                                          std::shared_ptr<T> ptr) {
    std::unique_lock lock(mutex_);
    shared_ptr_map_[std::string(key)] = std::move(ptr);
    updateMetadata(key, typeid(T).name());
}

template <typename T>
void GlobalSharedPtrManager::addWeakPtr(std::string_view key,
                                        const std::weak_ptr<T>& ptr) {
    std::unique_lock lock(mutex_);
    shared_ptr_map_[std::string(key)] = ptr;
    updateMetadata(key, typeid(T).name(), true);
}

template <typename T>
auto GlobalSharedPtrManager::getSharedPtrFromWeakPtr(std::string_view key)
    -> std::shared_ptr<T> {
    std::shared_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::weak_ptr<T>>(iter->second).lock();
            if (ptr && metadata_map_.contains(std::string(key))) {
                metadata_map_[std::string(key)].access_count++;
                metadata_map_[std::string(key)].ref_count = ptr.use_count();
            }
            total_access_count_++;
            return ptr;
        } catch (const std::bad_any_cast&) {
            return std::shared_ptr<T>();
        }
    }
    return std::shared_ptr<T>();
}

template <typename T>
auto GlobalSharedPtrManager::getWeakPtrFromSharedPtr(std::string_view key)
    -> std::weak_ptr<T> {
    std::shared_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            if (metadata_map_.contains(std::string(key))) {
                metadata_map_[std::string(key)].access_count++;
            }
            total_access_count_++;
            return std::weak_ptr<T>(ptr);
        } catch (const std::bad_any_cast&) {
            return std::weak_ptr<T>();
        }
    }
    return std::weak_ptr<T>();
}

template <typename T>
void GlobalSharedPtrManager::addDeleter(
    std::string_view key, const std::function<void(T*)>& deleter) {
    std::unique_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            ptr.reset(ptr.get(), deleter);
            iter->second = ptr;
            if (metadata_map_.contains(std::string(key))) {
                metadata_map_[std::string(key)].has_custom_deleter = true;
            }
        } catch (const std::bad_any_cast&) {
            // Ignore type mismatch
        }
    }
}

template <typename T>
void GlobalSharedPtrManager::deleteObject(std::string_view key, T* ptr) {
    std::unique_lock lock(mutex_);

    auto iter = shared_ptr_map_.find(std::string(key));
    if (iter != shared_ptr_map_.end()) {
        try {
            auto deleter = std::any_cast<std::function<void(T*)>>(iter->second);
            deleter(ptr);
        } catch (const std::bad_any_cast&) {
            delete ptr;  // Fallback to default delete
        }
        shared_ptr_map_.erase(iter);
        metadata_map_.erase(std::string(key));
    }
}

#endif  // ATOM_META_GLOBAL_PTR_HPP
