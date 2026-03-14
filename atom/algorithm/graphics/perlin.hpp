#ifndef ATOM_ALGORITHM_GRAPHICS_PERLIN_HPP
#define ATOM_ALGORITHM_GRAPHICS_PERLIN_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <memory>
#include <numeric>
#include <random>
#include <span>
#include <vector>

#include "../core/rust_numeric.hpp"
#include "noise_base.hpp"

#ifdef ATOM_USE_OPENCL
#include "perlin_opencl.hpp"
#endif

namespace atom::algorithm {

/**
 * @brief Perlin noise generator.
 *
 * Classic Perlin noise implementation with optional OpenCL acceleration.
 * OpenCL support is encapsulated in PerlinNoiseOpenCL (perlin_opencl.hpp).
 */
class PerlinNoise : public NoiseBase {
public:
    explicit PerlinNoise(u32 seed = std::default_random_engine::default_seed)
        : NoiseBase(seed) {
#ifdef ATOM_USE_OPENCL
        try {
            opencl_ = std::make_unique<PerlinNoiseOpenCL>();
        } catch (...) {
            opencl_ = nullptr;  // Fallback to CPU if OpenCL init fails
        }
#endif
    }

    ~PerlinNoise() override = default;

    template <std::floating_point T>
    [[nodiscard]] auto noise(T x, T y, T z) const -> T {
#ifdef ATOM_USE_OPENCL
        if (opencl_ && opencl_->isAvailable()) {
            return opencl_->computeNoise(x, y, z, perm_);
        }
#endif
        return noiseCPU(x, y, z);
    }

    template <std::floating_point T>
    [[nodiscard]] auto octaveNoise(T x, T y, T z, i32 octaves,
                                   T persistence) const -> T {
        T total = 0;
        T frequency = 1;
        T amplitude = 1;
        T maxValue = 0;

        for (i32 i = 0; i < octaves; ++i) {
            total +=
                noise(x * frequency, y * frequency, z * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= 2;
        }

        return total / maxValue;
    }

    [[nodiscard]] auto generateNoiseMap(
        i32 width, i32 height, f64 scale, i32 octaves, f64 persistence,
        f64 /*lacunarity*/, i32 seed = std::default_random_engine::default_seed)
        const -> std::vector<std::vector<f64>> {
        std::vector<std::vector<f64>> noiseMap(height, std::vector<f64>(width));
        std::default_random_engine prng(seed);
        std::uniform_real_distribution<f64> dist(-10000, 10000);
        f64 offsetX = dist(prng);
        f64 offsetY = dist(prng);

        for (i32 y = 0; y < height; ++y) {
            for (i32 x = 0; x < width; ++x) {
                f64 sampleX = (x - width / 2.0 + offsetX) / scale;
                f64 sampleY = (y - height / 2.0 + offsetY) / scale;
                noiseMap[y][x] =
                    octaveNoise(sampleX, sampleY, 0.0, octaves, persistence);
            }
        }

        return noiseMap;
    }

private:
    // Uses inherited perm_ from NoiseBase

#ifdef ATOM_USE_OPENCL
    std::unique_ptr<PerlinNoiseOpenCL> opencl_;
#endif

    template <std::floating_point T>
    [[nodiscard]] auto noiseCPU(T x, T y, T z) const -> T {
        // Find unit cube containing point
        i32 X = static_cast<i32>(std::floor(x)) & 255;
        i32 Y = static_cast<i32>(std::floor(y)) & 255;
        i32 Z = static_cast<i32>(std::floor(z)) & 255;

        // Find relative x, y, z of point in cube
        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        // Compute fade curves for each of x, y, z
#ifdef ATOM_USE_SIMD
        // SIMD-based fade function calculations
        __m256d xSimd = _mm256_set1_pd(x);
        __m256d ySimd = _mm256_set1_pd(y);
        __m256d zSimd = _mm256_set1_pd(z);

        __m256d uSimd =
            _mm256_mul_pd(xSimd, _mm256_sub_pd(xSimd, _mm256_set1_pd(15)));
        uSimd = _mm256_mul_pd(
            uSimd, _mm256_add_pd(_mm256_set1_pd(10),
                                 _mm256_mul_pd(xSimd, _mm256_set1_pd(6))));
        // Apply similar SIMD operations for v and w if needed
        __m256d vSimd =
            _mm256_mul_pd(ySimd, _mm256_sub_pd(ySimd, _mm256_set1_pd(15)));
        vSimd = _mm256_mul_pd(
            vSimd, _mm256_add_pd(_mm256_set1_pd(10),
                                 _mm256_mul_pd(ySimd, _mm256_set1_pd(6))));
        __m256d wSimd =
            _mm256_mul_pd(zSimd, _mm256_sub_pd(zSimd, _mm256_set1_pd(15)));
        wSimd = _mm256_mul_pd(
            wSimd, _mm256_add_pd(_mm256_set1_pd(10),
                                 _mm256_mul_pd(zSimd, _mm256_set1_pd(6))));
#else
        T u = fade(x);
        T v = fade(y);
        T w = fade(z);
#endif

        // Hash coordinates of the 8 cube corners
        i32 A = perm_[X] + Y;
        i32 AA = perm_[A] + Z;
        i32 AB = perm_[A + 1] + Z;
        i32 B = perm_[X + 1] + Y;
        i32 BA = perm_[B] + Z;
        i32 BB = perm_[B + 1] + Z;

        // Add blended results from 8 corners of cube
        T res = lerp(w,
                     lerp(v,
                          lerp(u, grad(perm_[AA], x, y, z),
                               grad(perm_[BA], x - 1, y, z)),
                          lerp(u, grad(perm_[AB], x, y - 1, z),
                               grad(perm_[BB], x - 1, y - 1, z))),
                     lerp(v,
                          lerp(u, grad(perm_[AA + 1], x, y, z - 1),
                               grad(perm_[BA + 1], x - 1, y, z - 1)),
                          lerp(u, grad(perm_[AB + 1], x, y - 1, z - 1),
                               grad(perm_[BB + 1], x - 1, y - 1, z - 1))));
        return (res + 1) / 2;  // Normalize to [0,1]
    }

    static constexpr auto fade(f64 t) noexcept -> f64 {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    static constexpr auto lerp(f64 t, f64 a, f64 b) noexcept -> f64 {
        return a + t * (b - a);
    }

    static constexpr auto grad(i32 hash, f64 x, f64 y, f64 z) noexcept -> f64 {
        i32 h = hash & 15;
        f64 u = h < 8 ? x : y;
        f64 v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_PERLIN_HPP
