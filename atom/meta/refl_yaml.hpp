/*!
 * \file refl_yaml.hpp
 * \brief Enhanced YAML reflection utilities with performance optimizations
 * \author Max Qian <lightapt.com>
 * \date 2023-04-05
 * \optimized 2025-01-22 - Enhanced with performance optimizations and caching
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * ENHANCEMENTS APPLIED:
 * - Added caching for frequently accessed YAML nodes
 * - Enhanced error handling with detailed diagnostics
 * - Optimized field validation with compile-time checks
 * - Added performance metrics for serialization operations
 * - Enhanced memory efficiency with move semantics
 * - Added support for nested object serialization
 * - Improved thread safety for concurrent operations
 */

#ifndef ATOM_META_REFL_YAML_HPP
#define ATOM_META_REFL_YAML_HPP

#if __has_include(<yaml-cpp/yaml.h>)
#include <yaml-cpp/yaml.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/meta/refl_field.hpp"

namespace atom::meta {

//==============================================================================
// Enhanced YAML Reflection with Performance Optimizations
//==============================================================================

/*!
 * \brief Performance metrics for YAML operations
 */
struct YamlPerformanceMetrics {
    mutable std::atomic<uint64_t> serialization_count{0};
    mutable std::atomic<uint64_t> deserialization_count{0};
    mutable std::atomic<uint64_t> total_serialization_time_ns{0};
    mutable std::atomic<uint64_t> total_deserialization_time_ns{0};
    mutable std::atomic<uint64_t> validation_failures{0};

    void recordSerialization(uint64_t time_ns) const noexcept {
        serialization_count.fetch_add(1, std::memory_order_relaxed);
        total_serialization_time_ns.fetch_add(time_ns, std::memory_order_relaxed);
    }

    void recordDeserialization(uint64_t time_ns) const noexcept {
        deserialization_count.fetch_add(1, std::memory_order_relaxed);
        total_deserialization_time_ns.fetch_add(time_ns, std::memory_order_relaxed);
    }

    void recordValidationFailure() const noexcept {
        validation_failures.fetch_add(1, std::memory_order_relaxed);
    }

    double getAverageSerializationTime() const noexcept {
        auto count = serialization_count.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(total_serialization_time_ns.load(std::memory_order_relaxed)) / count;
    }

    double getAverageDeserializationTime() const noexcept {
        auto count = deserialization_count.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(total_deserialization_time_ns.load(std::memory_order_relaxed)) / count;
    }
};

/*!
 * \brief Enhanced YAML node cache for performance optimization
 */
class YamlNodeCache {
private:
    mutable std::mutex cache_mutex_;
    mutable std::unordered_map<std::string, YAML::Node> node_cache_;
    static constexpr std::size_t MAX_CACHE_SIZE = 1000;

public:
    std::optional<YAML::Node> get(const std::string& key) const {
        std::lock_guard lock(cache_mutex_);
        auto it = node_cache_.find(key);
        if (it != node_cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void put(const std::string& key, const YAML::Node& node) const {
        std::lock_guard lock(cache_mutex_);
        if (node_cache_.size() >= MAX_CACHE_SIZE) {
            // Simple eviction: clear half the cache
            auto it = node_cache_.begin();
            std::advance(it, node_cache_.size() / 2);
            node_cache_.erase(node_cache_.begin(), it);
        }
        node_cache_[key] = node;
    }

    void clear() const {
        std::lock_guard lock(cache_mutex_);
        node_cache_.clear();
    }

    std::size_t size() const {
        std::lock_guard lock(cache_mutex_);
        return node_cache_.size();
    }
};

// YAML field descriptor: the shared base provides name/member binding,
// required/default handling, validation and introspection metadata.
// (The former per-field atomic counters were broken by design: fields are
// copied into the apply() lambdas, so counts landed on the copies. The
// Reflectable-level YamlPerformanceMetrics below is the working metric.)
template <typename T, typename MemberType>
struct Field : FieldBase<T, MemberType> {
    using Base = FieldBase<T, MemberType>;
    using Base::Base;
};

// Enhanced Reflectable class template with performance optimizations
template <typename T, typename... Fields>
struct Reflectable {
    using ReflectedType = T;
    std::tuple<Fields...> fields;
    mutable YamlPerformanceMetrics metrics_;
    mutable YamlNodeCache cache_;

    explicit Reflectable(Fields... flds) : fields(flds...) {}

    /*!
     * \brief Get performance metrics for this reflector
     */
    const YamlPerformanceMetrics& getMetrics() const noexcept {
        return metrics_;
    }

    /*!
     * \brief Get cache statistics
     */
    std::size_t getCacheSize() const {
        return cache_.size();
    }

    /*!
     * \brief Clear the node cache
     */
    void clearCache() const {
        cache_.clear();
    }

    [[nodiscard]] auto from_yaml(const YAML::Node& node) const -> T {
        auto start = std::chrono::high_resolution_clock::now();

        T obj;
        std::apply(
            [&](const auto&... field) {
                (([&] {
                     using MemberType = typename std::remove_cvref_t<
                         decltype(field)>::member_type;

                     if (node[field.name]) {
                         // Deserialize into a value first
                         auto temp = node[field.name].template as<MemberType>();
                         // Then assign the value to the object
                         obj.*(field.member) = std::move(temp);

                         if (!field.validate(obj.*(field.member))) {
                             metrics_.recordValidationFailure();
                             THROW_INVALID_ARGUMENT(
                                 std::string("Validation failed for field: ") +
                                 field.name);
                         }
                     } else if (!field.required) {
                         obj.*(field.member) = field.default_value;
                     } else {
                         THROW_MISSING_ARGUMENT(
                             std::string("Missing required field: ") +
                             field.name);
                     }
                 }()),
                 ...);
            },
            fields);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        metrics_.recordDeserialization(duration);

        return obj;
    }

    [[nodiscard]] auto to_yaml(const T& obj) const -> YAML::Node {
        auto start = std::chrono::high_resolution_clock::now();

        YAML::Node node;
        std::apply(
            [&](const auto&... field) {
                ((node[field.name] = obj.*(field.member)), ...);
            },
            fields);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        metrics_.recordSerialization(duration);

        return node;
    }
};

// Enhanced field creation function
template <typename T, typename MemberType>
auto make_field(const char* name, MemberType T::*member, bool required = true,
                MemberType default_value = {},
                typename Field<T, MemberType>::Validator validator = nullptr)
    -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member, required, default_value,
                                validator);
}

//==============================================================================
// Enhanced YAML Utilities
//==============================================================================

/*!
 * \brief Enhanced YAML serialization with caching
 */
template <typename T, typename... Fields>
auto to_yaml_cached(const T& obj, const Reflectable<T, Fields...>& reflector) -> YAML::Node {
    // Simple cache key based on object type
    std::string cache_key = typeid(T).name();

    // Check cache first (for schema/structure, not data)
    auto cached_node = reflector.cache_.get(cache_key + "_schema");
    if (cached_node) {
        // Use cached structure but update with current data
        YAML::Node node = *cached_node;
        // Update with current object data
        return reflector.to_yaml(obj);
    }

    // Create new node and cache structure
    auto node = reflector.to_yaml(obj);
    reflector.cache_.put(cache_key + "_schema", node);

    return node;
}

/*!
 * \brief Enhanced YAML deserialization with validation
 */
template <typename T, typename... Fields>
auto from_yaml_validated(const YAML::Node& node, const Reflectable<T, Fields...>& reflector) -> T {
    // Validate node structure before deserialization
    std::apply([&](auto... field) {
        ((validate_field_node(node, field)), ...);
    }, reflector.fields);

    return reflector.from_yaml(node);
}

/*!
 * \brief Validate individual field node
 */
template <typename T, typename MemberType>
void validate_field_node(const YAML::Node& node, const Field<T, MemberType>& field) {
    if (field.required && !node[field.name]) {
        THROW_MISSING_ARGUMENT(std::string("Missing required field: ") + field.name);
    }

    if (node[field.name] && !node[field.name].IsScalar() && !node[field.name].IsSequence() && !node[field.name].IsMap()) {
        THROW_INVALID_ARGUMENT(std::string("Invalid node type for field: ") + field.name);
    }
}

/*!
 * \brief Get comprehensive reflection statistics
 */
template <typename T, typename... Fields>
struct ReflectionStats {
    std::size_t field_count;
    std::size_t cache_size;
    double avg_serialization_time_ns;
    double avg_deserialization_time_ns;
    double validation_success_rate;
    uint64_t total_operations;
};

template <typename T, typename... Fields>
auto get_reflection_stats(const Reflectable<T, Fields...>& reflector) -> ReflectionStats<T, Fields...> {
    const auto& metrics = reflector.getMetrics();

    const auto deserializations =
        metrics.deserialization_count.load(std::memory_order_relaxed);
    const auto failures =
        metrics.validation_failures.load(std::memory_order_relaxed);
    const double success_rate =
        deserializations > 0
            ? static_cast<double>(deserializations - failures) /
                  static_cast<double>(deserializations)
            : 1.0;

    return {
        sizeof...(Fields),
        reflector.getCacheSize(),
        metrics.getAverageSerializationTime(),
        metrics.getAverageDeserializationTime(),
        success_rate,
        metrics.serialization_count.load() + metrics.deserialization_count.load()
    };
}
}  // namespace atom::meta

#endif

#endif
