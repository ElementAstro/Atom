/*
 * gpu_math.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating GPU math from atom/algorithm/math/gpu_math.hpp
 */

#include "atom/algorithm/math/gpu_math.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;
using namespace atom::algorithm::gpu;

// Helper function to generate random float vectorstd::vector<f32>
// generateRandomVector(usize size, f32 min_val = 0.0f,
                                      f32 max_val = 100.0f) {
                                          std::random_device rd;
                                          std::mt19937 gen(rd());
                                          std::uniform_real_distribution<f32>
                                              dis(min_val, max_val);

                                          std::vector<f32> vec(size);
                                          for (auto& v : vec) {
                                              v = dis(gen);
                                          }
                                          return vec;
                                      }

                                      // Helper function to print small
                                      // vectorvoid printVector(const
                                      // std::vector<f32>& vec, const
                                      // std::string& name,
                 usize max_elements = 10) {
                     std::cout << name << " (size=" << vec.size() << "): [";
                     for (usize i = 0; i < std::min(vec.size(), max_elements);
                          ++i) {
                         std::cout << std::fixed << std::setprecision(2)
                                   << vec[i];
                         if (i < std::min(vec.size(), max_elements) - 1)
                             std::cout << ", ";
                     }
                     if (vec.size() > max_elements)
                         std::cout << ", ...";
                     std::cout << "]\n";
                 }

                 // Demonstrate GPU initializationvoid demonstrateGPUInit() {
                 std::cout << "\n=== GPU Math Initialization ===\n";

                 GPUMath gpu;

                 bool initialized = gpu.initialize();
                 std::cout << "GPU initialized: "
                           << (initialized ? "Yes" : "No") << "\n";
                 std::cout << "GPU available: "
                           << (gpu.isAvailable() ? "Yes" : "No") << "\n";

                 if (!gpu.isAvailable()) {
                     std::cout << "\nNote: GPU acceleration not available. "
                                  "Operations will use "
                                  "CPU fallback.\n";
                 }
                 }

                 // Demonstrate GPU vector additionvoid demonstrateVectorAdd() {
                 std::cout << "\n=== GPU Vector Addition ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize SIZE = 1000;

                 auto a = generateRandomVector(SIZE);
                 auto b = generateRandomVector(SIZE);

                 printVector(a, "Vector A");
                 printVector(b, "Vector B");

                 auto result = gpu.vectorAdd(a, b);
                 printVector(result, "A + B");

                 // Verify result
                 bool correct = true;
                 for (usize i = 0; i < SIZE; ++i) {
                     if (std::abs(result[i] - (a[i] + b[i])) > 1e-5f) {
                         correct = false;
                         break;
                     }
                 }
                 std::cout << "Verification: "
                           << (correct ? "PASSED" : "FAILED") << "\n";
                 }

                 // Demonstrate GPU vector multiplicationvoid
                 // demonstrateVectorMultiply() {
                 std::cout
                     << "\n=== GPU Vector Multiplication (Element-wise) ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize SIZE = 1000;

                 auto a = generateRandomVector(SIZE, 1.0f, 10.0f);
                 auto b = generateRandomVector(SIZE, 1.0f, 10.0f);

                 printVector(a, "Vector A");
                 printVector(b, "Vector B");

                 auto result = gpu.vectorMultiply(a, b);
                 printVector(result, "A * B");

                 // Verify result
                 bool correct = true;
                 for (usize i = 0; i < SIZE; ++i) {
                     if (std::abs(result[i] - (a[i] * b[i])) > 1e-4f) {
                         correct = false;
                         break;
                     }
                 }
                 std::cout << "Verification: "
                           << (correct ? "PASSED" : "FAILED") << "\n";
                 }

                 // Demonstrate GPU dot productvoid demonstrateDotProduct() {
                 std::cout << "\n=== GPU Dot Product ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize SIZE = 10000;

                 auto a = generateRandomVector(SIZE, -10.0f, 10.0f);
                 auto b = generateRandomVector(SIZE, -10.0f, 10.0f);

                 f32 gpu_result = gpu.dotProduct(a, b);

                 // CPU reference
                 f32 cpu_result = 0.0f;
                 for (usize i = 0; i < SIZE; ++i) {
                     cpu_result += a[i] * b[i];
                 }

                 std::cout << "GPU dot product: " << std::fixed
                           << std::setprecision(4) << gpu_result << "\n";
                 std::cout << "CPU dot product: " << cpu_result << "\n";
                 std::cout << "Relative error: "
                           << std::abs(gpu_result - cpu_result) /
                                  std::max(std::abs(cpu_result), 1e-10f)
                           << "\n";
                 }

                 // Demonstrate GPU matrix multiplicationvoid
                 // demonstrateMatrixMultiply() {
                 std::cout << "\n=== GPU Matrix Multiplication ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 // Small matrices for demonstration
                 constexpr usize M = 4, K = 3, N = 5;

                 // Matrix A (M x K)
                 std::vector<f32> A = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

                 // Matrix B (K x N)
                 std::vector<f32> B = {1, 2,  3,  4,  5,  6,  7, 8,
                                       9, 10, 11, 12, 13, 14, 15};

                 std::cout << "Matrix A (" << M << "x" << K << "):\n";
                 for (usize i = 0; i < M; ++i) {
                     std::cout << "  ";
                     for (usize j = 0; j < K; ++j) {
                         std::cout << std::setw(4) << A[i * K + j];
                     }
                     std::cout << "\n";
                 }

                 std::cout << "\nMatrix B (" << K << "x" << N << "):\n";
                 for (usize i = 0; i < K; ++i) {
                     std::cout << "  ";
                     for (usize j = 0; j < N; ++j) {
                         std::cout << std::setw(4) << B[i * N + j];
                     }
                     std::cout << "\n";
                 }

                 auto C = gpu.matrixMultiply(A, B, M, K, N);

                 std::cout << "\nResult C = A * B (" << M << "x" << N << "):\n";
                 for (usize i = 0; i < M; ++i) {
                     std::cout << "  ";
                     for (usize j = 0; j < N; ++j) {
                         std::cout << std::setw(6)
                                   << static_cast<int>(C[i * N + j]);
                     }
                     std::cout << "\n";
                 }
                 }

                 // Demonstrate GPU matrix transposevoid
                 // demonstrateMatrixTranspose() {
                 std::cout << "\n=== GPU Matrix Transpose ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize ROWS = 3, COLS = 4;

                 std::vector<f32> matrix = {1, 2, 3, 4,  5,  6,
                                            7, 8, 9, 10, 11, 12};

                 std::cout << "Original matrix (" << ROWS << "x" << COLS
                           << "):\n";
                 for (usize i = 0; i < ROWS; ++i) {
                     std::cout << "  ";
                     for (usize j = 0; j < COLS; ++j) {
                         std::cout << std::setw(4)
                                   << static_cast<int>(matrix[i * COLS + j]);
                     }
                     std::cout << "\n";
                 }

                 auto transposed = gpu.matrixTranspose(matrix, ROWS, COLS);

                 std::cout << "\nTransposed matrix (" << COLS << "x" << ROWS
                           << "):\n";
                 for (usize i = 0; i < COLS; ++i) {
                     std::cout << "  ";
                     for (usize j = 0; j < ROWS; ++j) {
                         std::cout
                             << std::setw(4)
                             << static_cast<int>(transposed[i * ROWS + j]);
                     }
                     std::cout << "\n";
                 }
                 }

                 // Demonstrate GPU prime generationvoid
                 // demonstratePrimeGeneration() {
                 std::cout << "\n=== GPU Prime Number Generation ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 u32 limit = 100;
                 auto primes = gpu.generatePrimes(limit);

                 std::cout << "Primes up to " << limit << " (" << primes.size()
                           << " primes):\n";
                 for (usize i = 0; i < primes.size(); ++i) {
                     std::cout << primes[i];
                     if (i < primes.size() - 1)
                         std::cout << ", ";
                     if ((i + 1) % 15 == 0)
                         std::cout << "\n";
                 }
                 std::cout << "\n";
                 }

                 // Demonstrate GPU statistical meanvoid
                 // demonstrateStatisticalMean() {
                 std::cout << "\n=== GPU Statistical Mean ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize SIZE = 10000;
                 auto data = generateRandomVector(SIZE, 0.0f, 100.0f);

                 f32 gpu_mean = gpu.mean(data);

                 // CPU reference
                 f32 cpu_mean = 0.0f;
                 for (auto v : data) {
                     cpu_mean += v;
                 }
                 cpu_mean /= data.size();

                 std::cout << "GPU mean: " << std::fixed << std::setprecision(6)
                           << gpu_mean << "\n";
                 std::cout << "CPU mean: " << cpu_mean << "\n";
                 std::cout << "Difference: " << std::abs(gpu_mean - cpu_mean)
                           << "\n";
                 }

                 // Benchmark GPU vs CPUvoid benchmarkGPUvsCPU() {
                 std::cout << "\n=== GPU vs CPU Benchmark ===\n";

                 GPUMath gpu;
                 gpu.initialize();

                 constexpr usize SIZE = 1000000;
                 constexpr int ITERATIONS = 10;

                 auto a = generateRandomVector(SIZE);
                 auto b = generateRandomVector(SIZE);
                 std::vector<f32> result(SIZE);

                 std::cout << "Vector size: " << SIZE
                           << ", Iterations: " << ITERATIONS << "\n\n";

                 // GPU vector addition benchmark
                 auto start = std::chrono::high_resolution_clock::now();
                 for (int i = 0; i < ITERATIONS; ++i) {
                     result = gpu.vectorAdd(a, b);
                 }
                 auto end = std::chrono::high_resolution_clock::now();
                 auto gpu_duration =
                     std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - start);
                 std::cout << "GPU vector add: " << gpu_duration.count()
                           << " ms total (" << gpu_duration.count() / ITERATIONS
                           << " ms/iter)\n";

                 // CPU vector addition benchmark
                 start = std::chrono::high_resolution_clock::now();
                 for (int iter = 0; iter < ITERATIONS; ++iter) {
                     for (usize i = 0; i < SIZE; ++i) {
                         result[i] = a[i] + b[i];
                     }
                 }
                 end = std::chrono::high_resolution_clock::now();
                 auto cpu_duration =
                     std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - start);
                 std::cout << "CPU vector add: " << cpu_duration.count()
                           << " ms total (" << cpu_duration.count() / ITERATIONS
                           << " ms/iter)\n";

                 if (gpu_duration.count() > 0) {
                     std::cout << "Speedup: " << std::fixed
                               << std::setprecision(2)
                               << static_cast<f64>(cpu_duration.count()) /
                                      gpu_duration.count()
                               << "x\n";
                 }
                 }

                 int main() {
                     std::cout << "========================================\n";
                     std::cout << "   GPU Math Example\n";
                     std::cout << "========================================\n";

                     try {
                         demonstrateGPUInit();
                         demonstrateVectorAdd();
                         demonstrateVectorMultiply();
                         demonstrateDotProduct();
                         demonstrateMatrixMultiply();
                         demonstrateMatrixTranspose();
                         demonstratePrimeGeneration();
                         demonstrateStatisticalMean();
                         benchmarkGPUvsCPU();

                         std::cout
                             << "\n========================================\n";
                         std::cout
                             << "   All examples completed successfully!\n";
                         std::cout
                             << "========================================\n";

                     } catch (const std::exception& e) {
                         std::cerr << "Error: " << e.what() << std::endl;
                         return 1;
                     }

                     return 0;
                 }
