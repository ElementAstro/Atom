#include "gpu_math.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "../../error/exception.hpp"

namespace atom::algorithm::gpu {

#if ATOM_OPENCL_AVAILABLE

auto GPUMath::initialize() -> bool {
    if (initialized_) {
        return true;
    }

    compute_manager_ = &opencl::ComputeManager::getInstance();
    initialized_ = compute_manager_->initialize(opencl::DeviceType::GPU);

    return initialized_;
}

auto GPUMath::isAvailable() const noexcept -> bool {
    return initialized_ && compute_manager_ && compute_manager_->isAvailable();
}

auto GPUMath::vectorAdd(const std::vector<f32>& a,
                        const std::vector<f32>& b) -> std::vector<f32> {
    if (!isAvailable()) {
        THROW_RUNTIME_ERROR("GPU acceleration not available");
    }

    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Vector sizes must match");
    }

    return executeVectorOperation(getVectorAddKernel(), "vector_add", a, b);
}

auto GPUMath::vectorMultiply(const std::vector<f32>& a,
                             const std::vector<f32>& b) -> std::vector<f32> {
    if (!isAvailable()) {
        THROW_RUNTIME_ERROR("GPU acceleration not available");
    }

    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Vector sizes must match");
    }

    return executeVectorOperation(getVectorMultiplyKernel(), "vector_multiply",
                                  a, b);
}

auto GPUMath::dotProduct(const std::vector<f32>& a,
                         const std::vector<f32>& b) -> f32 {
    if (!isAvailable()) {
        THROW_RUNTIME_ERROR("GPU acceleration not available");
    }

    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Vector sizes must match");
    }

    // For small vectors, use CPU implementation
    if (a.size() < 1024) {
        return std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
    }

    // GPU reduction for dot product - fall back to CPU for now
    // Full OpenCL implementation would use getDotProductKernel()
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
}

auto GPUMath::calculateMean(const std::vector<f32>& data) -> f32 {
    if (!isAvailable() || data.empty()) {
        return std::accumulate(data.begin(), data.end(), 0.0f) /
               static_cast<f32>(data.size());
    }

    // For small datasets, use CPU implementation
    if (data.size() < 1024) {
        return std::accumulate(data.begin(), data.end(), 0.0f) /
               static_cast<f32>(data.size());
    }

    // GPU reduction for mean - fall back to CPU for now
    // Full OpenCL implementation would use getReductionKernel()
    return std::accumulate(data.begin(), data.end(), 0.0f) /
           static_cast<f32>(data.size());
}

auto GPUMath::getInstance() -> GPUMath& {
    static GPUMath instance;
    return instance;
}

auto GPUMath::matrixMultiply(const std::vector<f32>& a,
                             const std::vector<f32>& b, usize rows_a,
                             usize cols_a, usize cols_b) -> std::vector<f32> {
    if (!isAvailable()) {
        THROW_RUNTIME_ERROR("GPU acceleration not available");
    }

    if (a.size() != rows_a * cols_a || b.size() != cols_a * cols_b) {
        THROW_INVALID_ARGUMENT("Matrix dimensions do not match input sizes");
    }

    // For small matrices, use CPU implementation
    if (rows_a * cols_b < 1024) {
        std::vector<f32> result(rows_a * cols_b, 0.0f);
        for (usize i = 0; i < rows_a; ++i) {
            for (usize j = 0; j < cols_b; ++j) {
                f32 sum = 0.0f;
                for (usize k = 0; k < cols_a; ++k) {
                    sum += a[i * cols_a + k] * b[k * cols_b + j];
                }
                result[i * cols_b + j] = sum;
            }
        }
        return result;
    }

    // GPU implementation would go here - for now fall back to CPU
    std::vector<f32> result(rows_a * cols_b, 0.0f);
    for (usize i = 0; i < rows_a; ++i) {
        for (usize j = 0; j < cols_b; ++j) {
            f32 sum = 0.0f;
            for (usize k = 0; k < cols_a; ++k) {
                sum += a[i * cols_a + k] * b[k * cols_b + j];
            }
            result[i * cols_b + j] = sum;
        }
    }
    return result;
}

auto GPUMath::matrixTranspose(const std::vector<f32>& matrix, usize rows,
                              usize cols) -> std::vector<f32> {
    if (!isAvailable()) {
        THROW_RUNTIME_ERROR("GPU acceleration not available");
    }

    if (matrix.size() != rows * cols) {
        THROW_INVALID_ARGUMENT("Matrix dimensions do not match input size");
    }

    // For small matrices, use CPU implementation
    std::vector<f32> result(rows * cols);
    for (usize i = 0; i < rows; ++i) {
        for (usize j = 0; j < cols; ++j) {
            result[j * rows + i] = matrix[i * cols + j];
        }
    }
    return result;
}

auto GPUMath::generatePrimes(u32 limit) -> std::vector<u32> {
    if (limit < 2) {
        return {};
    }

    // Sieve of Eratosthenes - CPU implementation
    // GPU acceleration for prime sieve is complex due to data dependencies
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;

    for (u32 p = 2; p * p <= limit; ++p) {
        if (is_prime[p]) {
            for (u32 i = p * p; i <= limit; i += p) {
                is_prime[i] = false;
            }
        }
    }

    std::vector<u32> primes;
    primes.reserve(
        static_cast<usize>(limit / std::log(static_cast<f64>(limit)) * 1.2));

    for (u32 i = 2; i <= limit; ++i) {
        if (is_prime[i]) {
            primes.push_back(i);
        }
    }

    return primes;
}

auto GPUMath::calculateVariance(const std::vector<f32>& data, f32 mean) -> f32 {
    if (data.empty()) {
        return 0.0f;
    }

    // Calculate mean if not provided
    f32 actual_mean = (mean == 0.0f) ? calculateMean(data) : mean;

    // For small datasets, use CPU implementation
    if (data.size() < 1024 || !isAvailable()) {
        f32 sum_sq_diff = 0.0f;
        for (f32 value : data) {
            f32 diff = value - actual_mean;
            sum_sq_diff += diff * diff;
        }
        return sum_sq_diff / static_cast<f32>(data.size());
    }

    // GPU implementation would use reduction kernel - for now fall back to CPU
    f32 sum_sq_diff = 0.0f;
    for (f32 value : data) {
        f32 diff = value - actual_mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / static_cast<f32>(data.size());
}

auto GPUMath::executeVectorOperation(
    const std::string& kernel_source, const std::string& kernel_name,
    const std::vector<f32>& a, const std::vector<f32>& b) -> std::vector<f32> {
    // This is a simplified implementation - in practice, you would:
    // 1. Create OpenCL buffers for input and output
    // 2. Build and execute the kernel
    // 3. Read back the results

    // For now, fall back to CPU implementation
    std::vector<f32> result(a.size());

    if (kernel_name == "vector_add") {
        std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                       std::plus<f32>());
    } else if (kernel_name == "vector_multiply") {
        std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                       std::multiplies<f32>());
    }

    return result;
}

auto GPUMath::executeReduction(const std::vector<f32>& data,
                               const std::string& /*kernel_source*/,
                               const std::string& /*kernel_name*/) -> f32 {
    // CPU fallback implementation for reduction operations
    // Full OpenCL implementation would:
    // 1. Create buffer for input data
    // 2. Create buffer for partial sums
    // 3. Execute reduction kernel in multiple passes
    // 4. Sum final partial results on CPU

    if (data.empty()) {
        return 0.0f;
    }

    return std::accumulate(data.begin(), data.end(), 0.0f);
}

// Kernel source implementations
auto GPUMath::getVectorAddKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void vector_add(__global const float* a,
                        __global const float* b,
                        __global float* result,
                        const int size) {
    int gid = get_global_id(0);
    if (gid < size) {
        result[gid] = a[gid] + b[gid];
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getVectorMultiplyKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void vector_multiply(__global const float* a,
                             __global const float* b,
                             __global float* result,
                             const int size) {
    int gid = get_global_id(0);
    if (gid < size) {
        result[gid] = a[gid] * b[gid];
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getDotProductKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void dot_product(__global const float* a,
                         __global const float* b,
                         __global float* partial_sums,
                         __local float* local_sums,
                         const int size) {
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int group_size = get_local_size(0);

    // Initialize local memory
    local_sums[lid] = 0.0f;

    // Compute partial products
    if (gid < size) {
        local_sums[lid] = a[gid] * b[gid];
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // Reduction in local memory
    for (int offset = group_size / 2; offset > 0; offset /= 2) {
        if (lid < offset) {
            local_sums[lid] += local_sums[lid + offset];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Write result for this work group
    if (lid == 0) {
        partial_sums[get_group_id(0)] = local_sums[0];
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getMatrixMultiplyKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void matrix_multiply(__global const float* a,
                             __global const float* b,
                             __global float* c,
                             const int rows_a,
                             const int cols_a,
                             const int cols_b) {
    int row = get_global_id(0);
    int col = get_global_id(1);

    if (row < rows_a && col < cols_b) {
        float sum = 0.0f;
        for (int k = 0; k < cols_a; k++) {
            sum += a[row * cols_a + k] * b[k * cols_b + col];
        }
        c[row * cols_b + col] = sum;
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getMatrixTransposeKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void matrix_transpose(__global const float* input,
                              __global float* output,
                              const int rows,
                              const int cols) {
    int row = get_global_id(0);
    int col = get_global_id(1);

    if (row < rows && col < cols) {
        output[col * rows + row] = input[row * cols + col];
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getPrimeSieveKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void prime_sieve(__global char* is_prime,
                         const int limit) {
    int gid = get_global_id(0);
    int p = 2 + gid;

    if (p * p > limit) return;

    if (is_prime[p]) {
        for (int i = p * p; i <= limit; i += p) {
            is_prime[i] = 0;
        }
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getReductionKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void reduction_sum(__global const float* input,
                           __global float* output,
                           __local float* local_data,
                           const int size) {
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int group_size = get_local_size(0);

    // Load data into local memory
    local_data[lid] = (gid < size) ? input[gid] : 0.0f;
    barrier(CLK_LOCAL_MEM_FENCE);

    // Reduction in local memory
    for (int offset = group_size / 2; offset > 0; offset /= 2) {
        if (lid < offset) {
            local_data[lid] += local_data[lid + offset];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Write result for this work group
    if (lid == 0) {
        output[get_group_id(0)] = local_data[0];
    }
}
)CLC";
    return kernel;
}

auto GPUMath::getVarianceKernel() -> const std::string& {
    static const std::string kernel = R"CLC(
__kernel void variance_kernel(__global const float* data,
                             __global float* partial_vars,
                             __local float* local_data,
                             const float mean,
                             const int size) {
    int gid = get_global_id(0);
    int lid = get_local_id(0);
    int group_size = get_local_size(0);

    // Compute squared differences
    local_data[lid] = 0.0f;
    if (gid < size) {
        float diff = data[gid] - mean;
        local_data[lid] = diff * diff;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // Reduction in local memory
    for (int offset = group_size / 2; offset > 0; offset /= 2) {
        if (lid < offset) {
            local_data[lid] += local_data[lid + offset];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Write result for this work group
    if (lid == 0) {
        partial_vars[get_group_id(0)] = local_data[0];
    }
}
)CLC";
    return kernel;
}

#else  // !ATOM_OPENCL_AVAILABLE

// Stub implementations when OpenCL is not available
auto GPUMath::initialize() -> bool { return false; }
auto GPUMath::isAvailable() const noexcept -> bool { return false; }

auto GPUMath::vectorAdd(const std::vector<f32>& a,
                        const std::vector<f32>& b) -> std::vector<f32> {
    std::vector<f32> result(a.size());
    std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                   std::plus<f32>());
    return result;
}

auto GPUMath::vectorMultiply(const std::vector<f32>& a,
                             const std::vector<f32>& b) -> std::vector<f32> {
    std::vector<f32> result(a.size());
    std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                   std::multiplies<f32>());
    return result;
}

auto GPUMath::dotProduct(const std::vector<f32>& a,
                         const std::vector<f32>& b) -> f32 {
    return std::inner_product(a.begin(), a.end(), b.begin(), 0.0f);
}

auto GPUMath::calculateMean(const std::vector<f32>& data) -> f32 {
    if (data.empty()) {
        return 0.0f;
    }
    return std::accumulate(data.begin(), data.end(), 0.0f) /
           static_cast<f32>(data.size());
}

auto GPUMath::matrixMultiply(const std::vector<f32>& a,
                             const std::vector<f32>& b, usize rows_a,
                             usize cols_a, usize cols_b) -> std::vector<f32> {
    if (a.size() != rows_a * cols_a || b.size() != cols_a * cols_b) {
        THROW_INVALID_ARGUMENT("Matrix dimensions do not match input sizes");
    }

    std::vector<f32> result(rows_a * cols_b, 0.0f);

    for (usize i = 0; i < rows_a; ++i) {
        for (usize j = 0; j < cols_b; ++j) {
            f32 sum = 0.0f;
            for (usize k = 0; k < cols_a; ++k) {
                sum += a[i * cols_a + k] * b[k * cols_b + j];
            }
            result[i * cols_b + j] = sum;
        }
    }

    return result;
}

auto GPUMath::matrixTranspose(const std::vector<f32>& matrix, usize rows,
                              usize cols) -> std::vector<f32> {
    if (matrix.size() != rows * cols) {
        THROW_INVALID_ARGUMENT("Matrix dimensions do not match input size");
    }

    std::vector<f32> result(rows * cols);

    for (usize i = 0; i < rows; ++i) {
        for (usize j = 0; j < cols; ++j) {
            result[j * rows + i] = matrix[i * cols + j];
        }
    }

    return result;
}

auto GPUMath::generatePrimes(u32 limit) -> std::vector<u32> {
    if (limit < 2) {
        return {};
    }

    // Sieve of Eratosthenes implementation
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;

    for (u32 p = 2; p * p <= limit; ++p) {
        if (is_prime[p]) {
            for (u32 i = p * p; i <= limit; i += p) {
                is_prime[i] = false;
            }
        }
    }

    std::vector<u32> primes;
    primes.reserve(limit / std::log(limit) * 1.2);  // Approximate prime count

    for (u32 i = 2; i <= limit; ++i) {
        if (is_prime[i]) {
            primes.push_back(i);
        }
    }

    return primes;
}

auto GPUMath::calculateVariance(const std::vector<f32>& data, f32 mean) -> f32 {
    if (data.empty()) {
        return 0.0f;
    }

    // Calculate mean if not provided (mean == 0.0f is treated as "not
    // provided")
    f32 actual_mean = (mean == 0.0f) ? calculateMean(data) : mean;

    f32 sum_sq_diff = 0.0f;
    for (f32 value : data) {
        f32 diff = value - actual_mean;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / static_cast<f32>(data.size());
}

auto GPUMath::getInstance() -> GPUMath& {
    static GPUMath instance;
    return instance;
}

#endif  // ATOM_OPENCL_AVAILABLE

}  // namespace atom::algorithm::gpu
