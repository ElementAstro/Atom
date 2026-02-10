#ifndef ATOM_ALGORITHM_GRAPHICS_SIMPLEX_HPP
#define ATOM_ALGORITHM_GRAPHICS_SIMPLEX_HPP

#include <array>
#include <cmath>
#include <concepts>
#include <numeric>
#include <random>
#include <vector>

#include "../rust_numeric.hpp"
#include "noise_base.hpp"

namespace atom::algorithm {

/**
 * @brief Simplex noise generator - an improved version of Perlin noise
 *
 * Simplex noise has several advantages over Perlin noise:
 * - Lower computational complexity (O(n) vs O(n²))
 * - Better visual isotropy (no directional artifacts)
 * - Higher dimensional scalability
 * - More natural-looking results
 */
class SimplexNoise : public NoiseBase {
public:
    /**
     * @brief Construct a new Simplex Noise generator
     * @param seed Random seed for permutation table
     */
    explicit SimplexNoise(u32 seed = std::default_random_engine::default_seed)
        : NoiseBase(seed) {
        // Initialize gradient tables using inherited perm_
        for (usize i = 0; i < 256; ++i) {
            grad2_[i] = GRAD2[perm_[i] % 8];
        }

        for (usize i = 0; i < 256; ++i) {
            grad3_[i] = GRAD3[perm_[i] % 12];
        }
    }

    /**
     * @brief Generate 2D simplex noise
     * @param x X coordinate
     * @param y Y coordinate
     * @return Noise value in range [-1, 1]
     */
    template <std::floating_point T>
    [[nodiscard]] auto noise2D(T x, T y) const noexcept -> T {
        // Skew the input space to determine which simplex cell we're in
        constexpr T F2 = T(0.5) * (std::sqrt(T(3)) - T(1));
        T s = (x + y) * F2;
        i32 i = static_cast<i32>(std::floor(x + s));
        i32 j = static_cast<i32>(std::floor(y + s));

        // Unskew the cell origin back to (x,y) space
        constexpr T G2 = (T(3) - std::sqrt(T(3))) / T(6);
        T t = (i + j) * G2;
        T X0 = i - t;
        T Y0 = j - t;
        T x0 = x - X0;
        T y0 = y - Y0;

        // Determine which simplex we are in
        i32 i1, j1;
        if (x0 > y0) {
            i1 = 1;
            j1 = 0;  // Lower triangle, XY order: (0,0)->(1,0)->(1,1)
        } else {
            i1 = 0;
            j1 = 1;  // Upper triangle, YX order: (0,0)->(0,1)->(1,1)
        }

        // Offsets for second (middle) corner of simplex in (x,y) unskewed
        // coords
        T x1 = x0 - i1 + G2;
        T y1 = y0 - j1 + G2;
        // Offsets for last corner of simplex in (x,y) unskewed coords
        T x2 = x0 - T(1) + T(2) * G2;
        T y2 = y0 - T(1) + T(2) * G2;

        // Work out the hashed gradient indices of the three simplex corners
        i32 ii = i & 255;
        i32 jj = j & 255;
        i32 gi0 = perm_[ii + perm_[jj]] % 8;
        i32 gi1 = perm_[ii + i1 + perm_[jj + j1]] % 8;
        i32 gi2 = perm_[ii + 1 + perm_[jj + 1]] % 8;

        // Calculate the contribution from the three corners
        T n0, n1, n2;

        T t0 = T(0.5) - x0 * x0 - y0 * y0;
        if (t0 < 0) {
            n0 = 0;
        } else {
            t0 *= t0;
            n0 = t0 * t0 * dot(GRAD2[gi0], x0, y0);
        }

        T t1 = T(0.5) - x1 * x1 - y1 * y1;
        if (t1 < 0) {
            n1 = 0;
        } else {
            t1 *= t1;
            n1 = t1 * t1 * dot(GRAD2[gi1], x1, y1);
        }

        T t2 = T(0.5) - x2 * x2 - y2 * y2;
        if (t2 < 0) {
            n2 = 0;
        } else {
            t2 *= t2;
            n2 = t2 * t2 * dot(GRAD2[gi2], x2, y2);
        }

        // Add contributions from each corner to get the final noise value
        return T(70) * (n0 + n1 + n2);
    }

    /**
     * @brief Generate 3D simplex noise
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate
     * @return Noise value in range [-1, 1]
     */
    template <std::floating_point T>
    [[nodiscard]] auto noise3D(T x, T y, T z) const noexcept -> T {
        // Skew the input space to determine which simplex cell we're in
        constexpr T F3 = T(1) / T(3);
        T s = (x + y + z) * F3;
        i32 i = static_cast<i32>(std::floor(x + s));
        i32 j = static_cast<i32>(std::floor(y + s));
        i32 k = static_cast<i32>(std::floor(z + s));

        // Unskew the cell origin back to (x,y,z) space
        constexpr T G3 = T(1) / T(6);
        T t = (i + j + k) * G3;
        T X0 = i - t;
        T Y0 = j - t;
        T Z0 = k - t;
        T x0 = x - X0;
        T y0 = y - Y0;
        T z0 = z - Z0;

        // Determine which simplex we are in
        i32 i1, j1, k1, i2, j2, k2;
        if (x0 >= y0) {
            if (y0 >= z0) {
                i1 = 1;
                j1 = 0;
                k1 = 0;
                i2 = 1;
                j2 = 1;
                k2 = 0;
            } else if (x0 >= z0) {
                i1 = 1;
                j1 = 0;
                k1 = 0;
                i2 = 1;
                j2 = 0;
                k2 = 1;
            } else {
                i1 = 0;
                j1 = 0;
                k1 = 1;
                i2 = 1;
                j2 = 0;
                k2 = 1;
            }
        } else {
            if (y0 < z0) {
                i1 = 0;
                j1 = 0;
                k1 = 1;
                i2 = 0;
                j2 = 1;
                k2 = 1;
            } else if (x0 < z0) {
                i1 = 0;
                j1 = 1;
                k1 = 0;
                i2 = 0;
                j2 = 1;
                k2 = 1;
            } else {
                i1 = 0;
                j1 = 1;
                k1 = 0;
                i2 = 1;
                j2 = 1;
                k2 = 0;
            }
        }

        // Offsets for second corner of simplex in (x,y,z) coords
        T x1 = x0 - i1 + G3;
        T y1 = y0 - j1 + G3;
        T z1 = z0 - k1 + G3;
        // Offsets for third corner of simplex in (x,y,z) coords
        T x2 = x0 - i2 + T(2) * G3;
        T y2 = y0 - j2 + T(2) * G3;
        T z2 = z0 - k2 + T(2) * G3;
        // Offsets for last corner of simplex in (x,y,z) coords
        T x3 = x0 - T(1) + T(3) * G3;
        T y3 = y0 - T(1) + T(3) * G3;
        T z3 = z0 - T(1) + T(3) * G3;

        // Work out the hashed gradient indices of the four simplex corners
        i32 ii = i & 255;
        i32 jj = j & 255;
        i32 kk = k & 255;
        i32 gi0 = perm_[ii + perm_[jj + perm_[kk]]] % 12;
        i32 gi1 = perm_[ii + i1 + perm_[jj + j1 + perm_[kk + k1]]] % 12;
        i32 gi2 = perm_[ii + i2 + perm_[jj + j2 + perm_[kk + k2]]] % 12;
        i32 gi3 = perm_[ii + 1 + perm_[jj + 1 + perm_[kk + 1]]] % 12;

        // Calculate the contribution from the four corners
        T n0, n1, n2, n3;

        T t0 = T(0.6) - x0 * x0 - y0 * y0 - z0 * z0;
        if (t0 < 0) {
            n0 = 0;
        } else {
            t0 *= t0;
            n0 = t0 * t0 * dot(GRAD3[gi0], x0, y0, z0);
        }

        T t1 = T(0.6) - x1 * x1 - y1 * y1 - z1 * z1;
        if (t1 < 0) {
            n1 = 0;
        } else {
            t1 *= t1;
            n1 = t1 * t1 * dot(GRAD3[gi1], x1, y1, z1);
        }

        T t2 = T(0.6) - x2 * x2 - y2 * y2 - z2 * z2;
        if (t2 < 0) {
            n2 = 0;
        } else {
            t2 *= t2;
            n2 = t2 * t2 * dot(GRAD3[gi2], x2, y2, z2);
        }

        T t3 = T(0.6) - x3 * x3 - y3 * y3 - z3 * z3;
        if (t3 < 0) {
            n3 = 0;
        } else {
            t3 *= t3;
            n3 = t3 * t3 * dot(GRAD3[gi3], x3, y3, z3);
        }

        // Add contributions from each corner to get the final noise value
        return T(32) * (n0 + n1 + n2 + n3);
    }

    /**
     * @brief Generate fractal noise using multiple octaves
     * @param x X coordinate
     * @param y Y coordinate
     * @param octaves Number of octaves
     * @param persistence Amplitude multiplier for each octave
     * @param lacunarity Frequency multiplier for each octave
     * @return Fractal noise value
     */
    template <std::floating_point T>
    [[nodiscard]] auto fractal2D(T x, T y, i32 octaves, T persistence,
                                 T lacunarity = T(2)) const noexcept -> T {
        T total = 0;
        T frequency = 1;
        T amplitude = 1;
        T maxValue = 0;

        for (i32 i = 0; i < octaves; ++i) {
            total += noise2D(x * frequency, y * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        return total / maxValue;
    }

private:
    // perm_ is inherited from NoiseBase
    std::array<std::array<f64, 2>, 256> grad2_;
    std::array<std::array<f64, 3>, 256> grad3_;

    // 2D gradient vectors
    static constexpr std::array<std::array<f64, 2>, 8> GRAD2 = {{{{1, 1}},
                                                                 {{-1, 1}},
                                                                 {{1, -1}},
                                                                 {{-1, -1}},
                                                                 {{1, 0}},
                                                                 {{-1, 0}},
                                                                 {{0, 1}},
                                                                 {{0, -1}}}};

    // 3D gradient vectors
    static constexpr std::array<std::array<f64, 3>, 12> GRAD3 = {
        {{{1, 1, 0}},
         {{-1, 1, 0}},
         {{1, -1, 0}},
         {{-1, -1, 0}},
         {{1, 0, 1}},
         {{-1, 0, 1}},
         {{1, 0, -1}},
         {{-1, 0, -1}},
         {{0, 1, 1}},
         {{0, -1, 1}},
         {{0, 1, -1}},
         {{0, -1, -1}}}};

    template <std::floating_point T>
    static constexpr auto dot(const std::array<f64, 2>& g, T x, T y) noexcept
        -> T {
        return static_cast<T>(g[0]) * x + static_cast<T>(g[1]) * y;
    }

    template <std::floating_point T>
    static constexpr auto dot(const std::array<f64, 3>& g, T x, T y,
                              T z) noexcept -> T {
        return static_cast<T>(g[0]) * x + static_cast<T>(g[1]) * y +
               static_cast<T>(g[2]) * z;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_GRAPHICS_SIMPLEX_HPP
