#ifndef ATOM_ALGORITHM_WEIGHT_HPP
#define ATOM_ALGORITHM_WEIGHT_HPP

#include <algorithm>
#include <cassert>
#include <cmath>  // For std::pow
#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"
#include "atom/utils/random.hpp"  // Assuming this provides a suitable wrapper or can be adapted

#ifdef ATOM_USE_BOOST
#include <boost/format.hpp>
#include <boost/random.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Concept for numeric types that can be used for weights
 */
template <typename T>
concept WeightType = std::floating_point<T> || std::integral<T>;

/**
 * @brief Exception class for weight-related errors
 */
class WeightError : public std::runtime_error {
public:
    explicit WeightError(
        const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(
              std::format("{}:{}: {}", loc.file_name(), loc.line(), message)) {}
};

/**
 * @brief Core weight selection class with multiple selection strategies
 * @tparam T The numeric type used for weights (must satisfy WeightType concept)
 */
template <WeightType T>
class WeightSelector {
public:
    /**
     * @brief Base strategy interface for weight selection algorithms
     */
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

        /**
         * @brief Update internal state based on changes in the number of
         * weights
         * @param new_max_index The new maximum index (size of weights - 1)
         */
        virtual void updateMaxIndex(usize new_max_index) {}
    };

    /**
     * @brief Standard weight selection with uniform probability distribution
     */
    class DefaultSelectionStrategy : public SelectionStrategy {
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
            -> std::unique_ptr<SelectionStrategy> override {
            return std::make_unique<DefaultSelectionStrategy>(*this);
        }
    };

    /**
     * @brief Selection strategy that favors lower indices (square root
     * distribution)
     */
    class BottomHeavySelectionStrategy : public SelectionStrategy {
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
            -> std::unique_ptr<SelectionStrategy> override {
            return std::make_unique<BottomHeavySelectionStrategy>(*this);
        }
    };

    /**
     * @brief Completely random selection strategy (ignores weights)
     */
    class RandomSelectionStrategy : public SelectionStrategy {
    private:
#ifdef ATOM_USE_BOOST
        mutable boost::random::mt19937 gen_;
        mutable boost::random::uniform_int_distribution<> random_index_;
#else
        mutable std::mt19937 gen_;
        mutable std::uniform_int_distribution<> random_index_;
#endif
        usize max_index_;

    public:
        explicit RandomSelectionStrategy(usize max_index)
            : max_index_(max_index) {
            std::random_device rd;
            gen_.seed(rd());
            updateDistribution();
        }

        RandomSelectionStrategy(usize max_index, u32 seed)
            : gen_(seed), max_index_(max_index) {
            updateDistribution();
        }

        [[nodiscard]] auto select(std::span<const T> /*cumulative_weights*/,
                                  T /*total_weight*/) const -> usize override {
            if (max_index_ == 0)
                return 0;  // Handle empty case
            return random_index_(gen_);
        }

        void updateMaxIndex(usize new_max_index) override {
            max_index_ = new_max_index;
            updateDistribution();
        }

        [[nodiscard]] auto clone() const
            -> std::unique_ptr<SelectionStrategy> override {
            // Note: Cloning a strategy with a mutable RNG might not preserve
            // the exact sequence of random numbers if the clone is used in
            // parallel. If deterministic cloning is needed, the RNG state
            // would need to be copied.
            return std::make_unique<RandomSelectionStrategy>(max_index_);
        }

    private:
        void updateDistribution() {
            random_index_ = decltype(random_index_)(
                static_cast<usize>(0), max_index_ > 0 ? max_index_ - 1 : 0);
        }
    };

    /**
     * @brief Selection strategy that favors higher indices (squared
     * distribution)
     */
    class TopHeavySelectionStrategy : public SelectionStrategy {
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
            -> std::unique_ptr<SelectionStrategy> override {
            return std::make_unique<TopHeavySelectionStrategy>(*this);
        }
    };

    /**
     * @brief Custom power-law distribution selection strategy
     */
    class PowerLawSelectionStrategy : public SelectionStrategy {
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
            -> std::unique_ptr<SelectionStrategy> override {
            return std::make_unique<PowerLawSelectionStrategy>(exponent_);
        }
    };

    /**
     * @brief Utility class for batch sampling with replacement and without
     * replacement
     */
    class WeightedRandomSampler {
    private:
#ifdef ATOM_USE_BOOST
        mutable boost::random::mt19937 gen_;
#else
        mutable std::mt19937 gen_;
#endif

    public:
        WeightedRandomSampler() {
            std::random_device rd;
            gen_.seed(rd());
        }
        explicit WeightedRandomSampler(u32 seed) : gen_(seed) {}

        /**
         * @brief Sample n indices according to their weights (with replacement)
         * @param weights The weights for each index
         * @param n Number of samples to draw
         * @return Vector of sampled indices
         */
        [[nodiscard]] auto sample(std::span<const T> weights, usize n) const
            -> std::vector<usize> {
            if (weights.empty()) {
                throw WeightError("Cannot sample from empty weights");
            }

            if (n == 0) {
                return {};
            }

            std::vector<usize> results(n);

#ifdef ATOM_USE_BOOST
            boost::random::discrete_distribution<> dist(weights.begin(),
                                                        weights.end());
            std::generate(results.begin(), results.end(),
                          [&]() { return dist(gen_); });
#else
            std::discrete_distribution<> dist(weights.begin(), weights.end());
            std::generate(results.begin(), results.end(),
                          [&]() { return dist(gen_); });
#endif

            return results;
        }

        /**
         * @brief Sample n unique indices according to their weights (no
         * replacement)
         * @param weights The weights for each index
         * @param n Number of samples to draw
         * @return Vector of sampled indices
         * @throws WeightError if n is greater than the number of weights
         */
        [[nodiscard]] auto sampleUnique(std::span<const T> weights,
                                        usize n) const -> std::vector<usize> {
            if (weights.empty()) {
                throw WeightError("Cannot sample from empty weights");
            }

            if (n > weights.size()) {
                throw WeightError(std::format(
                    "Cannot sample {} unique items from a population of {}", n,
                    weights.size()));
            }

            if (n == 0) {
                return {};
            }

            // Use the more efficient shuffle method for weighted unique
            // sampling
            return sampleUniqueShuffle(weights, n);
        }

    private:
        // Rejection sampling method (kept for comparison, but shuffle is
        // generally better for weighted unique)
        [[nodiscard]] auto sampleUniqueRejection(std::span<const T> weights,
                                                 usize n) const
            -> std::vector<usize> {
            std::vector<usize> results;
            results.reserve(n);

            std::vector<bool> selected(weights.size(), false);

#ifdef ATOM_USE_BOOST
            boost::random::discrete_distribution<> dist(weights.begin(),
                                                        weights.end());
            while (results.size() < n) {
                usize idx = dist(gen_);
                if (!selected[idx]) {
                    selected[idx] = true;
                    results.push_back(idx);
                }
            }
#else
            std::discrete_distribution<> dist(weights.begin(), weights.end());
            while (results.size() < n) {
                usize idx = dist(gen_);
                if (!selected[idx]) {
                    selected[idx] = true;
                    results.push_back(idx);
                }
            }
#endif

            return results;
        }

        // Optimized shuffle method for weighted unique sampling
        [[nodiscard]] auto sampleUniqueShuffle(std::span<const T> weights,
                                               usize n) const
            -> std::vector<usize> {
            // Create a vector of pairs (random_value_derived_from_weight,
            // index)
            std::vector<std::pair<double, usize>> weighted_indices;
            weighted_indices.reserve(weights.size());

            std::uniform_real_distribution<double> dist(0.0, 1.0);

            for (usize i = 0; i < weights.size(); ++i) {
                T weight = weights[i];
                double random_value;
                if (weight <= 0) {
                    // Assign a value that will sort it to the end
                    random_value = -1.0;  // Or some value guaranteed to be low
                } else {
                    // Generate a random value such that higher weights are more
                    // likely to get a higher value Using log(rand()) / weight
                    // is a common trick (Gumbel-max related) Or pow(rand(),
                    // 1/weight) - need to sort descending for this
                    random_value =
                        std::pow(dist(gen_), 1.0 / static_cast<double>(weight));
                }
                weighted_indices.emplace_back(random_value, i);
            }

            // Sort by the calculated random values in descending order
            std::ranges::sort(
                weighted_indices,
                [](const auto& a, const auto& b) { return a.first > b.first; });

            // Extract the top n indices
            std::vector<usize> results;
            results.reserve(n);

            for (usize i = 0; i < n; ++i) {
                if (weighted_indices[i].first < 0) {
                    // Stop if we encounter weights that were zero or negative
                    // This handles cases where n is larger than the count of
                    // positive weights
                    break;
                }
                results.push_back(weighted_indices[i].second);
            }

            // If we didn't get enough unique samples because of zero/negative
            // weights, this indicates an issue or expectation mismatch, but the
            // current logic correctly returns fewer than n if there aren't
            // enough valid items. If exactly n unique items with positive
            // weights are required, additional error handling or logic would be
            // needed here. For now, we return what we got from the top N
            // positive-weighted items.
            return results;
        }
    };

private:
    std::vector<T> weights_;
    std::vector<T> cumulative_weights_;
    std::unique_ptr<SelectionStrategy> strategy_;
    mutable std::shared_mutex mutex_;  // For thread safety
    u32 seed_ =
        0;  // Seed is primarily for the Sampler, not the main strategy RNGs
    bool weights_dirty_ = true;

    /**
     * @brief Updates the cumulative weights array
     * @note This function is not thread-safe and should be called with proper
     * synchronization (unique_lock). Assumes weights_ is already validated.
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
     * @brief Validates that the weights are non-negative
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
                            std::unique_ptr<SelectionStrategy> custom_strategy =
                                std::make_unique<DefaultSelectionStrategy>())
        : weights_(input_weights.begin(), input_weights.end()),
          strategy_(std::move(custom_strategy)) {
        validateWeights();
        updateCumulativeWeights();
        // Inform strategy about initial size if it cares (e.g.,
        // RandomSelectionStrategy)
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
        }
    }

    /**
     * @brief Construct a WeightSelector with the given weights, strategy, and
     * seed
     * @param input_weights The initial weights
     * @param seed Seed for random number generation (primarily for Sampler)
     * @param custom_strategy Custom selection strategy (defaults to
     * DefaultSelectionStrategy)
     * @throws WeightError If input weights contain negative values
     */
    WeightSelector(std::span<const T> input_weights, u32 seed,
                   std::unique_ptr<SelectionStrategy> custom_strategy =
                       std::make_unique<DefaultSelectionStrategy>())
        : weights_(input_weights.begin(), input_weights.end()),
          strategy_(std::move(custom_strategy)),
          seed_(seed) {
        validateWeights();
        updateCumulativeWeights();
        // Inform strategy about initial size if it cares (e.g.,
        // RandomSelectionStrategy)
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
        }
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
            // Use std::scoped_lock for multiple mutexes in C++17+
            std::scoped_lock lock(mutex_, other.mutex_);

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
            // Use std::scoped_lock for multiple mutexes in C++17+
            // Note: shared_lock for 'other' is sufficient for reading its state
            std::unique_lock self_lock(mutex_);
            std::shared_lock other_lock(other.mutex_);
            // std::scoped_lock would require both to be unique_lock

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
    void setSelectionStrategy(std::unique_ptr<SelectionStrategy> new_strategy) {
        std::unique_lock lock(mutex_);
        strategy_ = std::move(new_strategy);
        // Inform new strategy about current size
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
        }
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

        // Calculate total weight under shared lock first
        T totalWeight = calculateTotalWeight();
        if (totalWeight <= T{0}) {
            throw WeightError(std::format(
                "Total weight must be positive (current: {})", totalWeight));
        }

        // If weights are dirty, we need to upgrade to a unique lock to update
        // cumulative weights.
        if (weights_dirty_) {
            lock.unlock();                        // Release shared lock
            std::unique_lock write_lock(mutex_);  // Acquire unique lock
            // Double-check weights_dirty_ in case another thread updated it
            if (weights_dirty_) {
                updateCumulativeWeights();
            }
            // write_lock goes out of scope, releasing unique lock
        }
        // Re-acquire shared lock for selection if it was released
        if (!lock.owns_lock()) {
            lock.lock();
        }

        // Now cumulative_weights_ is up-to-date (or was already)
        // We need to ensure the strategy's select method is thread-safe if it
        // uses mutable members (like RNGs). The current strategy
        // implementations use mutable RNGs but are called under the
        // WeightSelector's lock, which makes them safe in this context.
        return strategy_->select(cumulative_weights_, totalWeight);
    }

    /**
     * @brief Selects multiple indices based on weights (with replacement)
     * @param n Number of selections to make
     * @return Vector of selected indices
     */
    [[nodiscard]] auto selectMultiple(usize n) -> std::vector<usize> {
        if (n == 0)
            return {};

        std::vector<usize> results;
        results.reserve(n);

        // Each call to select() acquires and releases the lock, which might be
        // inefficient for large N. A batch selection method within the strategy
        // or Sampler would be better. For now, keep the simple loop.
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
     * @throws WeightError if n > number of weights or if total positive weight
     * is zero
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

        // Check if there are enough items with positive weight
        T totalPositiveWeight = std::accumulate(
            weights_.begin(), weights_.end(), T{0},
            [](T sum, T w) { return sum + (w > T{0} ? w : T{0}); });

        if (n > 0 && totalPositiveWeight <= T{0}) {
            throw WeightError(
                "Cannot select unique items when total positive weight is "
                "zero");
        }

        // WeightedRandomSampler handles its own seeding internally now
        WeightedRandomSampler sampler(seed_);
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
        // No need to update strategy max index here as size didn't change
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

        // Update strategy about the new size
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
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

        // Update strategy about the new size
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
        }
    }

    /**
     * @brief Normalizes weights so they sum to 1.0
     * @throws WeightError if all weights are zero or negative
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
        // No need to update strategy max index here as size didn't change
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
        return weights_;  // Returns a copy
    }

    /**
     * @brief Calculates the sum of all weights
     * @return Total weight
     * @note This method does NOT acquire a lock. It's a helper for methods that
     * already hold a lock.
     */
    [[nodiscard]] auto calculateTotalWeight() const -> T {
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
    [[nodiscard]] auto getTotalWeight() const -> T {
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

        // Update strategy about the new size
        if (strategy_) {
            strategy_->updateMaxIndex(weights_.size());
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
    [[nodiscard]] auto getAverageWeight() const -> T {
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
     * @brief Sets the random seed for the internal Sampler.
     * @param seed The new seed value
     */
    void setSeed(u32 seed) {
        std::unique_lock lock(mutex_);
        seed_ = seed;
        // Note: This seed is primarily used by the WeightedRandomSampler
        // created within selectUniqueMultiple. Strategies manage their own
        // RNGs.
    }

    /**
     * @brief Clears all weights
     */
    void clear() {
        std::unique_lock lock(mutex_);
        weights_.clear();
        cumulative_weights_.clear();
        weights_dirty_ = false;

        // Update strategy about the new size
        if (strategy_) {
            strategy_->updateMaxIndex(0);
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
        result.reserve(weights_.size());  // Reserve maximum possible space

        for (usize i = 0; i < weights_.size(); ++i) {
            if (std::invoke(std::forward<P>(predicate), weights_[i])) {
                result.push_back(i);
            }
        }

        return result;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_WEIGHT_HPP