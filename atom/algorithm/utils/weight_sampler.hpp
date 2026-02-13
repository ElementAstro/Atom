#ifndef ATOM_ALGORITHM_UTILS_WEIGHT_SAMPLER_HPP
#define ATOM_ALGORITHM_UTILS_WEIGHT_SAMPLER_HPP

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "weight_common.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Utility class for batch sampling with replacement
 * @tparam T The numeric type used for weights (must satisfy WeightType concept)
 */
template <WeightType T>
class WeightedRandomSampler {
private:
    std::optional<u32> seed_;

public:
    WeightedRandomSampler() = default;
    explicit WeightedRandomSampler(u32 seed) : seed_(seed) {}

    /**
     * @brief Sample n indices according to their weights
     * @param weights The weights for each index
     * @param n Number of samples to draw
     * @return Vector of sampled indices
     */
    [[nodiscard]] auto sample(std::span<const T> weights,
                              usize n) const -> std::vector<usize> {
        if (weights.empty()) {
            throw WeightError("Cannot sample from empty weights");
        }

        if (n == 0) {
            return {};
        }

        std::vector<usize> results(n);

#ifdef ATOM_USE_BOOST
        utils::Random<boost::random::mt19937,
                      boost::random::discrete_distribution<>>
            random(weights.begin(), weights.end(),
                   seed_.has_value() ? *seed_ : 0);

        std::generate(results.begin(), results.end(),
                      [&]() { return random(); });
#else
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        std::mt19937 gen;

        if (seed_.has_value()) {
            gen.seed(*seed_);
        } else {
            std::random_device rd;
            gen.seed(rd());
        }

        std::generate(results.begin(), results.end(),
                      [&]() { return dist(gen); });
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

        // For small n compared to weights size, use rejection sampling
        if (n <= weights.size() / 4) {
            return sampleUniqueRejection(weights, n);
        } else {
            // For larger n, use the algorithm based on shuffling
            return sampleUniqueShuffle(weights, n);
        }
    }

private:
    [[nodiscard]] auto sampleUniqueRejection(
        std::span<const T> weights, usize n) const -> std::vector<usize> {
        std::vector<usize> indices(weights.size());
        std::iota(indices.begin(), indices.end(), 0);

        std::vector<usize> results;
        results.reserve(n);

        std::vector<bool> selected(weights.size(), false);

#ifdef ATOM_USE_BOOST
        utils::Random<boost::random::mt19937,
                      boost::random::discrete_distribution<>>
            random(weights.begin(), weights.end(),
                   seed_.has_value() ? *seed_ : 0);

        while (results.size() < n) {
            usize idx = random();
            if (!selected[idx]) {
                selected[idx] = true;
                results.push_back(idx);
            }
        }
#else
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        std::mt19937 gen;

        if (seed_.has_value()) {
            gen.seed(*seed_);
        } else {
            std::random_device rd;
            gen.seed(rd());
        }

        while (results.size() < n) {
            usize idx = dist(gen);
            if (!selected[idx]) {
                selected[idx] = true;
                results.push_back(idx);
            }
        }
#endif

        return results;
    }

    [[nodiscard]] auto sampleUniqueShuffle(
        std::span<const T> weights, usize n) const -> std::vector<usize> {
        std::vector<usize> indices(weights.size());
        std::iota(indices.begin(), indices.end(), 0);

        // Create a vector of pairs (weight, index)
        std::vector<std::pair<T, usize>> weighted_indices;
        weighted_indices.reserve(weights.size());

        for (usize i = 0; i < weights.size(); ++i) {
            weighted_indices.emplace_back(weights[i], i);
        }

        // Generate random values
#ifdef ATOM_USE_BOOST
        boost::random::mt19937 gen(
            seed_.has_value() ? *seed_ : std::random_device{}());
#else
        std::mt19937 gen;
        if (seed_.has_value()) {
            gen.seed(*seed_);
        } else {
            std::random_device rd;
            gen.seed(rd());
        }
#endif

        // Sort by weighted random values
        std::ranges::sort(
            weighted_indices, [&](const auto& a, const auto& b) {
                // Generate a random value weighted by the item's weight
                T weight_a = a.first;
                T weight_b = b.first;

                if (weight_a <= 0 && weight_b <= 0)
                    return false;  // arbitrary order for zero weights
                if (weight_a <= 0)
                    return false;
                if (weight_b <= 0)
                    return true;

                // Generate random values weighted by the weights
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                double r_a = std::pow(dist(gen), 1.0 / weight_a);
                double r_b = std::pow(dist(gen), 1.0 / weight_b);

                return r_a > r_b;
            });

        // Extract the top n indices
        std::vector<usize> results;
        results.reserve(n);

        for (usize i = 0; i < n; ++i) {
            results.push_back(weighted_indices[i].second);
        }

        return results;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_WEIGHT_SAMPLER_HPP
