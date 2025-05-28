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

#include <algorithm>
#include <concepts>
#include <execution>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
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
#endif

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
}  // namespace atom::type::exceptions

namespace atom::type {

constexpr std::size_t DEFAULT_INITIAL_CAPACITY = 16;
constexpr std::size_t MAX_CONTAINER_SIZE =
    std::numeric_limits<std::size_t>::max() / 2;
constexpr std::size_t PARALLEL_THRESHOLD = 10000;

enum class ThreadSafetyMode { None, ReadOnly, ReadWrite };

namespace detail {
#ifdef ATOM_SIMD_ENABLED
template <typename Key, typename Value>
std::pair<bool, size_t> simd_search_avx2(
    const std::vector<std::pair<Key, Value>>& data, const Key& key) {
    if constexpr (!std::is_same_v<Key, int>) {
        return {false, 0};
    }

    const size_t size = data.size();
    if (size < 8)
        return {false, 0};

    const size_t simd_width = 8;
    const size_t aligned_size = (size / simd_width) * simd_width;
    const __m256i key_vec = _mm256_set1_epi32(key);

    for (size_t i = 0; i < aligned_size; i += simd_width) {
        alignas(32) int keys[simd_width];
        for (size_t j = 0; j < simd_width; ++j) {
            keys[j] = data[i + j].first;
        }

        __m256i data_vec =
            _mm256_load_si256(reinterpret_cast<const __m256i*>(keys));
        __m256i cmp_result = _mm256_cmpeq_epi32(data_vec, key_vec);
        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result));

        if (mask != 0) {
            return {true, i + __builtin_ctz(mask)};
        }
    }

    for (size_t i = aligned_size; i < size; ++i) {
        if (data[i].first == key) {
            return {true, i};
        }
    }

    return {false, 0};
}
#endif

template <typename Container, typename Key>
auto find_element(Container& container, const Key& key) {
    if constexpr (requires { container.find(key); }) {
        return container.find(key);
    } else {
        return std::ranges::find_if(
            container, [&key](const auto& pair) { return pair.first == key; });
    }
}
}  // namespace detail

/**
 * @brief A high-performance flat map implementation with optional thread
 * safety.
 *
 * @tparam Key The key type.
 * @tparam Value The value type.
 * @tparam Compare The comparison function type.
 * @tparam SafetyMode Thread safety mode.
 */
template <typename Key, typename Value, typename Compare = std::less<Key>,
          ThreadSafetyMode SafetyMode = ThreadSafetyMode::None>
    requires std::predicate<Compare, Key, Key>
class FlatMap {
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<Key, Value>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

#ifdef ATOM_USE_BOOST
    using container_type = boost::container::flat_map<Key, Value, Compare>;
#else
    using container_type = std::vector<value_type>;
#endif

    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

private:
    container_type data_;
    [[no_unique_address]] Compare comp_;
    mutable std::conditional_t<SafetyMode != ThreadSafetyMode::None,
                               std::shared_mutex, std::byte>
        mutex_;

    void ensure_capacity(size_type min_capacity) {
        if constexpr (requires { data_.reserve(min_capacity); }) {
            if (data_.capacity() < min_capacity) {
                auto new_cap =
                    std::max(min_capacity,
                             static_cast<size_type>(data_.capacity() * 1.5));
                if (new_cap > MAX_CONTAINER_SIZE) {
                    throw exceptions::container_full_error(
                        "Capacity exceeds maximum");
                }
                data_.reserve(new_cap);
            }
        }
    }

    template <typename K>
    auto find_impl(const K& key) -> iterator {
#ifdef ATOM_USE_BOOST
        return data_.find(key);
#else
        if (data_.size() > PARALLEL_THRESHOLD) {
            auto it = std::lower_bound(std::execution::par_unseq, data_.begin(),
                                       data_.end(), key,
                                       [this](const auto& pair, const auto& k) {
                                           return comp_(pair.first, k);
                                       });
            return (it != data_.end() && !comp_(key, it->first) &&
                    !comp_(it->first, key))
                       ? it
                       : data_.end();
        }

#ifdef ATOM_SIMD_ENABLED
        if constexpr (std::is_same_v<Key, int> && std::is_same_v<K, int>) {
            auto [found, index] = detail::simd_search_avx2(data_, key);
            return found ? data_.begin() + index : data_.end();
        }
#endif
        return std::ranges::find_if(
            data_, [&key](const auto& pair) { return pair.first == key; });
#endif
    }

    template <typename K>
    auto find_impl(const K& key) const -> const_iterator {
#ifdef ATOM_USE_BOOST
        return data_.find(key);
#else
        if (data_.size() > PARALLEL_THRESHOLD) {
            auto it = std::lower_bound(std::execution::par_unseq, data_.begin(),
                                       data_.end(), key,
                                       [this](const auto& pair, const auto& k) {
                                           return comp_(pair.first, k);
                                       });
            return (it != data_.end() && !comp_(key, it->first) &&
                    !comp_(it->first, key))
                       ? it
                       : data_.end();
        }

#ifdef ATOM_SIMD_ENABLED
        if constexpr (std::is_same_v<Key, int> && std::is_same_v<K, int>) {
            auto [found, index] = detail::simd_search_avx2(data_, key);
            return found ? data_.begin() + index : data_.end();
        }
#endif
        return std::ranges::find_if(
            data_, [&key](const auto& pair) { return pair.first == key; });
#endif
    }

public:
    /**
     * @brief Default constructor.
     */
    FlatMap() {
        if constexpr (requires { data_.reserve(DEFAULT_INITIAL_CAPACITY); }) {
            try {
                data_.reserve(DEFAULT_INITIAL_CAPACITY);
            } catch (...) {
            }
        }
    }

    /**
     * @brief Constructor with initial capacity.
     *
     * @param initial_capacity The initial capacity to reserve.
     */
    explicit FlatMap(size_type initial_capacity) {
        if (initial_capacity > MAX_CONTAINER_SIZE) {
            throw exceptions::container_full_error(
                "Initial capacity exceeds maximum");
        }
        if constexpr (requires { data_.reserve(initial_capacity); }) {
            data_.reserve(initial_capacity);
        }
    }

    /**
     * @brief Constructor with custom comparator.
     *
     * @param comp The comparator to use.
     */
    explicit FlatMap(const Compare& comp) : comp_(comp) {
        if constexpr (requires { data_.reserve(DEFAULT_INITIAL_CAPACITY); }) {
            try {
                data_.reserve(DEFAULT_INITIAL_CAPACITY);
            } catch (...) {
            }
        }
    }

    /**
     * @brief Constructor from range.
     *
     * @tparam InputIt The input iterator type.
     * @param first The beginning of the range.
     * @param last The end of the range.
     */
    template <std::input_iterator InputIt>
    FlatMap(InputIt first, InputIt last) {
        assign(first, last);
    }

    /**
     * @brief Constructor from initializer list.
     *
     * @param init The initializer list.
     */
    FlatMap(std::initializer_list<value_type> init)
        : FlatMap(init.begin(), init.end()) {}

    FlatMap(const FlatMap& other) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(other.mutex_);
            data_ = other.data_;
            comp_ = other.comp_;
        } else {
            data_ = other.data_;
            comp_ = other.comp_;
        }
    }

    FlatMap(FlatMap&& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(other.mutex_);
            data_ = std::move(other.data_);
            comp_ = std::move(other.comp_);
        } else {
            data_ = std::move(other.data_);
            comp_ = std::move(other.comp_);
        }
    }

    FlatMap& operator=(const FlatMap& other) {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::shared_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = other.data_;
                comp_ = other.comp_;
            } else {
                data_ = other.data_;
                comp_ = other.comp_;
            }
        }
        return *this;
    }

    FlatMap& operator=(FlatMap&& other) noexcept {
        if (this != &other) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::unique_lock lock(mutex_, std::defer_lock);
                std::unique_lock other_lock(other.mutex_, std::defer_lock);
                std::lock(lock, other_lock);
                data_ = std::move(other.data_);
                comp_ = std::move(other.comp_);
            } else {
                data_ = std::move(other.data_);
                comp_ = std::move(other.comp_);
            }
        }
        return *this;
    }

    /**
     * @brief Returns an iterator to the beginning.
     */
    iterator begin() noexcept { return data_.begin(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator cbegin() const noexcept { return data_.begin(); }

    /**
     * @brief Returns an iterator to the end.
     */
    iterator end() noexcept { return data_.end(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cend() const noexcept { return data_.end(); }

    /**
     * @brief Checks if the map is empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.empty();
        } else {
            return data_.empty();
        }
    }

    /**
     * @brief Returns the number of elements.
     */
    [[nodiscard]] size_type size() const noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return data_.size();
        } else {
            return data_.size();
        }
    }

    /**
     * @brief Returns the maximum possible number of elements.
     */
    [[nodiscard]] size_type max_size() const noexcept {
        return MAX_CONTAINER_SIZE;
    }

    /**
     * @brief Returns the current capacity.
     */
    [[nodiscard]] size_type capacity() const noexcept {
        if constexpr (requires { data_.capacity(); }) {
            if constexpr (SafetyMode != ThreadSafetyMode::None) {
                std::shared_lock lock(mutex_);
                return data_.capacity();
            } else {
                return data_.capacity();
            }
        } else {
            return size();
        }
    }

    /**
     * @brief Reserves storage for at least the specified number of elements.
     *
     * @param new_cap The new capacity.
     */
    void reserve(size_type new_cap) {
        if (new_cap > MAX_CONTAINER_SIZE) {
            throw exceptions::container_full_error(
                "Requested capacity exceeds maximum");
        }
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            if constexpr (requires { data_.reserve(new_cap); }) {
                data_.reserve(new_cap);
            }
        } else {
            if constexpr (requires { data_.reserve(new_cap); }) {
                data_.reserve(new_cap);
            }
        }
    }

    /**
     * @brief Clears the map.
     */
    void clear() noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
        } else {
            data_.clear();
        }
    }

    /**
     * @brief Finds an element with the specified key.
     *
     * @param key The key to search for.
     * @return An iterator to the element, or end() if not found.
     */
    template <typename K>
    iterator find(const K& key) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(key);
        } else {
            return find_impl(key);
        }
    }

    template <typename K>
    const_iterator find(const K& key) const {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::shared_lock lock(mutex_);
            return find_impl(key);
        } else {
            return find_impl(key);
        }
    }

    /**
     * @brief Checks if the map contains the specified key.
     *
     * @param key The key to check.
     * @return True if the key exists, false otherwise.
     */
    template <typename K>
    bool contains(const K& key) const {
        return find(key) != end();
    }

    /**
     * @brief Returns the number of elements with the specified key.
     *
     * @param key The key to count.
     * @return The number of elements (0 or 1 for a map).
     */
    template <typename K>
    size_type count(const K& key) const {
        return contains(key) ? 1 : 0;
    }

    /**
     * @brief Accesses or inserts an element with the specified key.
     *
     * @param key The key to access.
     * @return A reference to the mapped value.
     */
    Value& operator[](const Key& key) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            auto it = find_impl(key);
            if (it != data_.end()) {
                return it->second;
            }
            ensure_capacity(data_.size() + 1);
#ifdef ATOM_USE_BOOST
            return data_[key];
#else
            return data_.emplace_back(key, Value{}).second;
#endif
        } else {
            auto it = find_impl(key);
            if (it != data_.end()) {
                return it->second;
            }
            ensure_capacity(data_.size() + 1);
#ifdef ATOM_USE_BOOST
            return data_[key];
#else
            return data_.emplace_back(key, Value{}).second;
#endif
        }
    }

    /**
     * @brief Accesses an element with bounds checking.
     *
     * @param key The key to access.
     * @return A reference to the mapped value.
     * @throws key_not_found_error if the key is not found.
     */
    Value& at(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            throw exceptions::key_not_found_error("Key not found in map");
        }
        return it->second;
    }

    const Value& at(const Key& key) const {
        auto it = find(key);
        if (it == end()) {
            throw exceptions::key_not_found_error("Key not found in map");
        }
        return it->second;
    }

    /**
     * @brief Safely retrieves a value without throwing.
     *
     * @param key The key to search for.
     * @return An optional containing the value if found.
     */
    std::optional<Value> try_get(const Key& key) const {
        auto it = find(key);
        return it != end() ? std::make_optional(it->second) : std::nullopt;
    }

    /**
     * @brief Inserts an element.
     *
     * @param value The key-value pair to insert.
     * @return A pair of an iterator and a boolean indicating success.
     */
    std::pair<iterator, bool> insert(const value_type& value) {
        return insert_or_assign(value.first, value.second);
    }

    std::pair<iterator, bool> insert(value_type&& value) {
        return insert_or_assign(std::move(value.first),
                                std::move(value.second));
    }

    /**
     * @brief Inserts or assigns a value.
     *
     * @tparam M The type of the mapped value.
     * @param key The key to insert or update.
     * @param value The value to assign.
     * @return A pair of an iterator and a boolean indicating insertion.
     */
    template <typename M>
    std::pair<iterator, bool> insert_or_assign(const Key& key, M&& value) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            auto it = find_impl(key);
            if (it != data_.end()) {
                it->second = std::forward<M>(value);
                return {it, false};
            }
            ensure_capacity(data_.size() + 1);
#ifdef ATOM_USE_BOOST
            return data_.insert_or_assign(key, std::forward<M>(value));
#else
            auto pos = data_.emplace(data_.end(), key, std::forward<M>(value));
            return {pos, true};
#endif
        } else {
            auto it = find_impl(key);
            if (it != data_.end()) {
                it->second = std::forward<M>(value);
                return {it, false};
            }
            ensure_capacity(data_.size() + 1);
#ifdef ATOM_USE_BOOST
            return data_.insert_or_assign(key, std::forward<M>(value));
#else
            auto pos = data_.emplace(data_.end(), key, std::forward<M>(value));
            return {pos, true};
#endif
        }
    }

    /**
     * @brief Constructs and inserts an element in-place.
     *
     * @tparam Args The argument types.
     * @param args The arguments to construct the element.
     * @return A pair of an iterator and a boolean indicating success.
     */
    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }

    /**
     * @brief Erases an element by iterator.
     *
     * @param pos The iterator to the element to erase.
     * @return An iterator to the element following the erased element.
     */
    iterator erase(const_iterator pos) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return data_.erase(pos);
        } else {
            return data_.erase(pos);
        }
    }

    /**
     * @brief Erases elements in a range.
     *
     * @param first The beginning of the range.
     * @param last The end of the range.
     * @return An iterator to the element following the erased range.
     */
    iterator erase(const_iterator first, const_iterator last) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            return data_.erase(first, last);
        } else {
            return data_.erase(first, last);
        }
    }

    /**
     * @brief Erases an element by key.
     *
     * @param key The key of the element to erase.
     * @return The number of elements erased (0 or 1).
     */
    size_type erase(const Key& key) {
        auto it = find(key);
        if (it != end()) {
            erase(it);
            return 1;
        }
        return 0;
    }

    /**
     * @brief Assigns elements from a range.
     *
     * @tparam InputIt The input iterator type.
     * @param first The beginning of the range.
     * @param last The end of the range.
     */
    template <std::input_iterator InputIt>
    void assign(InputIt first, InputIt last) {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_);
            data_.clear();
            if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
                auto count = std::distance(first, last);
                if (count > 0 &&
                    static_cast<size_type>(count) <= MAX_CONTAINER_SIZE) {
                    if constexpr (requires { data_.reserve(count); }) {
                        data_.reserve(count);
                    }
                }
            }
#ifdef ATOM_USE_BOOST
            data_.insert(first, last);
#else
            data_.assign(first, last);
#endif
        } else {
            data_.clear();
            if constexpr (std::sized_sentinel_for<InputIt, InputIt>) {
                auto count = std::distance(first, last);
                if (count > 0 &&
                    static_cast<size_type>(count) <= MAX_CONTAINER_SIZE) {
                    if constexpr (requires { data_.reserve(count); }) {
                        data_.reserve(count);
                    }
                }
            }
#ifdef ATOM_USE_BOOST
            data_.insert(first, last);
#else
            data_.assign(first, last);
#endif
        }
    }

    /**
     * @brief Swaps the contents with another map.
     *
     * @param other The other map to swap with.
     */
    void swap(FlatMap& other) noexcept {
        if constexpr (SafetyMode != ThreadSafetyMode::None) {
            std::unique_lock lock(mutex_, std::defer_lock);
            std::unique_lock other_lock(other.mutex_, std::defer_lock);
            std::lock(lock, other_lock);
            std::swap(data_, other.data_);
            std::swap(comp_, other.comp_);
        } else {
            std::swap(data_, other.data_);
            std::swap(comp_, other.comp_);
        }
    }

    /**
     * @brief Returns the key comparison object.
     */
    key_compare key_comp() const { return comp_; }

    /**
     * @brief Executes a function with read lock (thread-safe version only).
     *
     * @tparam F The function type.
     * @param func The function to execute.
     * @return The result of the function.
     */
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

    /**
     * @brief Executes a function with write lock (thread-safe version only).
     *
     * @tparam F The function type.
     * @param func The function to execute.
     * @return The result of the function.
     */
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
};

/**
 * @brief Equality comparison operator.
 */
template <typename Key, typename Value, typename Compare,
          ThreadSafetyMode SafetyMode>
bool operator==(const FlatMap<Key, Value, Compare, SafetyMode>& lhs,
                const FlatMap<Key, Value, Compare, SafetyMode>& rhs) {
    return lhs.size() == rhs.size() &&
           std::ranges::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
 * @brief Swaps two FlatMaps.
 */
template <typename Key, typename Value, typename Compare,
          ThreadSafetyMode SafetyMode>
void swap(FlatMap<Key, Value, Compare, SafetyMode>& lhs,
          FlatMap<Key, Value, Compare, SafetyMode>& rhs) noexcept {
    lhs.swap(rhs);
}

}  // namespace atom::type

#endif  // ATOM_TYPE_FLATMAP_HPP
