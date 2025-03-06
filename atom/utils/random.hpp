/*
 * random.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-25

Description: Simple random number generator

**************************************************/

#ifndef ATOM_UTILS_RANDOM_HPP
#define ATOM_UTILS_RANDOM_HPP

#include <algorithm>
#include <concepts>
#include <iterator>
#include <random>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::utils {

template <typename T>
concept RandomEngine = requires(T engine) {
    typename T::result_type;
    { engine() } -> std::convertible_to<typename T::result_type>;
    { engine.min() } -> std::convertible_to<typename T::result_type>;
    { engine.max() } -> std::convertible_to<typename T::result_type>;
    { engine.seed() } -> std::same_as<void>;
};

template <typename T>
concept RandomDistribution = requires(T dist, std::mt19937& gen) {
    typename T::result_type;
    typename T::param_type;
    { dist(gen) } -> std::convertible_to<typename T::result_type>;
    { dist.param() } -> std::convertible_to<typename T::param_type>;
};

/**
 * @brief A template class that combines a random number engine and a
 * distribution.
 *
 * @tparam Engine A type that meets the requirements of
 * UniformRandomBitGenerator (e.g., std::mt19937).
 * @tparam Distribution A type of distribution (e.g.,
 * std::uniform_int_distribution).
 */
template <RandomEngine Engine, RandomDistribution Distribution>
class Random {
public:
    using EngineType = Engine;  ///< Public type alias for the engine.
    using DistributionType =
        Distribution;  ///< Public type alias for the distribution.
    using ResultType =
        typename DistributionType::result_type;  ///< Result type produced by
                                                 ///< the distribution.
    using ParamType =
        typename DistributionType::param_type;  ///< Parameter type for the
                                                ///< distribution.

private:
    EngineType engine_;              ///< Instance of the engine.
    DistributionType distribution_;  ///< Instance of the distribution.

public:
    /**
     * @brief Constructs a Random object with specified minimum and maximum
     * values for the distribution.
     *
     * @param min The minimum value of the distribution.
     * @param max The maximum value of the distribution.
     * @throws InvalidArgumentException if min > max
     */
    Random(ResultType min, ResultType max)
        : engine_(std::random_device{}()), distribution_(min, max) {
        if (min > max) {
            THROW_INVALID_ARGUMENT(
                "Minimum value must be less than or equal to maximum value.");
        }
    }

    /**
     * @brief Constructs a Random object with a seed and distribution
     * parameters.
     *
     * @param seed A seed value to initialize the engine.
     * @param args Arguments to initialize the distribution.
     */
    template <typename Seed, typename... Args>
    explicit Random(Seed&& seed, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<EngineType, Seed> &&
        std::is_nothrow_constructible_v<DistributionType, Args...>)
        : engine_(std::forward<Seed>(seed)),
          distribution_(std::forward<Args>(args)...) {}

    /**
     * @brief Re-seeds the engine.
     *
     * @param value The seed value (default is obtained from
     * std::random_device).
     */
    void seed(ResultType value = std::random_device{}()) noexcept {
        engine_.seed(value);
    }

    /**
     * @brief Generates a random value using the underlying distribution and
     * engine.
     *
     * @return A randomly generated value.
     */
    [[nodiscard]] auto operator()() noexcept -> ResultType {
        return distribution_(engine_);
    }

    /**
     * @brief Generates a random value using the underlying distribution and
     * engine, with specific parameters.
     *
     * @param parm Parameters for the distribution.
     * @return A randomly generated value.
     */
    [[nodiscard]] auto operator()(const ParamType& parm) noexcept
        -> ResultType {
        return distribution_(engine_, parm);
    }

    /**
     * @brief Fills a range with randomly generated values.
     *
     * @param range A range to fill with random values
     */
    template <std::ranges::range Range>
        requires std::assignable_from<std::ranges::range_value_t<Range>&,
                                      ResultType>
    void generate(Range&& range) noexcept {
        std::ranges::generate(range, [this]() { return (*this)(); });
    }

    /**
     * @brief Fills a range with randomly generated values.
     *
     * @param first An iterator pointing to the beginning of the range.
     * @param last An iterator pointing to the end of the range.
     */
    template <typename OutputIt>
    void generate(OutputIt first, OutputIt last) noexcept {
        std::generate(first, last, [this]() { return (*this)(); });
    }

    /**
     * @brief Creates a vector of randomly generated values.
     *
     * @param count The number of values to generate.
     * @return A vector containing randomly generated values.
     * @throws std::bad_alloc if memory allocation fails
     */
    [[nodiscard]] auto vector(size_t count) -> std::vector<ResultType> {
        if (count > std::vector<ResultType>().max_size()) {
            THROW_INVALID_ARGUMENT("Count exceeds maximum vector size");
        }

        std::vector<ResultType> vec;
        try {
            vec.reserve(count);
            std::generate_n(std::back_inserter(vec), count,
                            [this]() { return (*this)(); });
        } catch (const std::exception& e) {
            THROW_RUNTIME_ERROR("Failed to generate random vector: " +
                                std::string(e.what()));
        }
        return vec;
    }

    /**
     * @brief Sets parameters for the distribution.
     *
     * @param parm The new parameters for the distribution.
     */
    void param(const ParamType& parm) noexcept { distribution_.param(parm); }

    /**
     * @brief Accessor for the underlying engine.
     *
     * @return A reference to the engine.
     */
    [[nodiscard]] auto engine() noexcept -> EngineType& { return engine_; }

    /**
     * @brief Accessor for the underlying distribution.
     *
     * @return A reference to the distribution.
     */
    [[nodiscard]] auto distribution() noexcept -> DistributionType& {
        return distribution_;
    }

    /**
     * @brief Generate random values in a range
     *
     * @param count Number of values to generate
     * @param min Minimum value
     * @param max Maximum value
     * @return std::vector<ResultType> Vector of random values
     * @throws InvalidArgumentException if min > max or count exceeds limits
     */
    [[nodiscard]] static auto range(size_t count, ResultType min,
                                    ResultType max) -> std::vector<ResultType> {
        if (min > max) {
            THROW_INVALID_ARGUMENT(
                "Minimum value must be less than or equal to maximum value");
        }

        Random<Engine, Distribution> random(min, max);
        return random.vector(count);
    }
};

/**
 * @brief Generates a random string of specified length
 *
 * @param length Length of the string to generate
 * @param charset Optional character set to use (defaults to alphanumeric)
 * @return std::string Random string
 * @throws InvalidArgumentException if length is invalid
 */
[[nodiscard]] auto generateRandomString(
    int length, const std::string& charset = "") -> std::string;

/**
 * @brief Generate a cryptographically secure random string
 *
 * @param length Length of the string to generate
 * @return std::string Secure random string
 * @throws InvalidArgumentException if length is invalid
 */
[[nodiscard]] auto generateSecureRandomString(int length) -> std::string;

/**
 * @brief Shuffle elements in a container using a secure random generator
 *
 * @tparam Container Container type that supports std::ranges
 * @param container Container to shuffle
 */
template <std::ranges::random_access_range Container>
void secureShuffleRange(Container&& container) noexcept {
    std::random_device rd;
    std::mt19937_64 g(rd());
    std::ranges::shuffle(container, g);
}

}  // namespace atom::utils

#endif
