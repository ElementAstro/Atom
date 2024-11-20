/*
 * args.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-28

Description: Argument Container Library for C++

**************************************************/

#ifndef ATOM_TYPE_ARG_HPP
#define ATOM_TYPE_ARG_HPP

#include <concepts>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

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
#define GET_ARGUMENT(container, name, type) \
    container.get<type>(#name).value_or(type{})

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
 *
 * @note This class is move-only and cannot be copied
 *
 * Example usage:
 * @code
 * atom::Args args;
 * args.set("name", "test");
 * args.set("count", 42);
 *
 * auto name = args.get<std::string>("name");
 * auto count = args.getOr("count", 0);
 * @endcode
 */
class Args {
public:
    Args() = default;
    ~Args() = default;

    // 禁止拷贝,允许移动
    Args(const Args&) = delete;
    Args& operator=(const Args&) = delete;
    Args(Args&&) = default;
    Args& operator=(Args&&) = default;

    /**
     * @brief Set a value for a given key
     * @tparam T The type of the value to store
     * @param key The key to associate with the value
     * @param value The value to store
     * @throws std::runtime_error if value setting fails
     */
    template <typename T>
        requires(!std::same_as<std::decay_t<T>, any_type>)
    void set(string_view_type key, T&& value) {
        try {
            m_data_[key] = std::forward<T>(value);
        } catch (const std::exception& e) {
            // 处理异常
            throw std::runtime_error("Failed to set value: " +
                                     std::string(e.what()));
        }
    }

    /**
     * @brief Set multiple key-value pairs at once
     * @tparam T The type of values to store
     * @param pairs A span of key-value pairs to store
     */
    template <typename T>
        requires(!std::same_as<std::decay_t<T>, any_type>)
    void set(std::span<const std::pair<string_view_type, T>> pairs) {
        m_data_.reserve(m_data_.size() + pairs.size());  // 预分配空间
        for (const auto& [key, value] : pairs) {
            set(key, value);
        }
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
        if (auto it = m_data_.find(key); it != m_data_.end()) {
#ifdef ATOM_USE_BOOST
            return boost::any_cast<T>(it->second);
#else
            return std::any_cast<T>(it->second);
#endif
        }
        return std::forward<T>(default_value);
    }

    /**
     * @brief Get multiple values by keys
     * @tparam T The type to retrieve
     * @param keys Span of keys to look up
     * @return Vector of optional values
     */
    template <typename T>
    auto getOptional(string_view_type key) const -> std::optional<T> {
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
        return m_data_.contains(key);
    }

    /**
     * @brief Remove a key-value pair
     * @param key The key to remove
     */
    void remove(string_view_type key) { m_data_.erase(key); }

    /** @brief Remove all key-value pairs */
    void clear() noexcept { m_data_.clear(); }

    /**
     * @brief Get number of stored items
     * @return Number of key-value pairs
     */
    auto size() const noexcept -> size_t { return m_data_.size(); }

    /**
     * @brief Check if container is empty
     * @return true if empty, false otherwise
     */
    auto empty() const noexcept -> bool { return m_data_.empty(); }

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

private:
    map_type<string_view_type, any_type> m_data_;
};

}  // namespace atom

#endif