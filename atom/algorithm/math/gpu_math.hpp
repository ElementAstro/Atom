#ifndef ATOM_ALGORITHM_MATH_GPU_MATH_HPP
#define ATOM_ALGORITHM_MATH_GPU_MATH_HPP

#include <memory>
#include <string>
#include <vector>

#include "../core/opencl_utils.hpp"
#include "../rust_numeric.hpp"

namespace atom::algorithm::gpu {

/**
 * @brief GPU-accelerated mathematical operations using OpenCL
 *
 * This class provides GPU acceleration for computationally intensive
 * mathematical operations including:
 * - Vector operations (addition, multiplication, dot product)
 * - Matrix operations (multiplication, transpose)
 * - Statistical computations (mean, variance, correlation)
 * - Prime number generation and testing
 */
class GPUMath {
public:
    /**
     * @brief Initialize GPU math operations
     * @return true if GPU is available and initialized
     */
    [[nodiscard]] auto initialize() -> bool;

    /**
     * @brief Check if GPU acceleration is available
     * @return true if available
     */
    [[nodiscard]] auto isAvailable() const noexcept -> bool;

    /**
     * @brief GPU-accelerated vector addition
     * @param a First vector
     * @param b Second vector
     * @return Result vector (a + b)
     */
    [[nodiscard]] auto vectorAdd(const std::vector<f32>& a,
                                 const std::vector<f32>& b) -> std::vector<f32>;

    /**
     * @brief GPU-accelerated vector multiplication (element-wise)
     * @param a First vector
     * @param b Second vector
     * @return Result vector (a * b element-wise)
     */
    [[nodiscard]] auto vectorMultiply(const std::vector<f32>& a,
                                      const std::vector<f32>& b)
        -> std::vector<f32>;

    /**
     * @brief GPU-accelerated dot product
     * @param a First vector
     * @param b Second vector
     * @return Dot product result
     */
    [[nodiscard]] auto dotProduct(const std::vector<f32>& a,
                                  const std::vector<f32>& b) -> f32;

    /**
     * @brief GPU-accelerated matrix multiplication
     * @param a First matrix (row-major order)
     * @param b Second matrix (row-major order)
     * @param rows_a Number of rows in matrix A
     * @param cols_a Number of columns in matrix A (must equal rows_b)
     * @param cols_b Number of columns in matrix B
     * @return Result matrix (row-major order)
     */
    [[nodiscard]] auto matrixMultiply(const std::vector<f32>& a,
                                      const std::vector<f32>& b, usize rows_a,
                                      usize cols_a,
                                      usize cols_b) -> std::vector<f32>;

    /**
     * @brief GPU-accelerated matrix transpose
     * @param matrix Input matrix (row-major order)
     * @param rows Number of rows
     * @param cols Number of columns
     * @return Transposed matrix (row-major order)
     */
    [[nodiscard]] auto matrixTranspose(const std::vector<f32>& matrix,
                                       usize rows,
                                       usize cols) -> std::vector<f32>;

    /**
     * @brief GPU-accelerated prime number sieve
     * @param limit Upper limit for prime generation
     * @return Vector of prime numbers up to limit
     */
    [[nodiscard]] auto generatePrimes(u32 limit) -> std::vector<u32>;

    /**
     * @brief GPU-accelerated statistical mean calculation
     * @param data Input data
     * @return Mean value
     */
    [[nodiscard]] auto calculateMean(const std::vector<f32>& data) -> f32;

    /**
     * @brief GPU-accelerated variance calculation
     * @param data Input data
     * @param mean Pre-calculated mean (optional)
     * @return Variance value
     */
    [[nodiscard]] auto calculateVariance(const std::vector<f32>& data,
                                         f32 mean = 0.0f) -> f32;

    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    [[nodiscard]] static auto getInstance() -> GPUMath&;

private:
    GPUMath() = default;

    opencl::ComputeManager* compute_manager_ = nullptr;
    bool initialized_ = false;

    // OpenCL kernel sources
    static const std::string vector_add_kernel_;
    static const std::string vector_multiply_kernel_;
    static const std::string dot_product_kernel_;
    static const std::string matrix_multiply_kernel_;
    static const std::string matrix_transpose_kernel_;
    static const std::string prime_sieve_kernel_;
    static const std::string reduction_kernel_;
    static const std::string variance_kernel_;

    // Helper methods
    [[nodiscard]] auto executeVectorOperation(
        const std::string& kernel_source, const std::string& kernel_name,
        const std::vector<f32>& a,
        const std::vector<f32>& b) -> std::vector<f32>;

    [[nodiscard]] auto executeReduction(const std::vector<f32>& data,
                                        const std::string& kernel_source,
                                        const std::string& kernel_name) -> f32;
};

}  // namespace atom::algorithm::gpu

#endif  // ATOM_ALGORITHM_MATH_GPU_MATH_HPP
