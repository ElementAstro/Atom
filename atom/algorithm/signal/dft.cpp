/*
 * dft.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Implementation of two-dimensional Discrete Fourier Transform
(DFT) and Inverse Discrete Fourier Transform (IDFT).

**************************************************/

#include "dft.hpp"
#include "atom/algorithm/core/rust_numeric.hpp"

#include <cmath>
#include <numbers>
#include <thread>
#include <vector>

#if ATOM_USE_SIMD && !ATOM_USE_STD_SIMD
#ifdef __SSE__
#include <immintrin.h>
#endif
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma warning(disable : 4251)  // Needs to have dll-interface
#pragma warning(disable : 4275)  // Non dll-interface class used as base for
                                 // dll-interface class
#endif

namespace atom::algorithm {

// 2D Discrete Fourier Transform (2D DFT)
auto dft2D(const std::vector<std::vector<f64>>& signal, i32 numThreads)
    -> std::vector<std::vector<std::complex<f64>>> {
    const usize M = signal.size();
    const usize N = signal[0].size();
    std::vector<std::vector<std::complex<f64>>> frequency(
        M, std::vector<std::complex<f64>>(N, {0, 0}));

    // Lambda function to compute the DFT for a block of rows
    auto computeDFT = [&](usize startRow, usize endRow) {
#ifdef ATOM_ATOM_USE_SIMD
        std::array<f64, 4> realParts{};
        std::array<f64, 4> imagParts{};
#endif
        for (usize u = startRow; u < endRow; ++u) {
            for (usize v = 0; v < N; ++v) {
#ifdef ATOM_ATOM_USE_SIMD
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();

                for (usize m = 0; m < M; ++m) {
                    for (usize n = 0; n < N; n += 4) {
                        f64 theta[4];
                        for (i32 k = 0; k < 4; ++k) {
                            theta[k] =
                                -2.0 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + static_cast<usize>(k))) /
                                     static_cast<f64>(N));
                        }

                        __m256d signalVec = _mm256_loadu_pd(&signal[m][n]);
                        __m256d cosVec = _mm256_setr_pd(
                            F64::cos(theta[0]), F64::cos(theta[1]),
                            F64::cos(theta[2]), F64::cos(theta[3]));
                        __m256d sinVec = _mm256_setr_pd(
                            F64::sin(theta[0]), F64::sin(theta[1]),
                            F64::sin(theta[2]), F64::sin(theta[3]));

                        sumReal = _mm256_add_pd(
                            sumReal, _mm256_mul_pd(signalVec, cosVec));
                        sumImag = _mm256_add_pd(
                            sumImag, _mm256_mul_pd(signalVec, sinVec));
                    }
                }

                _mm256_store_pd(realParts.data(), sumReal);
                _mm256_store_pd(imagParts.data(), sumImag);

                f64 realSum =
                    realParts[0] + realParts[1] + realParts[2] + realParts[3];
                f64 imagSum =
                    imagParts[0] + imagParts[1] + imagParts[2] + imagParts[3];

                frequency[u][v] = std::complex<f64>(realSum, imagSum);
#else
                std::complex<f64> sum(0, 0);
                for (usize m = 0; m < M; ++m) {
                    for (usize n = 0; n < N; ++n) {
                        f64 theta =
                            -2 * std::numbers::pi *
                            ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                 static_cast<f64>(M) +
                             (static_cast<f64>(v) * static_cast<f64>(n)) /
                                 static_cast<f64>(N));
                        std::complex<f64> w(F64::cos(theta), F64::sin(theta));
                        sum += signal[m][n] * w;
                    }
                }
                frequency[u][v] = sum;
#endif
            }
        }
    };

    // Multithreading support
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        usize rowsPerThread = M / static_cast<usize>(numThreads);
        usize blockStartRow = 0;

        for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            usize blockEndRow = (threadIndex == numThreads - 1)
                                    ? M
                                    : blockStartRow + rowsPerThread;
            threadPool.emplace_back(computeDFT, blockStartRow, blockEndRow);
            blockStartRow = blockEndRow;
        }

        // Threads are joined automatically by jthread destructor
    } else {
        // Single-threaded execution
        computeDFT(0, M);
    }

    return frequency;
}

// 2D Inverse Discrete Fourier Transform (2D IDFT)
auto idft2D(const std::vector<std::vector<std::complex<f64>>>& spectrum,
            i32 numThreads) -> std::vector<std::vector<f64>> {
    const usize M = spectrum.size();
    const usize N = spectrum[0].size();
    std::vector<std::vector<f64>> spatial(M, std::vector<f64>(N, 0.0));

    // Lambda function to compute the IDFT for a block of rows
    auto computeIDFT = [&](usize startRow, usize endRow) {
        for (usize m = startRow; m < endRow; ++m) {
            for (usize n = 0; n < N; ++n) {
#ifdef ATOM_ATOM_USE_SIMD
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();
                for (usize u = 0; u < M; ++u) {
                    for (usize v = 0; v < N; v += SIMD_WIDTH) {
                        __m256d theta = _mm256_set_pd(
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 3)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 2)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 1)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) * static_cast<f64>(n)) /
                                     static_cast<f64>(N)));
                        __m256d wReal = _mm256_cos_pd(theta);
                        __m256d wImag = _mm256_sin_pd(theta);
                        __m256d spectrumReal =
                            _mm256_loadu_pd(&spectrum[u][v].real());
                        __m256d spectrumImag =
                            _mm256_loadu_pd(&spectrum[u][v].imag());

                        sumReal = _mm256_fmadd_pd(spectrumReal, wReal, sumReal);
                        sumImag = _mm256_fmadd_pd(spectrumImag, wImag, sumImag);
                    }
                }
                // Assuming _mm256_reduce_add_pd is defined or use an
                // alternative
                f64 realPart = _mm256_hadd_pd(sumReal, sumReal).m256d_f64[0] +
                               _mm256_hadd_pd(sumReal, sumReal).m256d_f64[2];
                f64 imagPart = _mm256_hadd_pd(sumImag, sumImag).m256d_f64[0] +
                               _mm256_hadd_pd(sumImag, sumImag).m256d_f64[2];
                spatial[m][n] = (realPart + imagPart) /
                                (static_cast<f64>(M) * static_cast<f64>(N));
#else
                std::complex<f64> sum(0.0, 0.0);
                for (usize u = 0; u < M; ++u) {
                    for (usize v = 0; v < N; ++v) {
                        f64 theta =
                            2 * std::numbers::pi *
                            ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                 static_cast<f64>(M) +
                             (static_cast<f64>(v) * static_cast<f64>(n)) /
                                 static_cast<f64>(N));
                        std::complex<f64> w(F64::cos(theta), F64::sin(theta));
                        sum += spectrum[u][v] * w;
                    }
                }
                spatial[m][n] = std::real(sum) /
                                (static_cast<f64>(M) * static_cast<f64>(N));
#endif
            }
        }
    };

    // Multithreading support
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        usize rowsPerThread = M / static_cast<usize>(numThreads);
        usize blockStartRow = 0;

        for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            usize blockEndRow = (threadIndex == numThreads - 1)
                                    ? M
                                    : blockStartRow + rowsPerThread;
            threadPool.emplace_back(computeIDFT, blockStartRow, blockEndRow);
            blockStartRow = blockEndRow;
        }

        // Threads are joined automatically by jthread destructor
    } else {
        // Single-threaded execution
        computeIDFT(0, M);
    }

    return spatial;
}

}  // namespace atom::algorithm

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
