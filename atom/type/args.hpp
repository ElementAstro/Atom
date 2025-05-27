/*
 * args.hpp - Enhanced Argument Container Library
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 */

#pragma once

#ifdef ATOM_THREAD_SAFE
#include <shared_mutex>
#define ATOM_LOCK_GUARD std::unique_lock<std::shared_mutex> lock(m_mutex_)
#define ATOM_SHARED_LOCK std::shared_lock<std::shared_mutex> lock(m_mutex_)
#else
#define ATOM_LOCK_GUARD
#define ATOM_SHARED_LOCK
#endif

#ifndef ATOM_TYPE_ARG_HPP
#define ATOM_TYPE_ARG_HPP

#include <concepts>
#include <functional>
#include <memory_resource>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#ifdef ATOM_USE_JSON
#include <nlohmann/json.hpp>
#endif

#ifdef ATOM_USE_BOOST
#include <boost/any.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/utility/string_view.hpp>
#else
#include <any>
#include <string_view>
#include <unordered_map>
#endif

// 设置参数的便捷宏
#define SET_ARGUMENT(container, name, value) container.set(#name, value)

// 获取参数的便捷宏
#define GET_ARGUMENT(container, name, type) container.get<type>(#name)

// 检查参数是否存在的便捷宏
#define HAS_ARGUMENT(container, name) container.contains(#name)

// 删除参数的便捷宏
#define REMOVE_ARGUMENT(container, name) container.remove(#name)

namespace atom {

#ifdef ATOM_USE_BOOST
using string_view_type = boost::string_view;
using any_type = boost::any;
template <typename K, typename V>
using map_type = boost::container::flat_map<K, V>;
#else
using string_view_type = std::string_view;
using any_type = std::any;
template <typename K, typename V>
using map_type = std::unordered_map<K, V>;
#endif

// 验证器类型
using Validator = std::function<bool(const any_type&)>;

/**
 * @brief A type-safe heterogeneous argument container
 *
 * @details The Args class provides a container for storing and retrieving
 * values of different types in a type-safe way. It supports both STL and
 * Boost implementations through conditional compilation.
 *
 * Features:
 * - Type-safe storage and retrieval
 * - Optional Boost integration
 * - Move-only semantics
 * - Exception safety
 * - Convenient macro interface
 * - Batch operations
 * - Type checking
 * - Thread-safe option
 * - Iteration support
 * - Validation support
 * - Memory pool optimization
 * - Range operations
 * - JSON serialization
 *
 * @note This class is move-only and cannot be copied
 */
class Args {
public:
    using iterator = typename map_type<string_view_type, any_type>::iterator;
    using const_iterator =
        typename map_type<string_view_type, any_type>::const_iterator;

    Args() : m_pool_(std::pmr::get_default_resource()) {}
    explicit Args(std::pmr::memory_resource* resource) : m_pool_(resource) {}
    ~Args() = default;

    Args(const Args&) = delete;
    Args& operator=(const Args&) = delete;
    Args(Args&&) = default;
    Args& operator=(Args&&) = default;

    /**
     * @brief Set a value for a given key with optional validation
     * @tparam T The type of the value to store
     * @param key The key to associate with the value
     * @param value The value to store
     * @throws std::runtime_error if value setting fails or validation fails
     */
    template <typename T>
        requires(!std::same_as<std::decay_t<T>, any_type>)
    void set(string_view_type key, T&& value) {
        ATOM_LOCK_GUARD;
        try {
            // 执行验证
            if (auto it = m_validators_.find(key); it != m_validators_.end()) {
                if (!it->second(value)) {
                    throw std::runtime_error("Validation failed for key: " +
                                             std::string(key));
                }
            }
            m_data_[key] = std::forward<T>(value);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to set value: " +
                                     std::string(e.what()));
        }
    }

    /**
     * @brief Set multiple key-value pairs at once (homogeneous types)
     * @tparam T The type of values to store
     * @param pairs A span of key-value pairs to store
     * @throws std::runtime_error if any value setting fails
     */
    template <typename T>
        requires(!std::same_as<std::decay_t<T>, any_type>)
    void set(std::span<const std::pair<string_view_type, T>> pairs) {
        ATOM_LOCK_GUARD;
        m_data_.reserve(m_data_.size() + pairs.size());
        for (const auto& [key, value] : pairs) {
            set(key, value);
        }
    }

    /**
     * @brief Set multiple key-value pairs at once (heterogeneous types)
     * @param pairs Initializer list of key-value pairs
     * @throws std::runtime_error if any value setting fails
     */
    void set(
        std::initializer_list<std::pair<string_view_type, any_type>> pairs) {
        ATOM_LOCK_GUARD;
        m_data_.reserve(m_data_.size() + pairs.size());
        for (const auto& [key, value] : pairs) {
            if (auto it = m_validators_.find(key); it != m_validators_.end()) {
                if (!it->second(value)) {
                    throw std::runtime_error("Validation failed for key: " +
                                             std::string(key));
                }
            }
            m_data_[key] = value;
        }
    }

    /**
     * @brief Set a validator for a key
     * @param key The key to validate
     * @param validator The validator function
     */
    void setValidator(string_view_type key, Validator validator) {
        ATOM_LOCK_GUARD;
        m_validators_[key] = std::move(validator);
    }

    /**
     * @brief Check if a value is of specific type
     * @tparam T The type to check against
     * @param key The key to check
     * @return true if value exists and is of type T, false otherwise
     */
    template <typename T>
    bool isType(string_view_type key) const noexcept {
        ATOM_SHARED_LOCK;
        if (auto it = m_data_.find(key); it != m_data_.end()) {
            try {
#ifdef ATOM_USE_BOOST
                return boost::any_cast<T>(&it->second) != nullptr;
#else
                return std::any_cast<T>(&it->second) != nullptr;
#endif
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    /**
     * @brief Get a value by key
     * @tparam T The type to retrieve
     * @param key The key to look up
     * @return The stored value
     * @throws std::out_of_range if key doesn't exist
     * @throws bad_any_cast if type doesn't match
     */
    template <typename T>
    auto get(string_view_type key) const -> T {
        ATOM_SHARED_LOCK;
#ifdef ATOM_USE_BOOST
        return boost::any_cast<T>(m_data_.at(key));
#else
        return std::any_cast<T>(m_data_.at(key));
#endif
    }

    /**
     * @brief Get a value by key with default
     * @tparam T The type to retrieve
     * @param key The key to look up
     * @param default_value The value to return if key not found
     * @return The stored value or default
     */
    template <typename T>
    auto getOr(string_view_type key, T&& default_value) const -> T {
        ATOM_SHARED_LOCK;
        if (auto it = m_data_.find(key); it != m_data_.end()) {
            try {
#ifdef ATOM_USE_BOOST
                return boost::any_cast<T>(it->second);
#else
                return std::any_cast<T>(it->second);
#endif
            } catch (...) {
                return std::forward<T>(default_value);
            }
        }
        return std::forward<T>(default_value);
    }

    /**
     * @brief Get an optional value by key
     * @tparam T The type to retrieve
     * @param key The key to look up
     * @return Optional containing the value if found and type matches
     */
    template <typename T>
    auto getOptional(string_view_type key) const -> std::optional<T> {
        ATOM_SHARED_LOCK;
        if (auto it = m_data_.find(key); it != m_data_.end()) {
            try {
#ifdef ATOM_USE_BOOST
                return boost::any_cast<T>(it->second);
#else
                return std::any_cast<T>(it->second);
#endif
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get multiple values by keys
     * @tparam T The type to retrieve
     * @param keys Span of keys to look up
     * @return Vector of optional values
     */
    template <typename T>
    auto get(std::span<const string_view_type> keys) const
        -> std::vector<std::optional<T>> {
        std::vector<std::optional<T>> values;
        values.reserve(keys.size());
        for (const auto& key : keys) {
            values.push_back(getOptional<T>(key));
        }
        return values;
    }

    /**
     * @brief Check if a key exists
     * @param key The key to check
     * @return true if key exists, false otherwise
     */
    auto contains(string_view_type key) const noexcept -> bool {
        ATOM_SHARED_LOCK;
        return m_data_.contains(key);
    }

    /**
     * @brief Remove a key-value pair
     * @param key The key to remove
     */
    void remove(string_view_type key) {
        ATOM_LOCK_GUARD;
        m_data_.erase(key);
        m_validators_.erase(key);
    }

    /**
     * @brief Remove all key-value pairs
     */
    void clear() noexcept {
        ATOM_LOCK_GUARD;
        m_data_.clear();
        m_validators_.clear();
    }

    /**
     * @brief Get number of stored items
     * @return Number of key-value pairs
     */
    auto size() const noexcept -> size_t {
        ATOM_SHARED_LOCK;
        return m_data_.size();
    }

    /**
     * @brief Check if container is empty
     * @return true if empty, false otherwise
     */
    auto empty() const noexcept -> bool {
        ATOM_SHARED_LOCK;
        return m_data_.empty();
    }

    /**
     * @brief Access value by key with type
     * @tparam T The type to retrieve
     * @param key The key to look up
     * @return Reference to stored value
     * @throws bad_any_cast if type doesn't match
     */
    template <typename T>
    auto operator[](string_view_type key) -> T& {
#ifdef ATOM_USE_BOOST
        return boost::any_cast<T&>(m_data_[key]);
#else
        return std::any_cast<T&>(m_data_[key]);
#endif
    }

    /**
     * @brief Access const value by key with type
     * @tparam T The type to retrieve
     * @param key The key to look up
     * @return Const reference to stored value
     * @throws std::out_of_range if key doesn't exist
     * @throws bad_any_cast if type doesn't match
     */
    template <typename T>
    auto operator[](string_view_type key) const -> const T& {
        ATOM_SHARED_LOCK;
#ifdef ATOM_USE_BOOST
        return boost::any_cast<const T&>(m_data_.at(key));
#else
        return std::any_cast<const T&>(m_data_.at(key));
#endif
    }

    /**
     * @brief Access underlying any object
     * @param key The key to look up
     * @return Reference to any object
     */
    auto operator[](string_view_type key) -> any_type& { return m_data_[key]; }

    /**
     * @brief Access const underlying any object
     * @param key The key to look up
     * @return Const reference to any object
     * @throws std::out_of_range if key doesn't exist
     */
    auto operator[](string_view_type key) const -> const any_type& {
        return m_data_.at(key);
    }

#ifdef ATOM_USE_JSON
    /**
     * @brief Convert to JSON
     * @return JSON object
     */
    nlohmann::json toJson() const {
        ATOM_SHARED_LOCK;
        nlohmann::json j;
        for (const auto& [key, value] : m_data_) {
            j[std::string(key)] = value;
        }
        return j;
    }

    /**
     * @brief Load from JSON
     * @param j JSON object
     */
    void fromJson(const nlohmann::json& j) {
        ATOM_LOCK_GUARD;
        m_data_.clear();
        for (auto it = j.begin(); it != j.end(); ++it) {
            m_data_[it.key()] = it.value();
        }
    }
#endif

#ifdef ATOM_USE_BOOST
    /**
     * @brief Serialize the container
     * @tparam Archive The archive type
     * @param ar The archive object
     * @param version The version number
     */
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & m_data_;
    }
#endif

    /**
     * @brief Apply a function to each key-value pair
     * @param f Function to apply
     */
    template <typename F>
    void forEach(F&& f) const {
        ATOM_SHARED_LOCK;
        for (const auto& [key, value] : m_data_) {
            f(key, value);
        }
    }

    /**
     * @brief Transform values and store results
     * @tparam F Transform function type
     * @param f Transform function
     * @return New Args object with transformed values
     */
    template <typename F>
    Args transform(F&& f) const {
        ATOM_SHARED_LOCK;
        Args result;
        for (const auto& [key, value] : m_data_) {
            result.m_data_[key] = f(value);
        }
        return result;
    }

    /**
     * @brief Filter key-value pairs
     * @tparam F Predicate function type
     * @param pred Predicate function
     * @return New Args object with filtered pairs
     */
    template <typename F>
    Args filter(F&& pred) const {
        ATOM_SHARED_LOCK;
        Args result;
        for (const auto& [key, value] : m_data_) {
            if (pred(key, value)) {
                result.m_data_[key] = value;
            }
        }
        return result;
    }

    /**
     * @brief Get begin iterator
     * @return Iterator to the beginning
     */
    iterator begin() noexcept { return m_data_.begin(); }
    const_iterator begin() const noexcept { return m_data_.begin(); }
    const_iterator cbegin() const noexcept { return m_data_.cbegin(); }

    /**
     * @brief Get end iterator
     * @return Iterator to the end
     */
    iterator end() noexcept { return m_data_.end(); }
    const_iterator end() const noexcept { return m_data_.end(); }
    const_iterator cend() const noexcept { return m_data_.cend(); }

    /**
     * @brief Get all items as key-value pairs
     * @return Vector of key-value pairs
     */
    auto items() const -> std::vector<std::pair<string_view_type, any_type>> {
        ATOM_SHARED_LOCK;
        return {m_data_.begin(), m_data_.end()};
    }

private:
#ifdef ATOM_THREAD_SAFE
    mutable std::shared_mutex m_mutex_;
#endif
    std::pmr::memory_resource* m_pool_;
    map_type<string_view_type, any_type> m_data_;
    map_type<string_view_type, Validator> m_validators_;
};

}  // namespace atom

#endif  // ATOM_TYPE_ARG_HPP