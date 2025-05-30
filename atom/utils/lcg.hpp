#ifndef ATOM_UTILS_LCG_HPP
#define ATOM_UTILS_LCG_HPP

#include <chrono>
#include <limits>
#include <mutex>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include "atom/error/exception.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atom::utils {

/**
 * @concept Arithmetic
 * @brief Concept for types that support arithmetic operations.
 */
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

/**
 * @class LCG
 * @brief Linear Congruential Generator for pseudo-random number generation with
 * C++20 features.
 *
 * This class implements a Linear Congruential Generator (LCG) which is a type
 * of pseudo-random number generator. It provides various methods to generate
 * random numbers following different distributions.
 */
class LCG {
public:
    using result_type = uint32_t;

    /**
     * @brief Constructs an LCG with an optional seed.
     * @param seed The initial seed value. Defaults to the current time since
     * epoch.
     */
    explicit LCG(result_type seed =
                     static_cast<result_type>(std::chrono::steady_clock::now()
                                                  .time_since_epoch()
                                                  .count())) noexcept;

    /**
     * @brief Generates the next random number in the sequence.
     * @return The next random number.
     */
    auto next() noexcept -> result_type;

    /**
     * @brief Seeds the generator with a new seed value.
     * @param new_seed The new seed value.
     */
    void seed(result_type new_seed) noexcept;

    /**
     * @brief Saves the current state of the generator to a file.
     * @param filename The name of the file to save the state to.
     * @throws std::runtime_error if file cannot be opened
     */
    void saveState(const std::string& filename);

    /**
     * @brief Loads the state of the generator from a file.
     * @param filename The name of the file to load the state from.
     * @throws std::runtime_error if file cannot be opened or is corrupt
     */
    void loadState(const std::string& filename);

    /**
     * @brief Generates a random integer within a specified range.
     * @param min The minimum value (inclusive). Defaults to 0.
     * @param max The maximum value (inclusive). Defaults to the maximum value
     * of int.
     * @return A random integer within the specified range.
     * @throws std::invalid_argument if min > max
     */
    auto nextInt(int min = 0, int max = std::numeric_limits<int>::max()) -> int;

    /**
     * @brief Generates a random double within a specified range.
     * @param min The minimum value (inclusive). Defaults to 0.0.
     * @param max The maximum value (exclusive). Defaults to 1.0.
     * @return A random double within the specified range.
     * @throws std::invalid_argument if min >= max
     */
    auto nextDouble(double min = 0.0, double max = 1.0) -> double;

    /**
     * @brief Validates that a probability value is in the range [0,1]
     * @param probability The probability value to validate
     * @param allowZeroOne Whether to allow 0.0 and 1.0 as valid values
     * @throws std::invalid_argument if probability is not in the valid range
     */
    void validateProbability(double probability, bool allowZeroOne = true);

    /**
     * @brief Generates a random boolean value based on a specified probability.
     * @param probability The probability of returning true. Defaults to 0.5.
     * @return A random boolean value.
     * @throws std::invalid_argument if probability is not in [0,1]
     */
    auto nextBernoulli(double probability = 0.5) -> bool;

    /**
     * @brief Generates a random number following a Gaussian (normal)
     * distribution.
     * @param mean The mean of the distribution. Defaults to 0.0.
     * @param stddev The standard deviation of the distribution. Defaults
     * to 1.0.
     * @return A random number following a Gaussian distribution.
     * @throws std::invalid_argument if stddev <= 0
     */
    auto nextGaussian(double mean = 0.0, double stddev = 1.0) -> double;

    /**
     * @brief Generates a random number following a Poisson distribution.
     * @param lambda The rate parameter (lambda) of the distribution. Defaults
     * to 1.0.
     * @return A random number following a Poisson distribution.
     * @throws std::invalid_argument if lambda <= 0
     */
    auto nextPoisson(double lambda = 1.0) -> int;

    /**
     * @brief Generates a random number following an Exponential distribution.
     * @param lambda The rate parameter (lambda) of the distribution. Defaults
     * to 1.0.
     * @return A random number following an Exponential distribution.
     * @throws std::invalid_argument if lambda <= 0
     */
    auto nextExponential(double lambda = 1.0) -> double;

    /**
     * @brief Generates a random number following a Geometric distribution.
     * @param probability The probability of success in each trial. Defaults to
     * 0.5.
     * @return A random number following a Geometric distribution.
     * @throws std::invalid_argument if probability not in (0,1)
     */
    auto nextGeometric(double probability = 0.5) -> int;

    /**
     * @brief Generates a random number following a Gamma distribution.
     * @param shape The shape parameter of the distribution.
     * @param scale The scale parameter of the distribution. Defaults to 1.0.
     * @return A random number following a Gamma distribution.
     * @throws std::invalid_argument if shape or scale <= 0
     */
    auto nextGamma(double shape, double scale = 1.0) -> double;

    /**
     * @brief Generates a random number following a Beta distribution.
     * @param alpha The alpha parameter of the distribution.
     * @param beta The beta parameter of the distribution.
     * @return A random number following a Beta distribution.
     * @throws std::invalid_argument if alpha or beta <= 0
     */
    auto nextBeta(double alpha, double beta) -> double;

    /**
     * @brief Generates a random number following a Chi-Squared distribution.
     * @param degreesOfFreedom The degrees of freedom of the distribution.
     * @return A random number following a Chi-Squared distribution.
     * @throws std::invalid_argument if degreesOfFreedom <= 0
     */
    auto nextChiSquared(double degreesOfFreedom) -> double;

    /**
     * @brief Generates a random number following a Hypergeometric distribution.
     * @param total The total number of items.
     * @param success The number of successful items.
     * @param draws The number of draws.
     * @return A random number following a Hypergeometric distribution.
     * @throws std::invalid_argument if parameters are invalid
     */
    auto nextHypergeometric(int total, int success, int draws) -> int;

    /**
     * @brief Generates a random index based on a discrete distribution.
     * @param weights The weights of the discrete distribution.
     * @return A random index based on the weights.
     * @throws std::invalid_argument if weights is empty or contains negative
     * values
     */
    auto nextDiscrete(std::span<const double> weights) -> int;

    /**
     * @brief Generates a multinomial distribution.
     * @param trials The number of trials.
     * @param probabilities The probabilities of each outcome.
     * @return A vector of counts for each outcome.
     * @throws std::invalid_argument if probabilities is invalid
     */
    auto nextMultinomial(int trials, std::span<const double> probabilities)
        -> std::vector<int>;

    /**
     * @brief Shuffles a range of data.
     * @tparam Range Type that satisfies std::ranges::random_access_range
     * @param data The range of data to shuffle.
     */
    template <std::ranges::random_access_range Range>
    void shuffle(Range&& data) noexcept;

    /**
     * @brief Samples a subset of data from a range.
     * @tparam T Element type
     * @param data The vector of data to sample from.
     * @param sampleSize The number of elements to sample.
     * @return A vector containing the sampled elements.
     * @throws std::invalid_argument if sampleSize > data.size()
     */
    template <typename T>
    auto sample(const std::vector<T>& data, int sampleSize) -> std::vector<T>;

private:
    result_type current_;
    std::mutex mutex_;

    static thread_local bool has_cached_gaussian_;
    static thread_local double cached_gaussian_value_;

    static constexpr result_type MULTIPLIER = 1664525;
    static constexpr result_type INCREMENT = 1013904223;
    static constexpr result_type MODULUS = 0xFFFFFFFF;
    static constexpr double SCALE_FACTOR = 1.0 / (1ULL << 32);
};

template <std::ranges::random_access_range Range>
void LCG::shuffle(Range&& data) noexcept {
    std::lock_guard lock(mutex_);
    const auto size = static_cast<int>(std::ranges::size(data));

    for (int i = size - 1; i > 0; --i) {
        std::swap(data[i], data[nextInt(0, i)]);
    }
}

template <typename T>
auto LCG::sample(const std::vector<T>& data, int sampleSize) -> std::vector<T> {
    if (sampleSize > static_cast<int>(data.size())) {
        THROW_INVALID_ARGUMENT(
            "Sample size cannot be greater than the size of the input data");
    }
    std::vector<T> result = data;
    shuffle(result);
    return std::vector<T>(result.begin(), result.begin() + sampleSize);
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_LCG_HPP
