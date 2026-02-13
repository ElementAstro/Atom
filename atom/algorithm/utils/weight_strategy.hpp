#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_STRATEGY_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_STRATEGY_HPP

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/utils/random/random.hpp"
#include "weight_common.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#include <boost/range/algorithm.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Base strategy interface for weight selection algorithms
 * @tparam T The numeric type used for weights (must satisfy WeightType concept)
 */
template <WeightType T>
class SelectionStrategy {
public:
    virtual ~SelectionStrategy() = default;

    /**
     * @brief Select an index based on weights
     * @param cumulative_weights Cumulative weights array
     * @param total_weight Sum of all weights
     * @return Selected index
     */
    [[nodiscard]] virtual auto select(std::span<const T> cumulative_weights,
                                      T total_weight) const -> usize = 0;

    /**
     * @brief Create a clone of this strategy
     * @return Unique pointer to a clone
     */
    [[nodiscard]] virtual auto clone() const
        -> std::unique_ptr<SelectionStrategy> = 0;
};

/**
 * @brief Standard weight selection with uniform probability distribution
 */
template <WeightType T>
class DefaultSelectionStrategy : public SelectionStrategy<T> {
private:
#ifdef ATOM_USE_BOOST
    mutable utils::Random<boost::random::mt19937,
                          boost::random::uniform_real_distribution<>>
        random_;
#else
    mutable utils::Random<std::mt19937, std::uniform_real_distribution<>>
        random_;
#endif
    static constexpr T min_value = static_cast<T>(0.0);
    static constexpr T max_value = static_cast<T>(1.0);

public:
    DefaultSelectionStrategy() : random_(min_value, max_value) {}

    explicit DefaultSelectionStrategy(u32 seed)
        : random_(min_value, max_value, seed) {}

    [[nodiscard]] auto select(std::span<const T> cumulative_weights,
                              T total_weight) const -> usize override {
        T randomValue = random_() * total_weight;
#ifdef ATOM_USE_BOOST
        auto it =
            boost::range::upper_bound(cumulative_weights, randomValue);
#else
        auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
        return std::distance(cumulative_weights.begin(), it);
    }

    [[nodiscard]] auto clone() const
        -> std::unique_ptr<SelectionStrategy<T>> override {
        return std::make_unique<DefaultSelectionStrategy>(*this);
    }
};

/**
 * @brief Selection strategy that favors lower indices (square root
 * distribution)
 */
template <WeightType T>
class BottomHeavySelectionStrategy : public SelectionStrategy<T> {
private:
#ifdef ATOM_USE_BOOST
    mutable utils::Random<boost::random::mt19937,
                          boost::random::uniform_real_distribution<>>
        random_;
#else
    mutable utils::Random<std::mt19937, std::uniform_real_distribution<>>
        random_;
#endif
    static constexpr T min_value = static_cast<T>(0.0);
    static constexpr T max_value = static_cast<T>(1.0);

public:
    BottomHeavySelectionStrategy() : random_(min_value, max_value) {}

    explicit BottomHeavySelectionStrategy(u32 seed)
        : random_(min_value, max_value, seed) {}

    [[nodiscard]] auto select(std::span<const T> cumulative_weights,
                              T total_weight) const -> usize override {
        T randomValue = std::sqrt(random_()) * total_weight;
#ifdef ATOM_USE_BOOST
        auto it =
            boost::range::upper_bound(cumulative_weights, randomValue);
#else
        auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
        return std::distance(cumulative_weights.begin(), it);
    }

    [[nodiscard]] auto clone() const
        -> std::unique_ptr<SelectionStrategy<T>> override {
        return std::make_unique<BottomHeavySelectionStrategy>(*this);
    }
};

/**
 * @brief Completely random selection strategy (ignores weights)
 */
template <WeightType T>
class RandomSelectionStrategy : public SelectionStrategy<T> {
private:
#ifdef ATOM_USE_BOOST
    mutable utils::Random<boost::random::mt19937,
                          boost::random::uniform_int_distribution<>>
        random_index_;
#else
    mutable utils::Random<std::mt19937, std::uniform_int_distribution<>>
        random_index_;
#endif
    usize max_index_;

public:
    explicit RandomSelectionStrategy(usize max_index)
        : random_index_(static_cast<usize>(0),
                        max_index > 0 ? max_index - 1 : 0),
          max_index_(max_index) {}

    RandomSelectionStrategy(usize max_index, u32 seed)
        : random_index_(0, max_index > 0 ? max_index - 1 : 0, seed),
          max_index_(max_index) {}

    [[nodiscard]] auto select(std::span<const T> /*cumulative_weights*/,
                              T /*total_weight*/) const -> usize override {
        return random_index_();
    }

    void updateMaxIndex(usize new_max_index) {
        max_index_ = new_max_index;
        random_index_ = decltype(random_index_)(
            static_cast<usize>(0),
            new_max_index > 0 ? new_max_index - 1 : 0);
    }

    [[nodiscard]] auto clone() const
        -> std::unique_ptr<SelectionStrategy<T>> override {
        return std::make_unique<RandomSelectionStrategy>(max_index_);
    }
};

/**
 * @brief Selection strategy that favors higher indices (squared
 * distribution)
 */
template <WeightType T>
class TopHeavySelectionStrategy : public SelectionStrategy<T> {
private:
#ifdef ATOM_USE_BOOST
    mutable utils::Random<boost::random::mt19937,
                          boost::random::uniform_real_distribution<>>
        random_;
#else
    mutable utils::Random<std::mt19937, std::uniform_real_distribution<>>
        random_;
#endif
    static constexpr T min_value = static_cast<T>(0.0);
    static constexpr T max_value = static_cast<T>(1.0);

public:
    TopHeavySelectionStrategy() : random_(min_value, max_value) {}

    explicit TopHeavySelectionStrategy(u32 seed)
        : random_(min_value, max_value, seed) {}

    [[nodiscard]] auto select(std::span<const T> cumulative_weights,
                              T total_weight) const -> usize override {
        T randomValue = std::pow(random_(), 2) * total_weight;
#ifdef ATOM_USE_BOOST
        auto it =
            boost::range::upper_bound(cumulative_weights, randomValue);
#else
        auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
        return std::distance(cumulative_weights.begin(), it);
    }

    [[nodiscard]] auto clone() const
        -> std::unique_ptr<SelectionStrategy<T>> override {
        return std::make_unique<TopHeavySelectionStrategy>(*this);
    }
};

/**
 * @brief Custom power-law distribution selection strategy
 */
template <WeightType T>
class PowerLawSelectionStrategy : public SelectionStrategy<T> {
private:
#ifdef ATOM_USE_BOOST
    mutable utils::Random<boost::random::mt19937,
                          boost::random::uniform_real_distribution<>>
        random_;
#else
    mutable utils::Random<std::mt19937, std::uniform_real_distribution<>>
        random_;
#endif
    T exponent_;
    static constexpr T min_value = static_cast<T>(0.0);
    static constexpr T max_value = static_cast<T>(1.0);

public:
    explicit PowerLawSelectionStrategy(T exponent = 2.0)
        : random_(static_cast<T>(min_value), static_cast<T>(max_value)),
          exponent_(exponent) {
        if (exponent <= 0) {
            throw WeightError("Exponent must be positive");
        }
    }

    PowerLawSelectionStrategy(T exponent, u32 seed)
        : random_(min_value, max_value, seed), exponent_(exponent) {
        if (exponent <= 0) {
            throw WeightError("Exponent must be positive");
        }
    }

    [[nodiscard]] auto select(std::span<const T> cumulative_weights,
                              T total_weight) const -> usize override {
        T randomValue = std::pow(random_(), exponent_) * total_weight;
#ifdef ATOM_USE_BOOST
        auto it =
            boost::range::upper_bound(cumulative_weights, randomValue);
#else
        auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
        return std::distance(cumulative_weights.begin(), it);
    }

    void setExponent(T exponent) {
        if (exponent <= 0) {
            throw WeightError("Exponent must be positive");
        }
        exponent_ = exponent;
    }

    [[nodiscard]] auto getExponent() const noexcept -> T {
        return exponent_;
    }

    [[nodiscard]] auto clone() const
        -> std::unique_ptr<SelectionStrategy<T>> override {
        return std::make_unique<PowerLawSelectionStrategy>(exponent_);
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_STRATEGY_HPP
