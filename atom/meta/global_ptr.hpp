/*!
 * \file global_ptr.hpp
 * \brief Enhanced global shared pointer manager with improved cross-platform
 * support
 * \author Max Qian <lightapt.com>
 * \date 2023-06-17
 * \update 2024-03-11
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

#define GET_OR_CREATE_PTR_WITH_CAPTURE(variable, type, constant, capture) \
    if (auto ptr = GetPtrOrCreate<type>(constant, [capture] {             \
            return atom::memory::makeShared<type>(capture);               \
        })) {                                                             \
        variable = ptr;                                                   \
    } else {                                                              \
        THROW_UNLAWFUL_OPERATION("Failed to create " #type ".");          \
    }

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
    auto getOrCreateSharedPtr(std::string_view key, CreatorFunc creator)
        -> std::shared_ptr<T>;

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
     * @brief Add custom deleter for key
     * @tparam T Object type
     * @param key Lookup key
     * @param deleter Custom deleter function
     */
    template <typename T>
    void addDeleter(std::string_view key,
                    const std::function<void(T*)>& deleter);

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

    /**
     * @brief Find iterator by key efficiently
     * @param key The key to find
     * @return Iterator to the element or end()
     */
    template <typename MapType>
    auto findByKey(MapType& map, std::string_view key) const ->
        typename MapType::iterator;
};

template <typename T>
auto GlobalSharedPtrManager::getSharedPtr(std::string_view key)
    -> std::optional<std::shared_ptr<T>> {
    std::shared_lock lock(mutex_);

    if (auto iter = shared_ptr_map_.find(std::string(key));
        iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            if (auto meta_iter = metadata_map_.find(std::string(key));
                meta_iter != metadata_map_.end()) {
                ++meta_iter->second.access_count;
                meta_iter->second.ref_count = ptr.use_count();
            }
            ++total_access_count_;
            return ptr;
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

template <typename T, typename CreatorFunc>
auto GlobalSharedPtrManager::getOrCreateSharedPtr(std::string_view key,
                                                  CreatorFunc creator)
    -> std::shared_ptr<T> {
    const std::string str_key{key};
    std::unique_lock lock(mutex_);

    if (auto iter = shared_ptr_map_.find(str_key);
        iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            updateMetadata(key, typeid(T).name());
            return ptr;
        } catch (const std::bad_any_cast&) {
            auto ptr = creator();
            iter->second = ptr;
            updateMetadata(key, typeid(T).name());
            return ptr;
        }
    } else {
        auto ptr = creator();
        shared_ptr_map_[str_key] = ptr;
        updateMetadata(key, typeid(T).name());
        return ptr;
    }
}

template <typename T>
auto GlobalSharedPtrManager::getWeakPtr(std::string_view key)
    -> std::weak_ptr<T> {
    std::shared_lock lock(mutex_);

    if (auto iter = shared_ptr_map_.find(std::string(key));
        iter != shared_ptr_map_.end()) {
        try {
            if (auto shared_ptr =
                    std::any_cast<std::shared_ptr<T>>(iter->second)) {
                if (auto meta_iter = metadata_map_.find(std::string(key));
                    meta_iter != metadata_map_.end()) {
                    ++meta_iter->second.access_count;
                }
                ++total_access_count_;
                return std::weak_ptr<T>(shared_ptr);
            }
            auto weak_ptr = std::any_cast<std::weak_ptr<T>>(iter->second);
            if (auto meta_iter = metadata_map_.find(std::string(key));
                meta_iter != metadata_map_.end()) {
                ++meta_iter->second.access_count;
            }
            ++total_access_count_;
            return weak_ptr;
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
void GlobalSharedPtrManager::addDeleter(
    std::string_view key, const std::function<void(T*)>& deleter) {
    std::unique_lock lock(mutex_);

    if (auto iter = shared_ptr_map_.find(std::string(key));
        iter != shared_ptr_map_.end()) {
        try {
            auto ptr = std::any_cast<std::shared_ptr<T>>(iter->second);
            ptr.reset(ptr.get(), deleter);
            iter->second = ptr;
            if (auto meta_iter = metadata_map_.find(std::string(key));
                meta_iter != metadata_map_.end()) {
                meta_iter->second.has_custom_deleter = true;
            }
        } catch (const std::bad_any_cast&) {
            // Ignore type mismatch
        }
    }
}

#endif  // ATOM_META_GLOBAL_PTR_HPP
