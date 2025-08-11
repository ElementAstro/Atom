#include "lcg.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef ATOM_USE_BOOST
#include <boost/math/special_functions.hpp>
#endif

namespace atom::utils {

thread_local bool LCG::has_cached_gaussian_ = false;
thread_local double LCG::cached_gaussian_value_ = 0.0;

LCG::LCG(result_type seed) noexcept : current_(seed) {
    spdlog::info("LCG initialized with seed: {}", seed);
}

auto LCG::next() noexcept -> result_type {
    // Parameters from "Numerical Recipes" for better quality
    constexpr result_type MULTIPLIER = 1664525;
    constexpr result_type INCREMENT = 1013904223;
    constexpr result_type MODULUS = 0xFFFFFFFF;  // 2^32

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = (MULTIPLIER * current_ + INCREMENT) &
               MODULUS;  // Bit-wise AND is faster than modulo
    return current_;
}

void LCG::seed(result_type new_seed) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = new_seed;
    spdlog::info("LCG reseeded with new seed: {}", new_seed);
    has_cached_gaussian_ = false;
}

void LCG::saveState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file for saving state: {}", filename);
            THROW_RUNTIME_ERROR("Failed to open file for saving state");
        }
        file.write(reinterpret_cast<char*>(&current_), sizeof(current_));
        spdlog::info("LCG state saved to file: {}", filename);
    } catch (const std::exception& e) {
        spdlog::error("Exception during save state: {}", e.what());
        throw;
    }
}

void LCG::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file for loading state: {}",
                          filename);
            THROW_RUNTIME_ERROR("Failed to open file for loading state");
        }
        file.read(reinterpret_cast<char*>(&current_), sizeof(current_));

        // Verify read was successful
        if (!file) {
            spdlog::error("Failed to read data from state file: {}", filename);
            THROW_RUNTIME_ERROR("Failed to read data from state file");
        }

        spdlog::info("LCG state loaded from file: {}", filename);
        has_cached_gaussian_ = false;
    } catch (const std::exception& e) {
        spdlog::error("Exception during load state: {}", e.what());
        throw;
    }
}

auto LCG::nextInt(int min, int max) -> int {
    if (min > max) {
        spdlog::error("Invalid argument: min ({}) > max ({})", min, max);
        THROW_INVALID_ARGUMENT("Min should be less than or equal to Max");
    }

    // For small ranges, this is more efficient than modulo
    if (max - min < 1000) {
        int result = min + static_cast<int>(
                               next() % (static_cast<uint64_t>(max) - min + 1));
        spdlog::trace("LCG generated next int: {} (range: [{}, {}])", result,
                      min, max);
        return result;
    }

    // For larger ranges, use floating-point method for better distribution
    double scaled = nextDouble();
    int result = min + static_cast<int>(scaled * (max - min + 1));

    // Ensure bounds (due to floating point precision issues)
    result = std::clamp(result, min, max);

    spdlog::trace("LCG generated next int: {} (range: [{}, {}])", result, min,
                  max);
    return result;
}

auto LCG::nextDouble(double min, double max) -> double {
    if (min >= max) {
        spdlog::error("Invalid argument: min ({}) >= max ({})", min, max);
        THROW_INVALID_ARGUMENT("Min should be less than Max");
    }

    // Use full 64-bit precision for better distribution
    constexpr double SCALE_FACTOR = 1.0 / (1ULL << 32);
    double uniform = static_cast<double>(next()) * SCALE_FACTOR;

    double result = min + uniform * (max - min);
    spdlog::trace("LCG generated next double: {} (range: [{}, {}])", result,
                  min, max);
    return result;
}

void LCG::validateProbability(double probability, bool allowZeroOne) {
    bool valid = (allowZeroOne) ? (probability >= 0.0 && probability <= 1.0)
                                : (probability > 0.0 && probability < 1.0);

    if (!valid) {
        spdlog::error("Invalid probability value: {}", probability);
        THROW_INVALID_ARGUMENT("Probability should be in range [0, 1]");
    }
}

auto LCG::nextBernoulli(double probability) -> bool {
    validateProbability(probability);
    bool result = nextDouble() < probability;
    spdlog::trace("LCG generated next Bernoulli: {} (probability: {})", result,
                  probability);
    return result;
}

auto LCG::nextGaussian(double mean, double stddev) -> double {
    if (stddev <= 0.0) {
        spdlog::error("Invalid standard deviation: {}", stddev);
        THROW_INVALID_ARGUMENT("Standard deviation must be positive");
    }

    // Use thread-local cache for better performance in multi-threaded contexts
    if (has_cached_gaussian_) {
        has_cached_gaussian_ = false;
        double result = cached_gaussian_value_ * stddev + mean;
        spdlog::trace(
            "LCG generated next Gaussian (cached): {} (mean: {}, stddev: {})",
            result, mean, stddev);
        return result;
    }

    // Box-Muller transform for Gaussian distribution
    double uniform1 = nextDouble();
    double uniform2 = nextDouble();

    // Guard against edge cases that might cause numerical issues
    if (uniform1 < std::numeric_limits<double>::min()) {
        uniform1 = std::numeric_limits<double>::min();
    }

    double radius = std::sqrt(-2.0 * std::log(uniform1));
    double theta = 2.0 * M_PI * uniform2;

    // Cache one value for future use
    cached_gaussian_value_ = radius * std::sin(theta);
    has_cached_gaussian_ = true;

    double result = radius * std::cos(theta) * stddev + mean;
    spdlog::trace("LCG generated next Gaussian: {} (mean: {}, stddev: {})",
                  result, mean, stddev);
    return result;
}

auto LCG::nextPoisson(double lambda) -> int {
    if (lambda <= 0.0) {
        spdlog::error("Invalid argument: lambda ({}) <= 0", lambda);
        THROW_INVALID_ARGUMENT("Lambda should be greater than 0");
    }

    // For small lambda, direct method is faster
    if (lambda < 30.0) {
        double expLambda = std::exp(-lambda);
        int count = 0;
        double product = 1.0;
        do {
            ++count;
            product *= nextDouble();
        } while (product > expLambda && count < 1000);

        int result = count - 1;
        spdlog::trace("LCG generated next Poisson: {} (lambda: {})", result,
                      lambda);
        return result;
    }

    // For larger lambda, use normal approximation with continuity correction
    double x = nextGaussian(lambda, std::sqrt(lambda));
    int result = std::max(0, static_cast<int>(std::floor(x + 0.5)));
    spdlog::trace("LCG generated next Poisson (approx): {} (lambda: {})",
                  result, lambda);
    return result;
}

auto LCG::nextExponential(double lambda) -> double {
    if (lambda <= 0.0) {
        spdlog::error("Invalid argument: lambda ({}) <= 0", lambda);
        THROW_INVALID_ARGUMENT("Lambda should be greater than 0");
    }

    // More robust implementation avoiding 1.0 edge case
    double u;
    do {
        u = nextDouble();
    } while (u == 0.0);

    double result = -std::log(u) / lambda;
    spdlog::trace("LCG generated next Exponential: {} (lambda: {})", result,
                  lambda);
    return result;
}

auto LCG::nextGeometric(double probability) -> int {
    validateProbability(probability, false);

    // More efficient algorithm for geometric distribution
    double u;
    do {
        u = nextDouble();
    } while (u == 0.0);  // Avoid log(0)

    int result =
        static_cast<int>(std::ceil(std::log(u) / std::log(1.0 - probability)));
    spdlog::trace("LCG generated next Geometric: {} (probability: {})", result,
                  probability);
    return result;
}

auto LCG::nextGamma(double shape, double scale) -> double {
    if (shape <= 0.0 || scale <= 0.0) {
        spdlog::error("Invalid argument: shape ({}) <= 0 or scale ({}) <= 0",
                      shape, scale);
        THROW_INVALID_ARGUMENT("Shape and scale must be greater than 0");
    }

    try {
        double result;

        // Marsaglia and Tsang's method for shape >= 1
        if (shape >= 1.0) {
            const double d = shape - 1.0 / 3.0;
            const double c = 1.0 / std::sqrt(9.0 * d);
            double x, v;
            while (true) {
                do {
                    x = nextGaussian();
                    v = 1.0 + c * x;
                } while (v <= 0.0);

                v = v * v * v;
                const double u = nextDouble();

                if (u < 1.0 - 0.0331 * x * x * x * x) {
                    result = d * v * scale;
                    break;
                }

                if (std::log(u) < 0.5 * x * x + d * (1.0 - v + std::log(v))) {
                    result = d * v * scale;
                    break;
                }
            }
        }
        // Use Ahrens-Dieter acceptance-rejection method for shape < 1
        else {
            const double b = (std::exp(1.0) + shape) / std::exp(1.0);
            double p, u, v;
            while (true) {
                u = nextDouble();
                p = b * u;

                if (p <= 1.0) {
                    v = std::pow(p, 1.0 / shape);
                } else {
                    v = -std::log((b - p) / shape);
                }

                const double u2 = nextDouble();

                if (p <= 1.0) {
                    if (u2 <= std::exp(-v)) {
                        result = v * scale;
                        break;
                    }
                } else {
                    if (u2 <= std::pow(v, shape - 1.0)) {
                        result = v * scale;
                        break;
                    }
                }
            }
        }

        spdlog::trace("LCG generated next Gamma: {} (shape: {}, scale: {})",
                      result, shape, scale);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in Gamma generation: {}", e.what());
        throw;
    }
}

auto LCG::nextBeta(double alpha, double beta) -> double {
    if (alpha <= 0.0 || beta <= 0.0) {
        spdlog::error("Invalid argument: alpha ({}) <= 0 or beta ({}) <= 0",
                      alpha, beta);
        THROW_INVALID_ARGUMENT("Alpha and Beta must be greater than 0");
    }

    try {
        // Special cases for improved performance
        if (alpha == 1.0 && beta == 1.0) {
            return nextDouble();
        }

        // Use ratio of Gamma variates
        double gammaAlpha = nextGamma(alpha, 1.0);
        double gammaBeta = nextGamma(beta, 1.0);

        double result = gammaAlpha / (gammaAlpha + gammaBeta);
        spdlog::trace("LCG generated next Beta: {} (alpha: {}, beta: {})",
                      result, alpha, beta);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in Beta generation: {}", e.what());
        throw;
    }
}

auto LCG::nextChiSquared(double degreesOfFreedom) -> double {
    if (degreesOfFreedom <= 0.0) {
        spdlog::error("Invalid degrees of freedom: {}", degreesOfFreedom);
        THROW_INVALID_ARGUMENT("Degrees of freedom must be positive");
    }

    double result = nextGamma(degreesOfFreedom / 2.0, 2.0);
    spdlog::trace("LCG generated next Chi-Squared: {} (degrees of freedom: {})",
                  result, degreesOfFreedom);
    return result;
}

auto LCG::nextHypergeometric(int total, int success, int draws) -> int {
    if (success > total || draws > total || success < 0 || draws < 0 ||
        total < 0) {
        spdlog::error(
            "Invalid parameters for hypergeometric distribution: total ({}), "
            "success ({}), draws ({})",
            total, success, draws);
        THROW_INVALID_ARGUMENT(
            "Invalid parameters for hypergeometric distribution");
    }

    try {
        // Optimization for edge cases
        if (draws == 0 || success == 0) {
            return 0;
        }

        if (draws == total) {
            return success;
        }

        // Use more efficient algorithm for large N
        if (total > 100 && draws > 10) {
            int successCount = 0;
            int remainingSuccess = success;
            int remainingTotal = total;

            for (int i = 0; i < draws; ++i) {
                double probability =
                    static_cast<double>(remainingSuccess) / remainingTotal;
                if (nextDouble() < probability) {
                    ++successCount;
                    --remainingSuccess;
                }
                --remainingTotal;
            }

            spdlog::trace(
                "LCG generated next Hypergeometric: {} (total: {}, success: "
                "{}, draws: {})",
                successCount, total, success, draws);
            return successCount;
        }
        // Use exact method for small N
        else {
            // Direct sampling from the hypergeometric PMF
            std::vector<double> pmf(std::min(draws, success) + 1);

            // Calculate PMF
            for (int k = 0; k <= std::min(draws, success); ++k) {
                // Compute binomial coefficients carefully to avoid overflow
                double logPmf = 0.0;

                // Log of C(success, k)
                for (int i = 0; i < k; ++i) {
                    logPmf += std::log(success - i);
                    logPmf -= std::log(i + 1);
                }

                // Log of C(total-success, draws-k)
                for (int i = 0; i < draws - k; ++i) {
                    logPmf += std::log(total - success - i);
                    logPmf -= std::log(i + 1);
                }

                // Log of 1/C(total, draws)
                for (int i = 0; i < draws; ++i) {
                    logPmf -= std::log(total - i);
                    logPmf += std::log(i + 1);
                }

                pmf[k] = std::exp(logPmf);
            }

            // Sample from PMF
            double u = nextDouble();
            double cumulative = 0.0;

            for (int k = 0; k <= std::min(draws, success); ++k) {
                cumulative += pmf[k];
                if (u <= cumulative) {
                    spdlog::trace(
                        "LCG generated next Hypergeometric: {} (total: {}, "
                        "success: {}, draws: {})",
                        k, total, success, draws);
                    return k;
                }
            }

            spdlog::trace(
                "LCG generated next Hypergeometric (fallback): {} (total: "
                "{}, success: {}, draws: {})",
                std::min(draws, success), total, success, draws);
            return std::min(draws, success);
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in Hypergeometric generation: {}", e.what());
        throw;
    }
}

auto LCG::nextDiscrete(std::span<const double> weights) -> int {
    if (weights.empty()) {
        spdlog::error("Empty weights vector provided to nextDiscrete");
        THROW_INVALID_ARGUMENT("Weights vector cannot be empty");
    }

    for (size_t i = 0; i < weights.size(); ++i) {
        if (weights[i] < 0.0) {
            spdlog::error("Negative weight found at index {}: {}", i,
                          weights[i]);
            THROW_INVALID_ARGUMENT("Weights must be non-negative");
        }
    }

    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (sum <= 0.0) {
        spdlog::error("Sum of weights is not positive: {}", sum);
        THROW_INVALID_ARGUMENT("Sum of weights must be positive");
    }

    double randValue = nextDouble(0.0, sum);
    double cumulative = 0.0;

    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
        if (randValue < cumulative) {
            spdlog::trace("LCG generated next Discrete: {} (weights index: {})",
                          static_cast<int>(i), i);
            return static_cast<int>(i);
        }
    }
    spdlog::trace("LCG generated next Discrete: {} (weights index: {})",
                  static_cast<int>(weights.size() - 1), weights.size() - 1);
    return static_cast<int>(weights.size() - 1);
}

auto LCG::nextMultinomial(int trials, std::span<const double> probabilities)
    -> std::vector<int> {
    std::vector<int> counts(probabilities.size(), 0);
    for (int i = 0; i < trials; ++i) {
        int idx = nextDiscrete(probabilities);
        counts[idx]++;
    }
    spdlog::trace(
        "LCG generated next Multinomial: trials ({}), probabilities size ({})",
        trials, probabilities.size());
    return counts;
}

}  // namespace atom::utils
