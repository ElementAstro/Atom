#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_COLLECTION_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_COLLECTION_HPP

#include <algorithm>
#include <functional>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "weight_common.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Thread-safe collection of weighted items with lookup and sampling
 */
template <WeightType T>
class WeightCollection {
public:
    WeightCollection() = default;

    // Move constructor - mutex cannot be moved, so we create a new one
    WeightCollection(WeightCollection&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        weights_ = std::move(other.weights_);
        random_engine_ = std::move(other.random_engine_);
    }

    // Move assignment operator
    WeightCollection& operator=(WeightCollection&& other) noexcept {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            weights_ = std::move(other.weights_);
            random_engine_ = std::move(other.random_engine_);
        }
        return *this;
    }

    // Delete copy operations since mutex is not copyable
    WeightCollection(const WeightCollection&) = delete;
    WeightCollection& operator=(const WeightCollection&) = delete;

    auto size() const -> usize {
        std::shared_lock lock(mutex_);
        return weights_.size();
    }

    auto empty() const -> bool { return size() == 0; }

    void add(const std::string& key, T weight) {
        std::unique_lock lock(mutex_);
        weights_[key] = weight;
    }

    auto update(const std::string& key, T weight) -> bool {
        std::unique_lock lock(mutex_);
        auto it = weights_.find(key);
        if (it == weights_.end()) {
            return false;
        }
        it->second = weight;
        return true;
    }

    auto remove(const std::string& key) -> bool {
        std::unique_lock lock(mutex_);
        return weights_.erase(key) > 0;
    }

    auto contains(const std::string& key) const -> bool {
        std::shared_lock lock(mutex_);
        return weights_.contains(key);
    }

    void clear() {
        std::unique_lock lock(mutex_);
        weights_.clear();
    }

    auto get(const std::string& key) const -> std::optional<T> {
        std::shared_lock lock(mutex_);
        auto it = weights_.find(key);
        if (it == weights_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    auto totalWeight() const -> T {
        std::shared_lock lock(mutex_);
        return std::accumulate(
            weights_.begin(), weights_.end(), static_cast<T>(0),
            [](T acc, const auto& pair) { return acc + pair.second; });
    }

    void normalize() {
        std::unique_lock lock(mutex_);
        T total = std::accumulate(
            weights_.begin(), weights_.end(), static_cast<T>(0),
            [](T acc, const auto& pair) { return acc + pair.second; });
        if (total == static_cast<T>(0)) {
            return;
        }
        for (auto& [_, weight] : weights_) {
            weight /= total;
        }
    }

    void scale(T factor) {
        std::unique_lock lock(mutex_);
        for (auto& [_, weight] : weights_) {
            weight *= factor;
        }
    }

    auto keys() const -> std::vector<std::string> {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(weights_.size());
        for (const auto& [key, _] : weights_) {
            result.push_back(key);
        }
        return result;
    }

    auto values() const -> std::vector<T> {
        std::shared_lock lock(mutex_);
        std::vector<T> result;
        result.reserve(weights_.size());
        for (const auto& [_, value] : weights_) {
            result.push_back(value);
        }
        return result;
    }

    auto selectWeighted() -> std::optional<std::string> {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            return std::nullopt;
        }

        T total = totalWeightUnsafe();
        if (total == static_cast<T>(0)) {
            return std::nullopt;
        }

        std::uniform_real_distribution<double> dist(0.0,
                                                    static_cast<double>(total));
        double target = dist(random_engine_);

        double cumulative = 0.0;
        for (const auto& [key, weight] : weights_) {
            cumulative += static_cast<double>(weight);
            if (target <= cumulative) {
                return key;
            }
        }

        return std::nullopt;
    }

    template <typename Predicate>
    auto filter(Predicate predicate) const -> WeightCollection {
        WeightCollection filtered;
        std::shared_lock lock(mutex_);
        for (const auto& [key, weight] : weights_) {
            if (predicate(key, weight)) {
                filtered.add(key, weight);
            }
        }
        return filtered;
    }

    template <typename Func>
    void apply(Func func) {
        std::unique_lock lock(mutex_);
        for (auto& [_, weight] : weights_) {
            weight = func(weight);
        }
    }

    auto maxWeightKey() const -> std::optional<std::string> {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            return std::nullopt;
        }
        auto it = std::max_element(weights_.begin(), weights_.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                       return lhs.second < rhs.second;
                                   });
        return it->first;
    }

    auto minWeightKey() const -> std::optional<std::string> {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            return std::nullopt;
        }
        auto it = std::min_element(weights_.begin(), weights_.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                       return lhs.second < rhs.second;
                                   });
        return it->first;
    }

private:
    // Unsafe helpers assume caller already holds lock
    auto totalWeightUnsafe() const -> T {
        return std::accumulate(
            weights_.begin(), weights_.end(), static_cast<T>(0),
            [](T acc, const auto& pair) { return acc + pair.second; });
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, T> weights_;
    std::mt19937 random_engine_{std::random_device{}()};
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_COLLECTION_HPP
