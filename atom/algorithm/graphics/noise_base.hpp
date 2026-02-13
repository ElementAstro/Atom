/*
 * noise_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Base class for noise generators with shared functionality

**************************************************/

#ifndef ATOM_ALGORITHM_GRAPHICS_NOISE_BASE_HPP
#define ATOM_ALGORITHM_GRAPHICS_NOISE_BASE_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <numeric>
#include <random>
#include <span>
#include <vector>

#include "../core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Base class for noise generators providing common functionality.
 *
 * This class provides shared utilities for noise generation algorithms
 * including permutation table management and common mathematical operations.
 */
class NoiseBase {
public:
    /**
     * @brief Construct noise generator with optional seed.
     * @param seed Random seed for permutation table initialization
     */
    explicit NoiseBase(u32 seed = std::default_random_engine::default_seed) {
        initializePermutationTable(seed);
    }

    virtual ~NoiseBase() = default;

    /**
     * @brief Re-seed the noise generator.
     * @param seed New random seed
     */
    void reseed(u32 seed) { initializePermutationTable(seed); }

protected:
    std::vector<i32> perm_;

    /**
     * @brief Initialize the permutation table with a seed.
     * @param seed Random seed
     */
    void initializePermutationTable(u32 seed) {
        perm_.resize(512);
        std::iota(perm_.begin(), perm_.begin() + 256, 0);

        std::default_random_engine engine(seed);
        std::ranges::shuffle(std::span(perm_.begin(), perm_.begin() + 256),
                             engine);

        // Duplicate for seamless wrapping
        std::ranges::copy(std::span(perm_.begin(), perm_.begin() + 256),
                          perm_.begin() + 256);
    }

    /**
     * @brief Fade function (smoothstep) for noise interpolation.
     * @param t Input value [0, 1]
     * @return Smoothed value
     */
    template <std::floating_point T>
    [[nodiscard]] static constexpr auto fade(T t) noexcept -> T {
        return t * t * t * (t * (t * T(6) - T(15)) + T(10));
    }

    /**
     * @brief Linear interpolation.
     * @param t Interpolation factor [0, 1]
     * @param a Start value
     * @param b End value
     * @return Interpolated value
     */
    template <std::floating_point T>
    [[nodiscard]] static constexpr auto lerp(T t, T a, T b) noexcept -> T {
        return a + t * (b - a);
    }

    /**
     * @brief 2D gradient dot product.
     * @param hash Hash value for gradient selection
     * @param x X coordinate
     * @param y Y coordinate
     * @return Dot product result
     */
    template <std::floating_point T>
    [[nodiscard]] static constexpr auto grad2D(i32 hash, T x,
                                               T y) noexcept -> T {
        i32 h = hash & 3;
        T u = h < 2 ? x : y;
        T v = h < 2 ? y : x;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    /**
     * @brief 3D gradient dot product.
     * @param hash Hash value for gradient selection
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @return Dot product result
     */
    template <std::floating_point T>
    [[nodiscard]] static constexpr auto grad3D(i32 hash, T x, T y,
                                               T z) noexcept -> T {
        i32 h = hash & 15;
        T u = h < 8 ? x : y;
        T v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    /**
     * @brief Generate fractal noise with multiple octaves.
     * @tparam NoiseFunc Callable that generates single-octave noise
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @param octaves Number of octaves
     * @param persistence Amplitude multiplier per octave
     * @param lacunarity Frequency multiplier per octave
     * @param noiseFunc The noise generation function
     * @return Fractal noise value
     */
    template <std::floating_point T, typename NoiseFunc>
    [[nodiscard]] auto fractal(T x, T y, T z, i32 octaves, T persistence,
                               T lacunarity, NoiseFunc noiseFunc) const -> T {
        T total = T(0);
        T frequency = T(1);
        T amplitude = T(1);
        T maxValue = T(0);

        for (i32 i = 0; i < octaves; ++i) {
            total += noiseFunc(x * frequency, y * frequency, z * frequency) *
                     amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxValue;
    }

    /**
     * @brief Generate a 2D noise map.
     * @tparam NoiseFunc Callable for noise generation
     * @param width Map width
     * @param height Map height
     * @param scale Coordinate scale factor
     * @param octaves Number of octaves
     * @param persistence Amplitude multiplier
     * @param lacunarity Frequency multiplier
     * @param seed Random seed for offset
     * @param noiseFunc The noise function to use
     * @return 2D vector of noise values
     */
    template <std::floating_point T, typename NoiseFunc>
    [[nodiscard]] auto generateNoiseMap2D(i32 width, i32 height, T scale,
                                          i32 octaves, T persistence,
                                          T lacunarity, i32 seed,
                                          NoiseFunc noiseFunc) const
        -> std::vector<std::vector<T>> {
        std::vector<std::vector<T>> noiseMap(height, std::vector<T>(width));
        std::default_random_engine prng(seed);
        std::uniform_real_distribution<T> dist(T(-10000), T(10000));
        T offsetX = dist(prng);
        T offsetY = dist(prng);

        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                T sampleX = (x - width / T(2) + offsetX) / scale;
                T sampleY = (y - height / T(2) + offsetY) / scale;
                noiseMap[y][x] = fractal(sampleX, sampleY, T(0), octaves,
                                         persistence, lacunarity, noiseFunc);
            }
        }

        return noiseMap;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_NOISE_BASE_HPP
