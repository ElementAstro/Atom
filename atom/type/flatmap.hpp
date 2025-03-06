/*
 * flatmap.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-2

Description: QuickFlatMap for C++20 with optional Boost support

**************************************************/

#ifndef ATOM_TYPE_FLATMAP_HPP
#define ATOM_TYPE_FLATMAP_HPP

#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
// Cache line size for most modern CPUs
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

// SIMD support detection
#if defined(__AVX2__)
#include <immintrin.h>
#define ATOM_SIMD_ENABLED
#endif

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/thread/shared_mutex.hpp>
#else
#include <algorithm>
#include <iterator>
#include <vector>
#endif

// Custom exception classes
namespace atom::type::exceptions {
class flat_map_error : public std::runtime_error {
public:
    explicit flat_map_error(const std::string& msg) : std::runtime_error(msg) {}
};

class key_not_found_error : public flat_map_error {
public:
    explicit key_not_found_error(const std::string& msg)
        : flat_map_error(msg) {}
};

class container_full_error : public flat_map_error {
public:
    explicit container_full_error(const std::string& msg)
        : flat_map_error(msg) {}
};

class invalid_operation_error : public flat_map_error {
public:
    explicit invalid_operation_error(const std::string& msg)
        : flat_map_error(msg) {}
};
}  // namespace atom::type::exceptions

namespace atom::type {

// Default capacity for containers
constexpr std::size_t DEFAULT_INITIAL_CAPACITY = 16;
constexpr std::size_t MAX_CONTAINER_SIZE =
    std::numeric_limits<std::size_t>::max() / 2;

// Thread safety modes
enum class ThreadSafetyMode {
    None,      // No thread safety
    ReadOnly,  // Multiple readers allowed
    ReadWrite  // Full read-write protection
};

#ifdef ATOM_USE_BOOST

template <typename Key, typename Value, typename Comparator = std::less<Key>,
          ThreadSafetyMode SafetyMode = ThreadSafetyMode::None>
class QuickFlatMap {
public:
    using value_type = std::pair<Key, Value>;
    using container_type = boost::container::flat_map<Key, Value, Comparator>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;

    // Constructors
    QuickFlatMap() = default;

    explicit QuickFlatMap(size_type initialCapacity) {
        try {
            if (initialCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Initial capacity exceeds maximum container size");
            }
            data_.reserve(initialCapacity);
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during construction");
        }
    }

    // Copy constructor with deep copy semantics
    QuickFlatMap(const QuickFlatMap& other) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(other.mutex_);
            data_ = other.data_;
        } else {
            data_ = other.data_;
        }
    }

    // Move constructor
    QuickFlatMap(QuickFlatMap&& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } else {
            data_ = std::move(other.data_);
        }
    }

    // Copy assignment operator
    QuickFlatMap& operator=(const QuickFlatMap& other) {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::shared_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = other.data_;
            } else {
                data_ = other.data_;
            }
        }
        return *this;
    }

    // Move assignment operator
    QuickFlatMap& operator=(QuickFlatMap&& other) noexcept {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::unique_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = std::move(other.data_);
            } else {
                data_ = std::move(other.data_);
            }
        }
        return *this;
    }

    // Finders
    template <typename Lookup>
    iterator find(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.find(s);
        } else {
            return data_.find(s);
        }
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.find(s);
        } else {
            return data_.find(s);
        }
    }

    // Size and capacity management
    [[nodiscard]] size_type size() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.size();
        } else {
            return data_.size();
        }
    }

    [[nodiscard]] size_type capacity() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.capacity();
        } else {
            return data_.capacity();
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.empty();
        } else {
            return data_.empty();
        }
    }

    void reserve(size_type newCapacity) {
        try {
            if (newCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Requested capacity exceeds maximum container size");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.reserve(newCapacity);
            } else {
                data_.reserve(newCapacity);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during reserve");
        }
    }

    // Iterators
    iterator begin() {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    const_iterator begin() const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    iterator end() {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    const_iterator end() const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    // Thread-safe access methods
    std::optional<Value> try_get(const Key& key) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto it = data_.find(key);
            if (it != data_.end()) {
                return it->second;
            }
        } else {
            auto it = data_.find(key);
            if (it != data_.end()) {
                return it->second;
            }
        }
        return std::nullopt;
    }

    // Operator []
    Value& operator[](const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                return data_[s];
            } else {
                return data_[s];
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during operator[]");
        }
    }

    // At methods with bounds checking
    Value& at(const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::shared_lock lock(mutex_);
                return data_.at(s);
            } else {
                return data_.at(s);
            }
        } catch (const std::out_of_range&) {
            throw exceptions::key_not_found_error("Key not found in map");
        }
    }

    const Value& at(const Key& s) const {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::shared_lock lock(mutex_);
                return data_.at(s);
            } else {
                return data_.at(s);
            }
        } catch (const std::out_of_range&) {
            throw exceptions::key_not_found_error("Key not found in map");
        }
    }

    // Insert or assign with perfect forwarding
    template <typename M>
    std::pair<iterator, bool> insertOrAssign(const Key& key, M&& m) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.insert_or_assign(key, std::forward<M>(m));
            } else {
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.insert_or_assign(key, std::forward<M>(m));
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Insert value
    std::pair<iterator, bool> insert(value_type value) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.insert(std::move(value));
            } else {
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.insert(std::move(value));
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Assign range
    template <typename Itr>
    void assign(Itr first, Itr last) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                // Calculate range size for validation
                auto count = std::distance(first, last);
                if (count < 0 ||
                    static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Range size exceeds container limits");
                }
                data_.clear();
                data_.insert(first, last);
            } else {
                // Calculate range size for validation
                auto count = std::distance(first, last);
                if (count < 0 ||
                    static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Range size exceeds container limits");
                }
                data_.clear();
                data_.insert(first, last);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during assign");
        }
    }

    // Check if contains key
    bool contains(const Key& s) const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.contains(s);
        } else {
            return data_.contains(s);
        }
    }

    // Erase key
    bool erase(const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                return data_.erase(s) > 0;
            } else {
                return data_.erase(s) > 0;
            }
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during erase: ") + e.what());
        }
    }

    // Clear container
    void clear() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
        } else {
            data_.clear();
        }
    }

    // Thread-safe methods for atomic operations
    template <typename F>
    auto with_read_lock(F&& func) const
        -> decltype(func(std::declval<const container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

    template <typename F>
    auto with_write_lock(F&& func)
        -> decltype(func(std::declval<container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

private:
    container_type data_;

    // Thread safety mechanism using shared mutex
    mutable std::conditional_t<SafetyMode != ThreadSafetyMode::None,
                               std::shared_mutex, std::byte>
        mutex_;
};

template <typename Key, typename Value, typename Comparator = std::less<Key>,
          ThreadSafetyMode SafetyMode = ThreadSafetyMode::None>
class QuickFlatMultiMap {
public:
    using value_type = std::pair<Key, Value>;
    using container_type =
        boost::container::flat_multimap<Key, Value, Comparator>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;

    // Constructors
    QuickFlatMultiMap() = default;

    explicit QuickFlatMultiMap(size_type initialCapacity) {
        try {
            if (initialCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Initial capacity exceeds maximum container size");
            }
            data_.reserve(initialCapacity);
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during construction");
        }
    }

    // Copy constructor
    QuickFlatMultiMap(const QuickFlatMultiMap& other) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(other.mutex_);
            data_ = other.data_;
        } else {
            data_ = other.data_;
        }
    }

    // Move constructor
    QuickFlatMultiMap(QuickFlatMultiMap&& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } else {
            data_ = std::move(other.data_);
        }
    }

    // Copy assignment operator
    QuickFlatMultiMap& operator=(const QuickFlatMultiMap& other) {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::shared_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = other.data_;
            } else {
                data_ = other.data_;
            }
        }
        return *this;
    }

    // Move assignment operator
    QuickFlatMultiMap& operator=(QuickFlatMultiMap&& other) noexcept {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::unique_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = std::move(other.data_);
            } else {
                data_ = std::move(other.data_);
            }
        }
        return *this;
    }

    // Find methods
    template <typename Lookup>
    iterator find(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.find(s);
        } else {
            return data_.find(s);
        }
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.find(s);
        } else {
            return data_.find(s);
        }
    }

    // Equal range
    template <typename Lookup>
    std::pair<iterator, iterator> equalRange(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.equal_range(s);
        } else {
            return data_.equal_range(s);
        }
    }

    template <typename Lookup>
    std::pair<const_iterator, const_iterator> equalRange(
        const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.equal_range(s);
        } else {
            return data_.equal_range(s);
        }
    }

    // Size and capacity
    [[nodiscard]] size_type size() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.size();
        } else {
            return data_.size();
        }
    }

    [[nodiscard]] size_type capacity() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.capacity();
        } else {
            return data_.capacity();
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.empty();
        } else {
            return data_.empty();
        }
    }

    void reserve(size_type newCapacity) {
        try {
            if (newCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Requested capacity exceeds maximum container size");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.reserve(newCapacity);
            } else {
                data_.reserve(newCapacity);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during reserve");
        }
    }

    // Iterators
    iterator begin() {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    const_iterator begin() const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    iterator end() {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    const_iterator end() const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    // Thread-safe retrieval
    std::vector<Value> get_all(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto [first, last] = data_.equal_range(s);
            std::vector<Value> result;
            result.reserve(std::distance(first, last));
            for (auto it = first; it != last; ++it) {
                result.push_back(it->second);
            }
            return result;
        } else {
            auto [first, last] = data_.equal_range(s);
            std::vector<Value> result;
            result.reserve(std::distance(first, last));
            for (auto it = first; it != last; ++it) {
                result.push_back(it->second);
            }
            return result;
        }
    }

    // Access with default inserted value
    Value& operator[](const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.emplace(s, Value())->second;
            } else {
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                return data_.emplace(s, Value())->second;
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during operator[]");
        }
    }

    // At with bounds checking
    Value& at(const Key& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = data_.find(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = data_.find(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in multimap");
    }

    const Value& at(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = data_.find(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = data_.find(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in multimap");
    }

    // Insert
    std::pair<iterator, bool> insert(value_type value) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                auto itr = data_.insert(std::move(value));
                return {itr, true};
            } else {
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                auto itr = data_.insert(std::move(value));
                return {itr, true};
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Assign range
    template <typename Itr>
    void assign(Itr first, Itr last) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                // Calculate range size for validation
                auto count = std::distance(first, last);
                if (count < 0 ||
                    static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Range size exceeds container limits");
                }
                data_.clear();
                data_.insert(first, last);
            } else {
                // Calculate range size for validation
                auto count = std::distance(first, last);
                if (count < 0 ||
                    static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Range size exceeds container limits");
                }
                data_.clear();
                data_.insert(first, last);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during assign");
        }
    }

    // Count elements with key
    size_type count(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.count(s);
        } else {
            return data_.count(s);
        }
    }

    // Contains key
    bool contains(const Key& s) const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.contains(s);
        } else {
            return data_.contains(s);
        }
    }

    // Erase key
    bool erase(const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                return data_.erase(s) > 0;
            } else {
                return data_.erase(s) > 0;
            }
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during erase: ") + e.what());
        }
    }

    // Clear container
    void clear() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
        } else {
            data_.clear();
        }
    }

    // Thread-safe methods for atomic operations
    template <typename F>
    auto with_read_lock(F&& func) const
        -> decltype(func(std::declval<const container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

    template <typename F>
    auto with_write_lock(F&& func)
        -> decltype(func(std::declval<container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

private:
    container_type data_;

    // Thread safety mechanism
    mutable std::conditional_t<SafetyMode != ThreadSafetyMode::None,
                               std::shared_mutex, std::byte>
        mutex_;
};

#else  // Standard C++ implementation without Boost

// Forward declaration of SIMD-enabled search for sorted vectors
namespace detail {
template <typename Key, typename Value, typename Comparator>
std::pair<bool, size_t> simd_search(
    const std::vector<std::pair<Key, Value> >& data, const Key& key,
    Comparator& comp);
}

template <typename Key, typename Value, typename Comparator = std::less<>,
          ThreadSafetyMode SafetyMode = ThreadSafetyMode::None,
          bool UseSortedVector = false>
    requires std::predicate<Comparator, Key, Key>
class QuickFlatMap {
public:
    using value_type = std::pair<Key, Value>;
    using container_type = std::vector<value_type>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;
    using allocator_type = std::allocator<value_type>;

    // Smart pointer for optional batch operations
    using value_ptr = std::shared_ptr<Value>;

    // Constructors
    QuickFlatMap() : data_(allocator_type()) {
        data_.reserve(DEFAULT_INITIAL_CAPACITY);
    }

    explicit QuickFlatMap(size_type initialCapacity) : data_(allocator_type()) {
        try {
            if (initialCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Initial capacity exceeds maximum container size");
            }
            data_.reserve(initialCapacity);
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during construction");
        }
    }

    // Copy constructor with deep copy semantics
    QuickFlatMap(const QuickFlatMap& other) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(other.mutex_);
            data_ = other.data_;
        } else {
            data_ = other.data_;
        }
    }

    // Move constructor
    QuickFlatMap(QuickFlatMap&& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } else {
            data_ = std::move(other.data_);
        }
    }

    // Copy assignment operator
    QuickFlatMap& operator=(const QuickFlatMap& other) {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::shared_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = other.data_;
            } else {
                data_ = other.data_;
            }
        }
        return *this;
    }

    // Move assignment operator
    QuickFlatMap& operator=(QuickFlatMap&& other) noexcept {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::unique_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = std::move(other.data_);
            } else {
                data_ = std::move(other.data_);
            }
        }
        return *this;
    }

    // Find element with key - optimized with SIMD if available
    template <typename Lookup>
    iterator find(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s);
        } else {
            return find_impl(s);
        }
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s);
        } else {
            return find_impl(s);
        }
    }

    // Helper method that implements the actual find logic with optimizations
    template <typename Lookup>
    iterator find_impl(const Lookup& s) {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto it =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (it != data_.end() && !comp(s, it->first) &&
                !comp(it->first, s)) {
                return it;
            }
            return data_.end();
        } else {
// Standard linear search with potential SIMD acceleration
#ifdef ATOM_SIMD_ENABLED
            if constexpr (std::is_same_v<Key, int> &&
                          std::is_same_v<Lookup, int> && data_.size() >= 16) {
                auto [found, index] =
                    detail::simd_search(data_, s, comparator_);
                return found ? data_.begin() + index : data_.end();
            } else {
#endif
                // Regular linear search for non-SIMD cases
                return std::ranges::find_if(data_, [&s, this](const auto& d) {
                    return comparator_(d.first, s);
                });
#ifdef ATOM_SIMD_ENABLED
            }
#endif
        }
    }

    template <typename Lookup>
    const_iterator find_impl(const Lookup& s) const {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto it =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (it != data_.end() && !comp(s, it->first) &&
                !comp(it->first, s)) {
                return it;
            }
            return data_.end();
        } else {
// Standard linear search with potential SIMD acceleration
#ifdef ATOM_SIMD_ENABLED
            if constexpr (std::is_same_v<Key, int> &&
                          std::is_same_v<Lookup, int> && data_.size() >= 16) {
                auto [found, index] =
                    detail::simd_search(data_, s, comparator_);
                return found ? data_.begin() + index : data_.end();
            } else {
#endif
                // Regular linear search for non-SIMD cases
                return std::ranges::find_if(data_, [&s, this](const auto& d) {
                    return comparator_(d.first, s);
                });
#ifdef ATOM_SIMD_ENABLED
            }
#endif
        }
    }

    // Size and capacity management
    [[nodiscard]] size_type size() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.size();
        } else {
            return data_.size();
        }
    }

    [[nodiscard]] size_type capacity() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.capacity();
        } else {
            return data_.capacity();
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.empty();
        } else {
            return data_.empty();
        }
    }

    void reserve(size_type newCapacity) {
        try {
            if (newCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Requested capacity exceeds maximum container size");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.reserve(newCapacity);
            } else {
                data_.reserve(newCapacity);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during reserve");
        }
    }

    // Iterators
    iterator begin() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    const_iterator begin() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    iterator end() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    const_iterator end() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    // Thread-safe access methods
    std::optional<Value> try_get(const Key& key) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto it = find_impl(key);
            if (it != data_.end()) {
                return it->second;
            }
        } else {
            auto it = find_impl(key);
            if (it != data_.end()) {
                return it->second;
            }
        }
        return std::nullopt;
    }

    // Operator []
    Value& operator[](const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    return itr->second;
                }
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                grow();
                auto& result = data_.emplace_back(s, Value()).second;

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                }

                return result;
            } else {
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    return itr->second;
                }
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }
                grow();
                auto& result = data_.emplace_back(s, Value()).second;

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                }

                return result;
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during operator[]");
        }
    }

    // At methods with bounds checking
    Value& at(const Key& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in map");
    }

    const Value& at(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in map");
    }

    // Insert or assign
    template <typename M>
    std::pair<iterator, bool> insertOrAssign(const Key& key, M&& m) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto itr = find_impl(key);
                if (itr != data_.end()) {
                    itr->second = std::forward<M>(m);
                    return {itr, false};
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position =
                    data_.emplace(data_.end(), key, std::forward<M>(m));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    itr = find_impl(key);
                    return {itr, true};
                } else {
                    return {position, true};
                }
            } else {
                auto itr = find_impl(key);
                if (itr != data_.end()) {
                    itr->second = std::forward<M>(m);
                    return {itr, false};
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position =
                    data_.emplace(data_.end(), key, std::forward<M>(m));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    itr = find_impl(key);
                    return {itr, true};
                } else {
                    return {position, true};
                }
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Insert element
    std::pair<iterator, bool> insert(value_type value) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto itr = find_impl(value.first);
                if (itr != data_.end()) {
                    return {itr, false};
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position = data_.insert(data_.end(), std::move(value));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    itr = find_impl(value.first);
                    return {itr, true};
                } else {
                    return {position, true};
                }
            } else {
                auto itr = find_impl(value.first);
                if (itr != data_.end()) {
                    return {itr, false};
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position = data_.insert(data_.end(), std::move(value));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    itr = find_impl(value.first);
                    return {itr, true};
                } else {
                    return {position, true};
                }
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Assign range
    template <typename Itr>
    void assign(Itr first, Itr last) {
        try {
            auto count = std::distance(first, last);
            if (count < 0 ||
                static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Range size exceeds container limits");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.clear();
                data_.reserve(count);
                data_.assign(first, last);

                if constexpr (UseSortedVector) {
                    sort_container();
                }
            } else {
                data_.clear();
                data_.reserve(count);
                data_.assign(first, last);

                if constexpr (UseSortedVector) {
                    sort_container();
                }
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during assign");
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during assign: ") + e.what());
        }
    }

    // Growth strategy for vector
    void grow() {
        if (data_.capacity() == data_.size()) {
            size_type newCap = data_.size() * 2;
            if (newCap < DEFAULT_INITIAL_CAPACITY) {
                newCap = DEFAULT_INITIAL_CAPACITY;
            } else if (newCap > MAX_CONTAINER_SIZE) {
                newCap = MAX_CONTAINER_SIZE;
            }
            data_.reserve(newCap);
        }
    }

    // Sort the container (for UseSortedVector mode)
    void sort_container() {
        std::sort(data_.begin(), data_.end(),
                  [this](const auto& a, const auto& b) {
                      return comparator_(a.first, b.first);
                  });
    }

    // Check if contains key
    bool contains(const Key& s) const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s) != data_.end();
        } else {
            return find_impl(s) != data_.end();
        }
    }

    // Erase key
    bool erase(const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    data_.erase(itr);
                    return true;
                }
            } else {
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    data_.erase(itr);
                    return true;
                }
            }
            return false;
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during erase: ") + e.what());
        }
    }

    // Clear container
    void clear() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
        } else {
            data_.clear();
        }
    }

    // Batch operations with smart pointers to avoid copies
    value_ptr get_ptr(const Key& key) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto it = find_impl(key);
            if (it != data_.end()) {
                return std::make_shared<Value>(it->second);
            }
        } else {
            auto it = find_impl(key);
            if (it != data_.end()) {
                return std::make_shared<Value>(it->second);
            }
        }
        return nullptr;
    }

    // Thread-safe methods for atomic operations
    template <typename F>
    auto with_read_lock(F&& func) const
        -> decltype(func(std::declval<const container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

    template <typename F>
    auto with_write_lock(F&& func)
        -> decltype(func(std::declval<container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

private:
    container_type data_;
    Comparator comparator_{};

    // Thread safety mechanism
    mutable std::conditional_t<SafetyMode != ThreadSafetyMode::None,
                               std::shared_mutex, std::byte>
        mutex_;
};

template <typename Key, typename Value, typename Comparator = std::equal_to<>,
          ThreadSafetyMode SafetyMode = ThreadSafetyMode::None,
          bool UseSortedVector = false>
    requires std::predicate<Comparator, Key, Key>
class QuickFlatMultiMap {
public:
    using value_type = std::pair<Key, Value>;
    using container_type = std::vector<value_type>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using size_type = typename container_type::size_type;
    using allocator_type = std::allocator<value_type>;

    // Smart pointer for optional batch operations
    using value_ptr = std::shared_ptr<Value>;

    // Constructors
    QuickFlatMultiMap() : data_(allocator_type()) {
        data_.reserve(DEFAULT_INITIAL_CAPACITY);
    }

    explicit QuickFlatMultiMap(size_type initialCapacity)
        : data_(allocator_type()) {
        try {
            if (initialCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Initial capacity exceeds maximum container size");
            }
            data_.reserve(initialCapacity);
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during construction");
        }
    }

    // Copy constructor with deep copy semantics
    QuickFlatMultiMap(const QuickFlatMultiMap& other) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(other.mutex_);
            data_ = other.data_;
        } else {
            data_ = other.data_;
        }
    }

    // Move constructor
    QuickFlatMultiMap(QuickFlatMultiMap&& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(other.mutex_);
            data_ = std::move(other.data_);
        } else {
            data_ = std::move(other.data_);
        }
    }

    // Copy assignment operator
    QuickFlatMultiMap& operator=(const QuickFlatMultiMap& other) {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::shared_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = other.data_;
            } else {
                data_ = other.data_;
            }
        }
        return *this;
    }

    // Move assignment operator
    QuickFlatMultiMap& operator=(QuickFlatMultiMap&& other) noexcept {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::unique_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = std::move(other.data_);
            } else {
                data_ = std::move(other.data_);
            }
        }
        return *this;
    }

    // Find element with key
    template <typename Lookup>
    iterator find(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s);
        } else {
            return find_impl(s);
        }
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s);
        } else {
            return find_impl(s);
        }
    }

    // Helper method that implements the actual find logic with optimizations
    template <typename Lookup>
    iterator find_impl(const Lookup& s) {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto it =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (it != data_.end() && !comp(s, it->first) &&
                !comp(it->first, s)) {
                return it;
            }
            return data_.end();
        } else {
// Standard linear search with potential SIMD acceleration
#ifdef ATOM_SIMD_ENABLED
            if constexpr (std::is_same_v<Key, int> &&
                          std::is_same_v<Lookup, int> && data_.size() >= 16) {
                auto [found, index] =
                    detail::simd_search(data_, s, comparator_);
                return found ? data_.begin() + index : data_.end();
            } else {
#endif
                // Regular linear search for non-SIMD cases
                return std::ranges::find_if(data_, [&s, this](const auto& d) {
                    return comparator_(d.first, s);
                });
#ifdef ATOM_SIMD_ENABLED
            }
#endif
        }
    }

    template <typename Lookup>
    const_iterator find_impl(const Lookup& s) const {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto it =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (it != data_.end() && !comp(s, it->first) &&
                !comp(it->first, s)) {
                return it;
            }
            return data_.end();
        } else {
// Standard linear search with potential SIMD acceleration
#ifdef ATOM_SIMD_ENABLED
            if constexpr (std::is_same_v<Key, int> &&
                          std::is_same_v<Lookup, int> && data_.size() >= 16) {
                auto [found, index] =
                    detail::simd_search(data_, s, comparator_);
                return found ? data_.begin() + index : data_.end();
            } else {
#endif
                // Regular linear search for non-SIMD cases
                return std::ranges::find_if(data_, [&s, this](const auto& d) {
                    return comparator_(d.first, s);
                });
#ifdef ATOM_SIMD_ENABLED
            }
#endif
        }
    }

    // Equal range for multi-map operations
    template <typename Lookup>
    std::pair<iterator, iterator> equalRange(const Lookup& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return equal_range_impl(s);
        } else {
            return equal_range_impl(s);
        }
    }

    template <typename Lookup>
    std::pair<const_iterator, const_iterator> equalRange(
        const Lookup& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return equal_range_impl(s);
        } else {
            return equal_range_impl(s);
        }
    }

    // Helper method for equal range implementation with optimizations
    template <typename Lookup>
    std::pair<iterator, iterator> equal_range_impl(const Lookup& s) {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto lower =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (lower == data_.end() || comp(s, lower->first) ||
                comp(lower->first, s)) {
                return {lower, lower};
            }

            auto upper =
                std::upper_bound(lower, data_.end(), s,
                                 [&comp](const auto& key, const auto& pair) {
                                     return comp(key, pair.first);
                                 });

            return {lower, upper};
        } else {
            auto lower = std::ranges::find_if(data_, [&s, this](const auto& d) {
                return comparator_(d.first, s);
            });

            if (lower == data_.end()) {
                return {lower, lower};
            }

            auto upper = std::ranges::find_if_not(
                lower, data_.end(),
                [&s, this](const auto& d) { return comparator_(d.first, s); });

            return {lower, upper};
        }
    }

    template <typename Lookup>
    std::pair<const_iterator, const_iterator> equal_range_impl(
        const Lookup& s) const {
        if constexpr (UseSortedVector) {
            // Use binary search for sorted vectors
            auto comp = [this](const auto& a, const auto& b) {
                return comparator_(a, b);
            };

            auto lower =
                std::lower_bound(data_.begin(), data_.end(), s,
                                 [&comp](const auto& pair, const auto& key) {
                                     return comp(pair.first, key);
                                 });

            if (lower == data_.end() || comp(s, lower->first) ||
                comp(lower->first, s)) {
                return {lower, lower};
            }

            auto upper =
                std::upper_bound(lower, data_.end(), s,
                                 [&comp](const auto& key, const auto& pair) {
                                     return comp(key, pair.first);
                                 });

            return {lower, upper};
        } else {
            auto lower = std::ranges::find_if(data_, [&s, this](const auto& d) {
                return comparator_(d.first, s);
            });

            if (lower == data_.cend()) {
                return {lower, lower};
            }

            auto upper = std::ranges::find_if_not(
                lower, data_.cend(),
                [&s, this](const auto& d) { return comparator_(d.first, s); });

            return {lower, upper};
        }
    }

    // Size and capacity management
    [[nodiscard]] size_type size() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.size();
        } else {
            return data_.size();
        }
    }

    [[nodiscard]] size_type capacity() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.capacity();
        } else {
            return data_.capacity();
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.empty();
        } else {
            return data_.empty();
        }
    }

    void reserve(size_type newCapacity) {
        try {
            if (newCapacity > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Requested capacity exceeds maximum container size");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.reserve(newCapacity);
            } else {
                data_.reserve(newCapacity);
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during reserve");
        }
    }

    // Iterators
    iterator begin() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    const_iterator begin() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.begin();
        } else {
            return data_.begin();
        }
    }

    iterator end() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    const_iterator end() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            // Note: This is unsafe without external synchronization
            return data_.end();
        } else {
            return data_.end();
        }
    }

    // Thread-safe retrieval
    std::vector<Value> get_all(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto [first, last] = equal_range_impl(s);
            std::vector<Value> result;
            result.reserve(std::distance(first, last));
            for (auto it = first; it != last; ++it) {
                result.push_back(it->second);
            }
            return result;
        } else {
            auto [first, last] = equal_range_impl(s);
            std::vector<Value> result;
            result.reserve(std::distance(first, last));
            for (auto it = first; it != last; ++it) {
                result.push_back(it->second);
            }
            return result;
        }
    }

    // Operator []
    Value& operator[](const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    return itr->second;
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto& result = data_.emplace_back(s, Value()).second;

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the element we just inserted
                    itr = find_impl(s);
                    return itr->second;
                }

                return result;
            } else {
                auto itr = find_impl(s);
                if (itr != data_.end()) {
                    return itr->second;
                }

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto& result = data_.emplace_back(s, Value()).second;

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the element we just inserted
                    itr = find_impl(s);
                    return itr->second;
                }

                return result;
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during operator[]");
        }
    }

    // At with bounds checking
    Value& at(const Key& s) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in multimap");
    }

    const Value& at(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        } else {
            auto itr = find_impl(s);
            if (itr != data_.end()) {
                return itr->second;
            }
        }
        throw exceptions::key_not_found_error("Key not found in multimap");
    }

    // Insert element
    std::pair<iterator, bool> insert(value_type value) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);

                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position = data_.insert(data_.end(), std::move(value));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    auto itr = find_impl(value.first);
                    return {itr, true};
                }

                return {position, true};
            } else {
                if (data_.size() >= MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Container full, cannot insert more elements");
                }

                grow();
                auto position = data_.insert(data_.end(), std::move(value));

                if constexpr (UseSortedVector) {
                    // Keep the vector sorted after insertion
                    sort_container();
                    // Find the newly inserted element again after sorting
                    auto itr = find_impl(value.first);
                    return {itr, true};
                }

                return {position, true};
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during insert");
        }
    }

    // Assign range
    template <typename Itr>
    void assign(Itr first, Itr last) {
        try {
            auto count = std::distance(first, last);
            if (count < 0 ||
                static_cast<size_type>(count) > MAX_CONTAINER_SIZE) {
                throw exceptions::container_full_error(
                    "Range size exceeds container limits");
            }

            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                data_.clear();
                data_.reserve(count);
                data_.assign(first, last);

                if constexpr (UseSortedVector) {
                    sort_container();
                }
            } else {
                data_.clear();
                data_.reserve(count);
                data_.assign(first, last);

                if constexpr (UseSortedVector) {
                    sort_container();
                }
            }
        } catch (const std::bad_alloc&) {
            throw exceptions::container_full_error(
                "Memory allocation failed during assign");
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during assign: ") + e.what());
        }
    }

    // Growth strategy for vector
    void grow() {
        if (data_.capacity() == data_.size()) {
            size_type newCap = data_.size() * 2;
            if (newCap < DEFAULT_INITIAL_CAPACITY) {
                newCap = DEFAULT_INITIAL_CAPACITY;
            } else if (newCap > MAX_CONTAINER_SIZE) {
                newCap = MAX_CONTAINER_SIZE;
            }
            data_.reserve(newCap);
        }
    }

    // Sort the container (for UseSortedVector mode)
    void sort_container() {
        std::sort(data_.begin(), data_.end(),
                  [this](const auto& a, const auto& b) {
                      return comparator_(a.first, b.first);
                  });
    }

    // Count elements with key
    size_type count(const Key& s) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            auto [lower, upper] = equal_range_impl(s);
            return std::distance(lower, upper);
        } else {
            auto [lower, upper] = equal_range_impl(s);
            return std::distance(lower, upper);
        }
    }

    // Check if contains key
    bool contains(const Key& s) const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(s) != data_.end();
        } else {
            return find_impl(s) != data_.end();
        }
    }

    // Erase all elements with key
    bool erase(const Key& s) {
        try {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_);
                auto [lower, upper] = equal_range_impl(s);
                if (lower != upper) {
                    data_.erase(lower, upper);
                    return true;
                }
            } else {
                auto [lower, upper] = equal_range_impl(s);
                if (lower != upper) {
                    data_.erase(lower, upper);
                    return true;
                }
            }
            return false;
        } catch (const std::exception& e) {
            throw exceptions::flat_map_error(
                std::string("Error during erase: ") + e.what());
        }
    }

    // Clear container
    void clear() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
        } else {
            data_.clear();
        }
    }

    // Thread-safe methods for atomic operations
    template <typename F>
    auto with_read_lock(F&& func) const
        -> decltype(func(std::declval<const container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

    template <typename F>
    auto with_write_lock(F&& func)
        -> decltype(func(std::declval<container_type&>())) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return func(data_);
        } else {
            return func(data_);
        }
    }

private:
    container_type data_;
    Comparator comparator_{};

    // Thread safety mechanism
    mutable std::conditional_t<SafetyMode != ThreadSafetyMode::None,
                               std::shared_mutex, std::byte>
        mutex_;
};

// Implementation of SIMD search for integer keys
namespace detail {
#ifdef ATOM_SIMD_ENABLED
template <typename Key, typename Value>
std::pair<bool, size_t> simd_search_avx2(
    const std::vector<std::pair<Key, Value> >& data, const int key) {
    // Only implemented for int keys
    if constexpr (!std::is_same_v<Key, int>) {
        return {false, 0};
    }

    const size_t size = data.size();
    if (size < 8) {
        return {false, 0};  // Too small for SIMD
    }

    // Number of elements to process in each SIMD batch
    const size_t simd_width = 8;  // AVX2 can process 8 integers at once
    const size_t aligned_size = (size / simd_width) * simd_width;

    // Create a vector with 8 copies of the target key
    const __m256i key_vec = _mm256_set1_epi32(key);

    for (size_t i = 0; i < aligned_size; i += simd_width) {
        // Extract 8 key values from the data array
        // Note: This is not optimally aligned but will work
        int keys[simd_width];
        for (size_t j = 0; j < simd_width; ++j) {
            keys[j] = data[i + j].first;
        }

        // Load keys into SIMD register
        __m256i data_vec =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(keys));

        // Compare the keys with the target key
        __m256i cmp_result = _mm256_cmpeq_epi32(data_vec, key_vec);

        // Convert comparison results to a bitmask
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result));

        // If any of the comparisons were true
        if (mask != 0) {
            // Find the position of the first match
            unsigned long index;
            _BitScanForward(&index, mask);
            return {true, i + index};
        }
    }

    // Check remaining elements
    for (size_t i = aligned_size; i < size; ++i) {
        if (data[i].first == key) {
            return {true, i};
        }
    }

    return {false, 0};
}
#endif

template <typename Key, typename Value, typename Comparator>
std::pair<bool, size_t> simd_search(
    const std::vector<std::pair<Key, Value> >& data, const Key& key,
    Comparator& comp) {

#ifdef ATOM_SIMD_ENABLED
    if constexpr (std::is_same_v<Key, int>) {
        // Use SIMD search for integer keys if they use standard equality
        // comparison
        if constexpr (std::is_same_v<Comparator, std::equal_to<> > ||
                      std::is_same_v<Comparator, std::equal_to<Key> >) {
            return simd_search_avx2(data, key);
        }
    }
#endif

    // Fallback to scalar search
    for (size_t i = 0; i < data.size(); ++i) {
        if (comp(data[i].first, key)) {
            return {true, i};
        }
    }

    return {false, 0};
}
}  // namespace detail

#endif  // ATOM_USE_BOOST

}  // namespace atom::type

#endif  // ATOM_TYPE_FLATMAP_HPP
