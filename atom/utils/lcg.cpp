#include "lcg.hpp"

#include <cmath>
#include <fstream>
#include <numeric>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/math/special_functions.hpp>
#endif

namespace atom::utils {

LCG::LCG(result_type seed) : current_(seed) {
    LOG_F(INFO, "LCG initialized with seed: %u", seed);
}

auto LCG::next() -> result_type {
    constexpr result_type MULTIPLIER = 1664525;
    constexpr result_type INCREMENT = 1013904223;
    constexpr result_type MODULUS = 0xFFFFFFFF;  // 2^32

    std::lock_guard<std::mutex> lock(mutex_);
    current_ = (MULTIPLIER * current_ + INCREMENT) % MODULUS;
    LOG_F(INFO, "LCG generated next value: %u", current_);
    return current_;
}

void LCG::seed(result_type new_seed) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = new_seed;
    LOG_F(INFO, "LCG reseeded with new seed: %u", new_seed);
}

void LCG::saveState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
#ifdef ATOM_USE_BOOST
        LOG_F(ERROR, "Failed to open file for saving state: {}", filename);
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to open file for saving state"));
#else
        LOG_F(ERROR, "Failed to open file for saving state: {}", filename);
        THROW_RUNTIME_ERROR("Failed to open file for saving state");
#endif
    }
    file.write(reinterpret_cast<char*>(&current_), sizeof(current_));
    LOG_F(INFO, "LCG state saved to file: {}", filename);
}

void LCG::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
#ifdef ATOM_USE_BOOST
        LOG_F(ERROR, "Failed to open file for loading state: {}", filename);
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to open file for loading state"));
#else
        LOG_F(ERROR, "Failed to open file for loading state: {}", filename);
        THROW_RUNTIME_ERROR("Failed to open file for loading state");
#endif
    }
    file.read(reinterpret_cast<char*>(&current_), sizeof(current_));
    LOG_F(INFO, "LCG state loaded from file: {}", filename);
}

auto LCG::nextInt(int min, int max) -> int {
    if (min > max) {
        LOG_F(ERROR, "Invalid argument: min ({}) > max ({})", min, max);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Min should be less than or equal to Max"));
#else
        THROW_INVALID_ARGUMENT("Min should be less than or equal to Max");
#endif
    }
    int result = min + static_cast<int>(next() % (max - min + 1));
    LOG_F(INFO, "LCG generated next int: {} (range: [{}, {}])", result, min, max);
    return result;
}

auto LCG::nextDouble(double min, double max) -> double {
    if (min >= max) {
        LOG_F(ERROR, "Invalid argument: min ({}) >= max ({})", min, max);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Min should be less than Max"));
#else
        THROW_INVALID_ARGUMENT("Min should be less than Max");
#endif
    }
    constexpr double MAX_UINT32 = 0xFFFFFFFF;
#ifdef ATOM_USE_BOOST
    double result = min + boost::math::nextafter(current_ / static_cast<double>(MAX_UINT32), 1.0) * (max - min);
#else
    double result = min + static_cast<double>(next()) / MAX_UINT32 * (max - min);
#endif
    LOG_F(INFO, "LCG generated next double: {} (range: [{}, {}])", result, min, max);
    return result;
}

auto LCG::nextBernoulli(double probability) -> bool {
    if (probability < 0.0 || probability > 1.0) {
        LOG_F(ERROR, "Invalid argument: probability ({}) out of range [0, 1]", probability);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Probability should be in range [0, 1]"));
#else
        THROW_INVALID_ARGUMENT("Probability should be in range [0, 1]");
#endif
    }
#ifdef ATOM_USE_BOOST
    bool result = nextDouble() < probability;
#else
    bool result = nextDouble() < probability;
#endif
    LOG_F(INFO, "LCG generated next Bernoulli: {} (probability: {})", result ? "true" : "false", probability);
    return result;
}

auto LCG::nextGaussian(double mean, double stddev) -> double {
    static bool hasCachedValue = false;
    static double cachedValue;
    if (hasCachedValue) {
        hasCachedValue = false;
        double result = cachedValue * stddev + mean;
        LOG_F(INFO, "LCG generated next Gaussian (cached): {} (mean: {}, stddev: {})", result, mean, stddev);
        return result;
    }
    double uniform1 = nextDouble(0.0, 1.0);
    double uniform2 = nextDouble(0.0, 1.0);
#ifdef ATOM_USE_BOOST
    double radius = std::sqrt(-2.0 * boost::math::log(uniform1));
    double theta = 2.0 * boost::math::constants::pi<double>() * uniform2;
#else
    double radius = std::sqrt(-2.0 * std::log(uniform1));
    double theta = 2.0 * M_PI * uniform2;
#endif
    cachedValue = radius * std::sin(theta);
    hasCachedValue = true;
    double result = radius * std::cos(theta) * stddev + mean;
    LOG_F(INFO, "LCG generated next Gaussian: {} (mean: {}, stddev: {})", result, mean, stddev);
    return result;
}

auto LCG::nextPoisson(double lambda) -> int {
    if (lambda <= 0.0) {
        LOG_F(ERROR, "Invalid argument: lambda ({}) <= 0", lambda);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Lambda should be greater than 0"));
#else
        THROW_INVALID_ARGUMENT("Lambda should be greater than 0");
#endif
    }
    double expLambda = std::exp(-lambda);
    int count = 0;
    double product = 1.0;
    do {
        ++count;
        product *= nextDouble();
    } while (product > expLambda);
    int result = count - 1;
    LOG_F(INFO, "LCG generated next Poisson: {} (lambda: {})", result, lambda);
    return result;
}

auto LCG::nextExponential(double lambda) -> double {
    if (lambda <= 0.0) {
        LOG_F(ERROR, "Invalid argument: lambda ({}) <= 0", lambda);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Lambda should be greater than 0"));
#else
        THROW_INVALID_ARGUMENT("Lambda should be greater than 0");
#endif
    }
#ifdef ATOM_USE_BOOST
    double result = -boost::math::log1p(-nextDouble()) / lambda;
#else
    double result = -std::log(1.0 - nextDouble()) / lambda;
#endif
    LOG_F(INFO, "LCG generated next Exponential: {} (lambda: {})", result, lambda);
    return result;
}

auto LCG::nextGeometric(double probability) -> int {
    if (probability <= 0.0 || probability >= 1.0) {
        LOG_F(ERROR, "Invalid argument: probability ({}) out of range (0, 1)", probability);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Probability should be in range (0, 1)"));
#else
        THROW_INVALID_ARGUMENT("Probability should be in range (0, 1)");
#endif
    }
#ifdef ATOM_USE_BOOST
    int result = static_cast<int>(std::ceil(boost::math::log1p(-nextDouble()) / boost::math::log1p(-probability)));
#else
    int result = static_cast<int>(std::ceil(std::log(1.0 - nextDouble()) / std::log(1.0 - probability)));
#endif
    LOG_F(INFO, "LCG generated next Geometric: {} (probability: {})", result, probability);
    return result;
}

auto LCG::nextGamma(double shape, double scale) -> double {
    if (shape <= 0.0 || scale <= 0.0) {
        LOG_F(ERROR, "Invalid argument: shape ({}) <= 0 or scale ({}) <= 0", shape, scale);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Shape and scale must be greater than 0"));
#else
        THROW_INVALID_ARGUMENT("Shape and scale must be greater than 0");
#endif
    }
    if (shape < 1.0) {
        double result;
#ifdef ATOM_USE_BOOST
        result = nextGamma(1.0 + shape, scale) * std::pow(nextDouble(), 1.0 / shape);
#else
        result = nextGamma(1.0 + shape, scale) * std::pow(nextDouble(), 1.0 / shape);
#endif
        LOG_F(INFO, "LCG generated next Gamma (shape < 1): {} (shape: {}, scale: {})", result, shape, scale);
        return result;
    }
    constexpr double MAGIC_NUMBER_3 = 3.0;
    constexpr double MAGIC_NUMBER_9 = 9.0;
    constexpr double MAGIC_NUMBER_0_0331 = 0.0331;
    double d = shape - 1.0 / MAGIC_NUMBER_3;
    double c = 1.0 / std::sqrt(MAGIC_NUMBER_9 * d);
    double x;
    double v;
    do {
        do {
#ifdef ATOM_USE_BOOST
            x = nextGaussian(0.0, 1.0);
#else
            x = nextGaussian(0.0, 1.0);
#endif
            v = 1.0 + c * x;
        } while (v <= 0);
        v = v * v * v;
#ifdef ATOM_USE_BOOST
    } while (nextDouble() > (1.0 - MAGIC_NUMBER_0_0331 * (x * x) * (x * x)) &&
             std::log(nextDouble()) >
                 0.5 * x * x + d * (1.0 - v + std::log(v)));
#else
    } while (nextDouble() > (1.0 - MAGIC_NUMBER_0_0331 * (x * x) * (x * x)) &&
             std::log(nextDouble()) >
                 0.5 * x * x + d * (1.0 - v + std::log(v)));
#endif
    double result = d * v * scale;
    LOG_F(INFO, "LCG generated next Gamma: {} (shape: {}, scale: {})", result, shape, scale);
    return result;
}

auto LCG::nextBeta(double alpha, double beta) -> double {
    if (alpha <= 0.0 || beta <= 0.0) {
        LOG_F(ERROR, "Invalid argument: alpha ({}) <= 0 or beta ({}) <= 0", alpha, beta);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Alpha and Beta must be greater than 0"));
#else
        THROW_INVALID_ARGUMENT("Alpha and Beta must be greater than 0");
#endif
    }
#ifdef ATOM_USE_BOOST
    double gammaAlpha = nextGamma(alpha, 1.0);
    double gammaBeta = nextGamma(beta, 1.0);
#else
    double gammaAlpha = nextGamma(alpha, 1.0);
    double gammaBeta = nextGamma(beta, 1.0);
#endif
    double result = gammaAlpha / (gammaAlpha + gammaBeta);
    LOG_F(INFO, "LCG generated next Beta: {} (alpha: {}, beta: {})", result, alpha, beta);
    return result;
}

auto LCG::nextChiSquared(double degreesOfFreedom) -> double {
#ifdef ATOM_USE_BOOST
    double result = nextGamma(degreesOfFreedom / 2.0, 2.0);
#else
    double result = nextGamma(degreesOfFreedom / 2.0, 2.0);
#endif
    LOG_F(INFO, "LCG generated next Chi-Squared: {} (degrees of freedom: {})", result, degreesOfFreedom);
    return result;
}

auto LCG::nextHypergeometric(int total, int success, int draws) -> int {
    if (success > total || draws > total || success < 0 || draws < 0) {
        LOG_F(ERROR, "Invalid parameters for hypergeometric distribution: total ({}), success ({}), draws ({})", total, success, draws);
#ifdef ATOM_USE_BOOST
        BOOST_THROW_EXCEPTION(std::invalid_argument("Invalid parameters for hypergeometric distribution"));
#else
        THROW_INVALID_ARGUMENT("Invalid parameters for hypergeometric distribution");
#endif
    }

    int successCount = 0;
    int remainingSuccess = success;
    int remainingTotal = total;

    for (int i = 0; i < draws; ++i) {
#ifdef ATOM_USE_BOOST
        double probability = static_cast<double>(remainingSuccess) / remainingTotal;
#else
        double probability = static_cast<double>(remainingSuccess) / remainingTotal;
#endif
        if (nextDouble(0.0, 1.0) < probability) {
            ++successCount;
            --remainingSuccess;
        }
        --remainingTotal;
    }
    LOG_F(INFO, "LCG generated next Hypergeometric: {} (total: {}, success: {}, draws: {})", successCount, total, success, draws);
    return successCount;
}

auto LCG::nextDiscrete(const std::vector<double>& weights) -> int {
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    double randValue = nextDouble(0.0, sum);
    double cumulative = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        cumulative += weights[i];
#ifdef ATOM_USE_BOOST
        if (randValue < cumulative) {
            LOG_F(INFO, "LCG generated next Discrete: {} (weights index: %zu)", static_cast<int>(i), i);
            return static_cast<int>(i);
        }
#else
        if (randValue < cumulative) {
            LOG_F(INFO, "LCG generated next Discrete: {} (weights index: %zu)", static_cast<int>(i), i);
            return static_cast<int>(i);
        }
#endif
    }
    LOG_F(INFO, "LCG generated next Discrete: {} (weights index: %zu)", static_cast<int>(weights.size() - 1), weights.size() - 1);
    return static_cast<int>(weights.size() - 1);
}

auto LCG::nextMultinomial(int trials, const std::vector<double>& probabilities) -> std::vector<int> {
    std::vector<int> counts(probabilities.size(), 0);
    for (int i = 0; i < trials; ++i) {
#ifdef ATOM_USE_BOOST
        int idx = nextDiscrete(probabilities);
#else
        int idx = nextDiscrete(probabilities);
#endif
        counts[idx]++;
    }
    LOG_F(INFO, "LCG generated next Multinomial: trials ({}), probabilities size (%zu)", trials, probabilities.size());
    return counts;
}

}  // namespace atom::utils