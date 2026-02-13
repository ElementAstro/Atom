#ifndef ATOM_ALGORITHM_OPTIMIZATION_TSP_HPP
#define ATOM_ALGORITHM_OPTIMIZATION_TSP_HPP

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#ifdef ATOM_USE_SIMD
#ifdef __x86_64__
#include <immintrin.h>
#elif __aarch64__
#include <arm_neon.h>
#endif
#endif

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#endif

#include "spdlog/spdlog.h"

// Example TSP (Traveling Salesman Problem) implementation
class TSP {
private:
    std::vector<std::pair<double, double>> cities_;

public:
    explicit TSP(const std::vector<std::pair<double, double>>& cities);

    [[nodiscard]] auto energy(const std::vector<int>& solution) const -> double;

    [[nodiscard]] static auto neighbor(const std::vector<int>& solution)
        -> std::vector<int>;

    [[nodiscard]] auto randomSolution() const -> std::vector<int>;
};

inline TSP::TSP(const std::vector<std::pair<double, double>>& cities)
    : cities_(cities) {
    spdlog::info("TSP instance created with {} cities.", cities_.size());
}

inline auto TSP::energy(const std::vector<int>& solution) const -> double {
    double totalDistance = 0.0;
    size_t numCities = solution.size();

#ifdef ATOM_USE_SIMD
#ifdef __AVX2__
    // AVX2 implementation
    __m256d totalDistanceVec = _mm256_setzero_pd();

    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        __m256d v1 = _mm256_set_pd(0.0, 0.0, y1, x1);
        __m256d v2 = _mm256_set_pd(0.0, 0.0, y2, x2);
        __m256d diff = _mm256_sub_pd(v1, v2);
        __m256d squared = _mm256_mul_pd(diff, diff);

        // Extract x^2 and y^2
        __m128d low = _mm256_extractf128_pd(squared, 0);
        double dx_squared = _mm_cvtsd_f64(low);
        double dy_squared = _mm_cvtsd_f64(_mm_permute_pd(low, 1));

        // Calculate distance and add to total
        double distance = std::sqrt(dx_squared + dy_squared);
        totalDistance += distance;
    }

#elif defined(__ARM_NEON)
    // ARM NEON implementation
    float32x4_t totalDistanceVec = vdupq_n_f32(0.0f);

    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        float32x2_t p1 =
            vset_f32(static_cast<float>(x1), static_cast<float>(y1));
        float32x2_t p2 =
            vset_f32(static_cast<float>(x2), static_cast<float>(y2));

        float32x2_t diff = vsub_f32(p1, p2);
        float32x2_t squared = vmul_f32(diff, diff);

        // Sum x^2 + y^2 and take sqrt
        float sum = vget_lane_f32(vpadd_f32(squared, squared), 0);
        totalDistance += std::sqrt(static_cast<double>(sum));
    }

#else
    // Fallback SIMD implementation for other architectures
    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance += std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }
#endif
#else
    // Standard optimized implementation
    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance += std::hypot(deltaX, deltaY);
    }
#endif

    return totalDistance;
}

inline auto TSP::neighbor(const std::vector<int>& solution)
    -> std::vector<int> {
    std::vector<int> newSolution = solution;
    try {
#ifdef ATOM_USE_BOOST
        boost::random::random_device randomDevice;
        boost::random::mt19937 generator(randomDevice());
        boost::random::uniform_int_distribution<int> distribution(
            0, static_cast<int>(solution.size()) - 1);
#else
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_int_distribution<int> distribution(
            0, static_cast<int>(solution.size()) - 1);
#endif
        int index1 = distribution(generator);
        int index2 = distribution(generator);
        std::swap(newSolution[index1], newSolution[index2]);
        spdlog::info(
            "Generated neighbor solution by swapping indices {} and {}.",
            index1, index2);
    } catch (const std::exception& e) {
        spdlog::error("Exception in TSP::neighbor: {}", e.what());
        throw;
    }
    return newSolution;
}

inline auto TSP::randomSolution() const -> std::vector<int> {
    std::vector<int> solution(cities_.size());
    std::iota(solution.begin(), solution.end(), 0);
    try {
#ifdef ATOM_USE_BOOST
        boost::random::random_device randomDevice;
        boost::random::mt19937 generator(randomDevice());
        boost::range::random_shuffle(solution, generator);
#else
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::ranges::shuffle(solution, generator);
#endif
        spdlog::info("Generated random solution.");
    } catch (const std::exception& e) {
        spdlog::error("Exception in TSP::randomSolution: {}", e.what());
        throw;
    }
    return solution;
}

#endif  // ATOM_ALGORITHM_OPTIMIZATION_TSP_HPP
