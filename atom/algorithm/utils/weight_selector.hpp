#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_SELECTOR_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_SELECTOR_HPP

#include <algorithm>
#include <format>
#include <memory>
#include <numeric>
#include <ostream>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "weight_common.hpp"
#include "weight_sampler.hpp"
#include "weight_strategy.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/format.hpp>
#include <boost/random.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Core weight selection class with multiple selection strategies
 * @tparam T The numeric type used for weights (must satisfy WeightType concept)
 */
template <WeightType T>
class WeightSelector {
public:
    // Backward-compatible type aliases for nested strategy classes
    using SelectionStrategy = atom::algorithm::SelectionStrategy<T>;
    using DefaultSelectionStrategy = atom::algorithm::DefaultSelectionStrategy<T>;
    using BottomHeavySelectionStrategy = atom::algorithm::BottomHeavySelectionStrategy<T>;
    using RandomSelectionStrategy = atom::algorithm::RandomSelectionStrategy<T>;
    using TopHeavySelectionStrategy = atom::algorithm::TopHeavySelectionStrategy<T>;
    using PowerLawSelectionStrategy = atom::algorithm::PowerLawSelectionStrategy<T>;
    using WeightedRandomSampler = atom::algorithm::WeightedRandomSampler<T>;

private:
    std::vector<T> weights_;
    std::vector<T> cumulative_weights_;
    std::unique_ptr<atom::algorithm::SelectionStrategy<T>> strategy_;
    mutable std::shared_mutex mutex_;  // For thread safety
    u32 seed_ = 0;
    bool weights_dirty_ = true;

    /**
     * @brief Updates the cumulative weights array
     * @note This function is not thread-safe and should be called with proper
     * synchronization
     */
    void updateCumulativeWeights() {
        if (!weights_dirty_)
            return;

        if (weights_.empty()) {
            cumulative_weights_.clear();
            weights_dirty_ = false;
            return;
        }

        cumulative_weights_.resize(weights_.size());
#ifdef ATOM_USE_BOOST
        boost::range::partial_sum(weights_, cumulative_weights_.begin());
#else
        std::partial_sum(weights_.begin(), weights_.end(),
                         cumulative_weights_.begin());
#endif
        weights_dirty_ = false;
    }

    /**
     * @brief Validates that the weights are positive
     * @throws WeightError if any weight is negative
     */
    void validateWeights() const {
        for (usize i = 0; i < weights_.size(); ++i) {
            if (weights_[i] < T{0}) {
                throw WeightError(std::format(
                    "Weight at index {} is negative: {}", i, weights_[i]));
            }
        }
    }

public:
    /**
     * @brief Construct a WeightSelector with the given weights and strategy
     * @param input_weights The initial weights
     * @param custom_strategy Custom selection strategy (defaults to
     * DefaultSelectionStrategy)
     * @throws WeightError If input weights contain negative values
     */
    explicit WeightSelector(std::span<const T> input_weights,
                            std::unique_ptr<atom::algorithm::SelectionStrategy<T>> custom_strategy =
                                std::make_unique<atom::algorithm::DefaultSelectionStrategy<T>>())
        : weights_(input_weights.begin(), input_weights.end()),
          strategy_(std::move(custom_strategy)) {
        validateWeights();
        updateCumulativeWeights();
    }

    /**
     * @brief Construct a WeightSelector with the given weights, strategy, and
     * seed
     * @param input_weights The initial weights
     * @param seed Seed for random number generation
     * @param custom_strategy Custom selection strategy (defaults to
     * DefaultSelectionStrategy)
     * @throws WeightError If input weights contain negative values
     */
    WeightSelector(std::span<const T> input_weights, u32 seed,
                   std::unique_ptr<atom::algorithm::SelectionStrategy<T>> custom_strategy =
                       std::make_unique<atom::algorithm::DefaultSelectionStrategy<T>>())
        : weights_(input_weights.begin(), input_weights.end()),
          strategy_(std::move(custom_strategy)),
          seed_(seed) {
        validateWeights();
        updateCumulativeWeights();
    }

    /**
     * @brief Move constructor
     */
    WeightSelector(WeightSelector&& other) noexcept
        : weights_(std::move(other.weights_)),
          cumulative_weights_(std::move(other.cumulative_weights_)),
          strategy_(std::move(other.strategy_)),
          seed_(other.seed_),
          weights_dirty_(other.weights_dirty_) {}

    /**
     * @brief Move assignment operator
     */
    WeightSelector& operator=(WeightSelector&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            weights_ = std::move(other.weights_);
            cumulative_weights_ = std::move(other.cumulative_weights_);
            strategy_ = std::move(other.strategy_);
            seed_ = other.seed_;
            weights_dirty_ = other.weights_dirty_;
        }
        return *this;
    }

    /**
     * @brief Copy constructor
     */
    WeightSelector(const WeightSelector& other)
        : weights_(other.weights_),
          cumulative_weights_(other.cumulative_weights_),
          strategy_(other.strategy_ ? other.strategy_->clone() : nullptr),
          seed_(other.seed_),
          weights_dirty_(other.weights_dirty_) {}

    /**
     * @brief Copy assignment operator
     */
    WeightSelector& operator=(const WeightSelector& other) {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::shared_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            weights_ = other.weights_;
            cumulative_weights_ = other.cumulative_weights_;
            strategy_ = other.strategy_ ? other.strategy_->clone() : nullptr;
            seed_ = other.seed_;
            weights_dirty_ = other.weights_dirty_;
        }
        return *this;
    }

    /**
     * @brief Sets a new selection strategy
     * @param new_strategy The new selection strategy to use
     */
    void setSelectionStrategy(std::unique_ptr<atom::algorithm::SelectionStrategy<T>> new_strategy) {
        std::unique_lock lock(mutex_);
        strategy_ = std::move(new_strategy);
    }

    /**
     * @brief Selects an index based on weights using the current strategy
     * @return Selected index
     * @throws WeightError if total weight is zero or negative
     */
    [[nodiscard]] auto select() -> usize {
        std::shared_lock lock(mutex_);

        if (weights_.empty()) {
            throw WeightError("Cannot select from empty weights");
        }

        T totalWeight = calculateTotalWeight();
        if (totalWeight <= T{0}) {
            throw WeightError(std::format(
                "Total weight must be positive (current: {})", totalWeight));
        }

        if (weights_dirty_) {
            lock.unlock();
            std::unique_lock write_lock(mutex_);
            if (weights_dirty_) {
                updateCumulativeWeights();
            }
            write_lock.unlock();
            lock.lock();
        }

        return strategy_->select(cumulative_weights_, totalWeight);
    }

    /**
     * @brief Selects multiple indices based on weights
     * @param n Number of selections to make
     * @return Vector of selected indices
     */
    [[nodiscard]] auto selectMultiple(usize n) -> std::vector<usize> {
        if (n == 0)
            return {};

        std::vector<usize> results;
        results.reserve(n);

        for (usize i = 0; i < n; ++i) {
            results.push_back(select());
        }

        return results;
    }

    /**
     * @brief Selects multiple unique indices based on weights (without
     * replacement)
     * @param n Number of selections to make
     * @return Vector of unique selected indices
     * @throws WeightError if n > number of weights
     */
    [[nodiscard]] auto selectUniqueMultiple(usize n) const
        -> std::vector<usize> {
        if (n == 0)
            return {};

        std::shared_lock lock(mutex_);

        if (n > weights_.size()) {
            throw WeightError(std::format(
                "Cannot select {} unique items from a population of {}", n,
                weights_.size()));
        }

        atom::algorithm::WeightedRandomSampler<T> sampler(seed_);
        return sampler.sampleUnique(weights_, n);
    }

    /**
     * @brief Updates a single weight
     * @param index Index of the weight to update
     * @param new_weight New weight value
     * @throws std::out_of_range if index is out of bounds
     * @throws WeightError if new_weight is negative
     */
    void updateWeight(usize index, T new_weight) {
        if (new_weight < T{0}) {
            throw WeightError(
                std::format("Weight cannot be negative: {}", new_weight));
        }

        std::unique_lock lock(mutex_);
        if (index >= weights_.size()) {
            throw std::out_of_range(std::format(
                "Index {} out of range (size: {})", index, weights_.size()));
        }
        weights_[index] = new_weight;
        weights_dirty_ = true;
    }

    /**
     * @brief Adds a new weight to the collection
     * @param new_weight Weight to add
     * @throws WeightError if new_weight is negative
     */
    void addWeight(T new_weight) {
        if (new_weight < T{0}) {
            throw WeightError(
                std::format("Weight cannot be negative: {}", new_weight));
        }

        std::unique_lock lock(mutex_);
        weights_.push_back(new_weight);
        weights_dirty_ = true;

        // Update RandomSelectionStrategy if that's what we're using
        if (auto* random_strategy =
                dynamic_cast<atom::algorithm::RandomSelectionStrategy<T>*>(strategy_.get())) {
            random_strategy->updateMaxIndex(weights_.size());
        }
    }

    /**
     * @brief Removes a weight at the specified index
     * @param index Index of the weight to remove
     * @throws std::out_of_range if index is out of bounds
     */
    void removeWeight(usize index) {
        std::unique_lock lock(mutex_);
        if (index >= weights_.size()) {
            throw std::out_of_range(std::format(
                "Index {} out of range (size: {})", index, weights_.size()));
        }
        weights_.erase(weights_.begin() + static_cast<std::ptrdiff_t>(index));
        weights_dirty_ = true;

        // Update RandomSelectionStrategy if that's what we're using
        if (auto* random_strategy =
                dynamic_cast<atom::algorithm::RandomSelectionStrategy<T>*>(strategy_.get())) {
            random_strategy->updateMaxIndex(weights_.size());
        }
    }

    /**
     * @brief Normalizes weights so they sum to 1.0
     * @throws WeightError if all weights are zero
     */
    void normalizeWeights() {
        std::unique_lock lock(mutex_);
        T sum = calculateTotalWeight();

        if (sum <= T{0}) {
            throw WeightError(
                "Cannot normalize: total weight must be positive");
        }

#ifdef ATOM_USE_BOOST
        boost::transform(weights_, weights_.begin(),
                         [sum](T w) { return w / sum; });
#else
        std::ranges::transform(weights_, weights_.begin(),
                               [sum](T w) { return w / sum; });
#endif
        weights_dirty_ = true;
    }

    /**
     * @brief Applies a function to all weights
     * @param func Function that takes and returns a weight value
     * @throws WeightError if resulting weights are negative
     */
    template <std::invocable<T> F>
    void applyFunctionToWeights(F&& func) {
        std::unique_lock lock(mutex_);

#ifdef ATOM_USE_BOOST
        boost::transform(weights_, weights_.begin(), std::forward<F>(func));
#else
        std::ranges::transform(weights_, weights_.begin(),
                               std::forward<F>(func));
#endif

        // Validate weights after transformation
        validateWeights();
        weights_dirty_ = true;
    }

    /**
     * @brief Updates multiple weights in batch
     * @param updates Vector of (index, new_weight) pairs
     * @throws std::out_of_range if any index is out of bounds
     * @throws WeightError if any new weight is negative
     */
    void batchUpdateWeights(const std::vector<std::pair<usize, T>>& updates) {
        std::unique_lock lock(mutex_);

        // Validate first
        for (const auto& [index, new_weight] : updates) {
            if (index >= weights_.size()) {
                throw std::out_of_range(
                    std::format("Index {} out of range (size: {})", index,
                                weights_.size()));
            }
            if (new_weight < T{0}) {
                throw WeightError(
                    std::format("Weight at index {} cannot be negative: {}",
                                index, new_weight));
            }
        }

        // Then update
        for (const auto& [index, new_weight] : updates) {
            weights_[index] = new_weight;
        }

        weights_dirty_ = true;
    }

    /**
     * @brief Gets the weight at the specified index
     * @param index Index of the weight to retrieve
     * @return Optional containing the weight, or nullopt if index is out of
     * bounds
     */
    [[nodiscard]] auto getWeight(usize index) const -> std::optional<T> {
        std::shared_lock lock(mutex_);
        if (index >= weights_.size()) {
            return std::nullopt;
        }
        return weights_[index];
    }

    /**
     * @brief Gets the index of the maximum weight
     * @return Index of the maximum weight
     * @throws WeightError if weights collection is empty
     */
    [[nodiscard]] auto getMaxWeightIndex() const -> usize {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            throw WeightError(
                "Cannot find max weight index in empty collection");
        }

#ifdef ATOM_USE_BOOST
        return std::distance(weights_.begin(),
                             boost::range::max_element(weights_));
#else
        return std::distance(weights_.begin(),
                             std::ranges::max_element(weights_));
#endif
    }

    /**
     * @brief Gets the index of the minimum weight
     * @return Index of the minimum weight
     * @throws WeightError if weights collection is empty
     */
    [[nodiscard]] auto getMinWeightIndex() const -> usize {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            throw WeightError(
                "Cannot find min weight index in empty collection");
        }

#ifdef ATOM_USE_BOOST
        return std::distance(weights_.begin(),
                             boost::range::min_element(weights_));
#else
        return std::distance(weights_.begin(),
                             std::ranges::min_element(weights_));
#endif
    }

    /**
     * @brief Gets the number of weights
     * @return Number of weights
     */
    [[nodiscard]] auto size() const -> usize {
        std::shared_lock lock(mutex_);
        return weights_.size();
    }

    /**
     * @brief Gets read-only access to the weights
     * @return Span of the weights
     * @note This returns a copy to ensure thread safety
     */
    [[nodiscard]] auto getWeights() const -> std::vector<T> {
        std::shared_lock lock(mutex_);
        return weights_;
    }

    /**
     * @brief Calculates the sum of all weights
     * @return Total weight
     */
    [[nodiscard]] auto calculateTotalWeight() -> T {
#ifdef ATOM_USE_BOOST
        return boost::accumulate(weights_, T{0});
#else
        return std::reduce(weights_.begin(), weights_.end(), T{0});
#endif
    }

    /**
     * @brief Gets the sum of all weights
     * @return Total weight
     */
    [[nodiscard]] auto getTotalWeight() -> T {
        std::shared_lock lock(mutex_);
        return calculateTotalWeight();
    }

    /**
     * @brief Replaces all weights with new values
     * @param new_weights New weights collection
     * @throws WeightError if any weight is negative
     */
    void resetWeights(std::span<const T> new_weights) {
        std::unique_lock lock(mutex_);
        weights_.assign(new_weights.begin(), new_weights.end());
        validateWeights();
        weights_dirty_ = true;

        // Update RandomSelectionStrategy if that's what we're using
        if (auto* random_strategy =
                dynamic_cast<atom::algorithm::RandomSelectionStrategy<T>*>(strategy_.get())) {
            random_strategy->updateMaxIndex(weights_.size());
        }
    }

    /**
     * @brief Multiplies all weights by a factor
     * @param factor Scaling factor
     * @throws WeightError if factor is negative
     */
    void scaleWeights(T factor) {
        if (factor < T{0}) {
            throw WeightError(
                std::format("Scaling factor cannot be negative: {}", factor));
        }

        std::unique_lock lock(mutex_);
#ifdef ATOM_USE_BOOST
        boost::transform(weights_, weights_.begin(),
                         [factor](T w) { return w * factor; });
#else
        std::ranges::transform(weights_, weights_.begin(),
                               [factor](T w) { return w * factor; });
#endif
        weights_dirty_ = true;
    }

    /**
     * @brief Calculates the average of all weights
     * @return Average weight
     * @throws WeightError if weights collection is empty
     */
    [[nodiscard]] auto getAverageWeight() -> T {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            throw WeightError("Cannot calculate average of empty weights");
        }
        return calculateTotalWeight() / static_cast<T>(weights_.size());
    }

    /**
     * @brief Prints weights to the provided output stream
     * @param oss Output stream
     */
    void printWeights(std::ostream& oss) const {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            oss << "[]\n";
            return;
        }

#ifdef ATOM_USE_BOOST
        oss << boost::format("[%1$.2f") % weights_.front();
        for (auto it = weights_.begin() + 1; it != weights_.end(); ++it) {
            oss << boost::format(", %1$.2f") % *it;
        }
        oss << "]\n";
#else
        if constexpr (std::is_floating_point_v<T>) {
            oss << std::format("[{:.2f}", weights_.front());
            for (auto it = weights_.begin() + 1; it != weights_.end(); ++it) {
                oss << std::format(", {:.2f}", *it);
            }
        } else {
            oss << '[' << weights_.front();
            for (auto it = weights_.begin() + 1; it != weights_.end(); ++it) {
                oss << ", " << *it;
            }
        }
        oss << "]\n";
#endif
    }

    /**
     * @brief Sets the random seed for selection strategies
     * @param seed The new seed value
     */
    void setSeed(u32 seed) {
        std::unique_lock lock(mutex_);
        seed_ = seed;
    }

    /**
     * @brief Clears all weights
     */
    void clear() {
        std::unique_lock lock(mutex_);
        weights_.clear();
        cumulative_weights_.clear();
        weights_dirty_ = false;

        // Update RandomSelectionStrategy if that's what we're using
        if (auto* random_strategy =
                dynamic_cast<atom::algorithm::RandomSelectionStrategy<T>*>(strategy_.get())) {
            random_strategy->updateMaxIndex(0);
        }
    }

    /**
     * @brief Reserves space for weights
     * @param capacity New capacity
     */
    void reserve(usize capacity) {
        std::unique_lock lock(mutex_);
        weights_.reserve(capacity);
        cumulative_weights_.reserve(capacity);
    }

    /**
     * @brief Checks if the weights collection is empty
     * @return True if empty, false otherwise
     */
    [[nodiscard]] auto empty() const -> bool {
        std::shared_lock lock(mutex_);
        return weights_.empty();
    }

    /**
     * @brief Gets the weight with the maximum value
     * @return Maximum weight value
     * @throws WeightError if weights collection is empty
     */
    [[nodiscard]] auto getMaxWeight() const -> T {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            throw WeightError("Cannot find max weight in empty collection");
        }

#ifdef ATOM_USE_BOOST
        return *boost::range::max_element(weights_);
#else
        return *std::ranges::max_element(weights_);
#endif
    }

    /**
     * @brief Gets the weight with the minimum value
     * @return Minimum weight value
     * @throws WeightError if weights collection is empty
     */
    [[nodiscard]] auto getMinWeight() const -> T {
        std::shared_lock lock(mutex_);
        if (weights_.empty()) {
            throw WeightError("Cannot find min weight in empty collection");
        }

#ifdef ATOM_USE_BOOST
        return *boost::range::min_element(weights_);
#else
        return *std::ranges::min_element(weights_);
#endif
    }

    /**
     * @brief Finds indices of weights matching a predicate
     * @param predicate Function that takes a weight and returns a boolean
     * @return Vector of indices where predicate returns true
     */
    template <std::predicate<T> P>
    [[nodiscard]] auto findIndices(P&& predicate) const -> std::vector<usize> {
        std::shared_lock lock(mutex_);
        std::vector<usize> result;

        for (usize i = 0; i < weights_.size(); ++i) {
            if (std::invoke(std::forward<P>(predicate), weights_[i])) {
                result.push_back(i);
            }
        }

        return result;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_SELECTOR_HPP
