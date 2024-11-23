#ifndef ATOM_ALGORITHM_WEIGHT_HPP
#define ATOM_ALGORITHM_WEIGHT_HPP

#include <algorithm>
#include <concepts>
#include <format>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/function/concept.hpp"
#include "atom/utils/random.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/format.hpp>
#include <boost/random.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#endif

namespace atom::algorithm {

template <Arithmetic T>
class WeightSelector {
public:
    class SelectionStrategy {
    public:
        virtual ~SelectionStrategy() = default;
        virtual auto select(std::span<const T> cumulative_weights,
                            T total_weight) -> size_t = 0;
    };

    class DefaultSelectionStrategy : public SelectionStrategy {
    private:
#ifdef ATOM_USE_BOOST
        utils::Random<boost::random::mt19937,
                      boost::random::uniform_real_distribution<>>
            random_;
#else
        utils::Random<std::mt19937, std::uniform_real_distribution<>> random_;
#endif

    public:
        DefaultSelectionStrategy() : random_(0.0, 1.0) {}

        auto select(std::span<const T> cumulative_weights,
                    T total_weight) -> size_t override {
            T randomValue = random_() * total_weight;
#ifdef ATOM_USE_BOOST
            auto it =
                boost::range::upper_bound(cumulative_weights, randomValue);
#else
            auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
            return std::distance(cumulative_weights.begin(), it);
        }
    };

    class BottomHeavySelectionStrategy : public SelectionStrategy {
    private:
#ifdef ATOM_USE_BOOST
        utils::Random<boost::random::mt19937,
                      boost::random::uniform_real_distribution<>>
            random_;
#else
        utils::Random<std::mt19937, std::uniform_real_distribution<>> random_;
#endif

    public:
        BottomHeavySelectionStrategy() : random_(0.0, 1.0) {}

        auto select(std::span<const T> cumulative_weights,
                    T total_weight) -> size_t override {
#ifdef ATOM_USE_BOOST
            T randomValue = std::sqrt(random_()) * total_weight;
#else
            T randomValue = std::sqrt(random_()) * total_weight;
#endif
#ifdef ATOM_USE_BOOST
            auto it =
                boost::range::upper_bound(cumulative_weights, randomValue);
#else
            auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
            return std::distance(cumulative_weights.begin(), it);
        }
    };

    class RandomSelectionStrategy : public SelectionStrategy {
    private:
#ifdef ATOM_USE_BOOST
        utils::Random<boost::random::mt19937,
                      boost::random::uniform_int_distribution<>>
            random_index_;
#else
        utils::Random<std::mt19937, std::uniform_int_distribution<>>
            random_index_;
#endif

    public:
        explicit RandomSelectionStrategy(size_t max_index)
#ifdef ATOM_USE_BOOST
            : random_index_(0, max_index - 1){}
#else
            : random_index_(0, max_index - 1) {
        }
#endif

              auto select(std::span<const T> /*cumulative_weights*/,
                          T /*total_weight*/) -> size_t override {
            return random_index_();
        }
    };

    class WeightedRandomSampler {
    public:
        auto sample(std::span<const T> weights,
                    size_t n) -> std::vector<size_t> {
            std::vector<size_t> indices(weights.size());
            std::iota(indices.begin(), indices.end(), 0);

#ifdef ATOM_USE_BOOST
            utils::Random<boost::random::mt19937,
                          boost::random::discrete_distribution<>>
                random(weights.begin(), weights.end());
#else
            std::discrete_distribution<> dist(weights.begin(), weights.end());
            std::random_device rd;
            std::mt19937 gen(rd());
#endif

            std::vector<size_t> results(n);
#ifdef ATOM_USE_BOOST
            std::generate(results.begin(), results.end(),
                          [&]() { return random(); });
#else
            std::generate(results.begin(), results.end(),
                          [&]() { return dist(gen); });
#endif

            return results;
        }
    };

private:
    std::vector<T> weights_;
    std::vector<T> cumulative_weights_;
    std::unique_ptr<SelectionStrategy> strategy_;

    void updateCumulativeWeights() {
        cumulative_weights_.resize(weights_.size());
#ifdef ATOM_USE_BOOST
        boost::range::partial_sum(weights_, cumulative_weights_);
#else
        std::exclusive_scan(weights_.begin(), weights_.end(),
                            cumulative_weights_.begin(), T{0});
#endif
    }

public:
    explicit WeightSelector(std::span<const T> input_weights,
                            std::unique_ptr<SelectionStrategy> custom_strategy =
                                std::make_unique<DefaultSelectionStrategy>())
        : weights_(input_weights.begin(), input_weights.end()),
          strategy_(std::move(custom_strategy)) {
        updateCumulativeWeights();
    }

    void setSelectionStrategy(std::unique_ptr<SelectionStrategy> new_strategy) {
        strategy_ = std::move(new_strategy);
    }

    auto select() -> size_t {
#ifdef ATOM_USE_BOOST
        T totalWeight = boost::accumulate(weights_, T{0});
#else
        T totalWeight = std::reduce(weights_.begin(), weights_.end());
#endif
        if (totalWeight <= T{0}) {
            THROW_RUNTIME_ERROR("Total weight must be greater than zero.");
        }
        return strategy_->select(cumulative_weights_, totalWeight);
    }

    auto selectMultiple(size_t n) -> std::vector<size_t> {
        std::vector<size_t> results;
        results.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            results.push_back(select());
        }
        return results;
    }

    void updateWeight(size_t index, T new_weight) {
        if (index >= weights_.size()) {
            throw std::out_of_range("Index out of range");
        }
        weights_[index] = new_weight;
        updateCumulativeWeights();
    }

    void addWeight(T new_weight) {
        weights_.push_back(new_weight);
        updateCumulativeWeights();
    }

    void removeWeight(size_t index) {
        if (index >= weights_.size()) {
            throw std::out_of_range("Index out of range");
        }
        weights_.erase(weights_.begin() + index);
        updateCumulativeWeights();
    }

    void normalizeWeights() {
#ifdef ATOM_USE_BOOST
        T sum = boost::accumulate(weights_, T{0});
#else
        T sum = std::reduce(weights_.begin(), weights_.end());
#endif
        if (sum > T{0}) {
#ifdef ATOM_USE_BOOST
            boost::transform(weights_, weights_.begin(),
                             [sum](T w) { return w / sum; });
#else
            std::ranges::transform(weights_, weights_.begin(),
                                   [sum](T w) { return w / sum; });
#endif
            updateCumulativeWeights();
        }
    }

    void applyFunctionToWeights(std::invocable<T> auto&& func) {
#ifdef ATOM_USE_BOOST
        boost::transform(weights_, weights_.begin(),
                         std::forward<decltype(func)>(func));
#else
        std::ranges::transform(weights_, weights_.begin(),
                               std::forward<decltype(func)>(func));
#endif
        updateCumulativeWeights();
    }

    void batchUpdateWeights(const std::vector<std::pair<size_t, T>>& updates) {
        for (const auto& [index, new_weight] : updates) {
            if (index >= weights_.size()) {
                throw std::out_of_range("Index out of range");
            }
            weights_[index] = new_weight;
        }
        updateCumulativeWeights();
    }

    [[nodiscard]] auto getWeight(size_t index) const -> std::optional<T> {
        if (index >= weights_.size()) {
            return std::nullopt;
        }
        return weights_[index];
    }

    [[nodiscard]] auto getMaxWeightIndex() const -> size_t {
#ifdef ATOM_USE_BOOST
        return std::distance(weights_.begin(),
                             boost::range::max_element(weights_));
#else
        return std::distance(weights_.begin(),
                             std::ranges::max_element(weights_));
#endif
    }

    [[nodiscard]] auto getMinWeightIndex() const -> size_t {
#ifdef ATOM_USE_BOOST
        return std::distance(weights_.begin(),
                             boost::range::min_element(weights_));
#else
        return std::distance(weights_.begin(),
                             std::ranges::min_element(weights_));
#endif
    }

    [[nodiscard]] auto size() const -> size_t { return weights_.size(); }

    [[nodiscard]] auto getWeights() const -> std::span<const T> {
        return weights_;
    }

    [[nodiscard]] auto getTotalWeight() const -> T {
#ifdef ATOM_USE_BOOST
        return boost::accumulate(weights_, T{0});
#else
        return std::reduce(weights_.begin(), weights_.end());
#endif
    }

    void resetWeights(const std::vector<T>& new_weights) {
        weights_ = new_weights;
        updateCumulativeWeights();
    }

    void scaleWeights(T factor) {
#ifdef ATOM_USE_BOOST
        boost::transform(weights_, weights_.begin(),
                         [factor](T w) { return w * factor; });
#else
        std::ranges::transform(weights_, weights_.begin(),
                               [factor](T w) { return w * factor; });
#endif
        updateCumulativeWeights();
    }

    [[nodiscard]] auto getAverageWeight() const -> T {
        if (weights_.empty()) {
            THROW_RUNTIME_ERROR("No weights available to calculate average.");
        }
        return getTotalWeight() / static_cast<T>(weights_.size());
    }

    void printWeights(std::ostream& oss) const {
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
        oss << std::format("[{:.2f}", weights_.front());
        for (auto it = weights_.begin() + 1; it != weights_.end(); ++it) {
            oss << std::format(", {:.2f}", *it);
        }
        oss << "]\n";
#endif
    }
};

template <Arithmetic T>
class TopHeavySelectionStrategy : public WeightSelector<T>::SelectionStrategy {
private:
#ifdef ATOM_USE_BOOST
    utils::Random<boost::random::mt19937,
                  boost::random::uniform_real_distribution<>>
        random_;
#else
    utils::Random<std::mt19937, std::uniform_real_distribution<>> random_;
#endif

public:
    TopHeavySelectionStrategy() : random_(0.0, 1.0) {}

    auto select(std::span<const T> cumulative_weights,
                T total_weight) -> size_t override {
        T randomValue = std::pow(random_(), 2) * total_weight;
#ifdef ATOM_USE_BOOST
        auto it = boost::range::upper_bound(cumulative_weights, randomValue);
#else
        auto it = std::ranges::upper_bound(cumulative_weights, randomValue);
#endif
        return std::distance(cumulative_weights.begin(), it);
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_WEIGHT_HPP