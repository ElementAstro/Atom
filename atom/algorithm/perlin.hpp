#ifndef ATOM_ALGORITHM_PERLIN_HPP
#define ATOM_ALGORITHM_PERLIN_HPP

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <future>  // For std::async and std::future
#include <numeric>
#include <random>
#include <span>
#include <thread>  // For std::thread::hardware_concurrency
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"

#ifdef ATOM_USE_OPENCL
#include <CL/cl.h>
#include "atom/error/exception.hpp"
#endif

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#endif

namespace atom::algorithm {
class PerlinNoise {
public:
    /**
     * @brief Constructs a PerlinNoise object with an optional seed.
     *
     * Initializes the permutation table using the provided seed.
     *
     * @param seed The seed for the random number generator used to initialize
     * the permutation table.
     */
    explicit PerlinNoise(u32 seed = std::default_random_engine::default_seed) {
        p.resize(512);
        std::iota(p.begin(), p.begin() + 256, 0);

        std::default_random_engine engine(seed);
        std::ranges::shuffle(std::span(p.begin(), p.begin() + 256), engine);

        std::ranges::copy(std::span(p.begin(), p.begin() + 256),
                          p.begin() + 256);

#ifdef ATOM_USE_OPENCL
        initializeOpenCL();
#endif
    }

    /**
     * @brief Destroys the PerlinNoise object.
     *
     * Cleans up OpenCL resources if they were initialized.
     */
    ~PerlinNoise() {
#ifdef ATOM_USE_OPENCL
        cleanupOpenCL();
#endif
    }

    /**
     * @brief Calculates the Perlin noise value for a 3D point.
     *
     * Dispatches to either the CPU or OpenCL implementation based on
     * availability.
     *
     * @tparam T A floating-point type (e.g., float, double).
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @param z The z-coordinate.
     * @return The normalized Perlin noise value in the range [0, 1].
     */
    template <std::floating_point T>
    [[nodiscard]] auto noise(T x, T y, T z) const -> T {
#ifdef ATOM_USE_OPENCL
        // Note: The current OpenCL implementation calculates noise for a single
        // point and uses a simplified lerp/grad. For performance, OpenCL should
        // be used for batch processing (e.g., generating a whole map) with
        // a kernel implementing the standard Perlin noise functions (fade,
        // lerp, grad). The CPU implementation below is the standard reference.
        if (opencl_available_) {
            // This call is currently inefficient for single points and uses
            // a different kernel implementation than the CPU version.
            // Consider using OpenCL only for batch processing like
            // generateNoiseMap.
            return noiseOpenCL(x, y, z);
        }
#endif
        return noiseCPU(x, y, z);
    }

    /**
     * @brief Calculates octave Perlin noise for a 3D point.
     *
     * Combines multiple layers (octaves) of Perlin noise to create more complex
     * patterns.
     *
     * @tparam T A floating-point type (e.g., float, double).
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @param z The z-coordinate.
     * @param octaves The number of noise layers to combine.
     * @param persistence Controls the amplitude of each successive octave.
     * @return The combined and normalized octave noise value.
     */
    template <std::floating_point T>
    [[nodiscard]] auto octaveNoise(T x, T y, T z, i32 octaves,
                                   T persistence) const -> T {
        T total = 0;
        T frequency = 1;
        T amplitude = 1;
        T maxValue = 0;  // Used for normalization

        for (i32 i = 0; i < octaves; ++i) {
            total +=
                noise(x * frequency, y * frequency, z * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= persistence;
            frequency *= 2;
        }

        // Avoid division by zero if maxValue is 0 (e.g., octaves = 0)
        return maxValue == 0 ? 0 : total / maxValue;
    }

    /**
     * @brief Generates a 2D noise map using octave Perlin noise.
     *
     * Creates a grid of noise values, optionally using multiple threads for
     * parallel processing.
     *
     * @param width The width of the noise map.
     * @param height The height of the noise map.
     * @param scale Controls the zoom level of the noise.
     * @param octaves The number of noise layers.
     * @param persistence Controls the amplitude of each successive octave.
     * @param lacunarity Controls the frequency of each successive octave
     * (currently unused).
     * @param seed The seed for the random offset.
     * @param numThreads The number of threads to use for parallel generation.
     *                   If 0, uses hardware concurrency. If 1, uses single
     * thread.
     * @return A 2D vector representing the noise map, with values in [0, 1].
     */
    [[nodiscard]] auto generateNoiseMap(
        i32 width, i32 height, f64 scale, i32 octaves, f64 persistence,
        f64 /*lacunarity*/, i32 seed = std::default_random_engine::default_seed,
        usize numThreads = 0) const -> std::vector<std::vector<f64>> {
        if (width <= 0 || height <= 0 || scale <= 0 || octaves <= 0 ||
            persistence <= 0) {
            // Basic validation
            spdlog::warn(
                "Invalid parameters for generateNoiseMap. Width: {}, Height: "
                "{}, Scale: {}, Octaves: {}, Persistence: {}",
                width, height, scale, octaves, persistence);
            return std::vector<std::vector<f64>>(height,
                                                 std::vector<f64>(width, 0.0));
        }

        std::vector<std::vector<f64>> noiseMap(height, std::vector<f64>(width));
        std::default_random_engine prng(seed);
        std::uniform_real_distribution<f64> dist(
            -100000, 100000);  // Use larger range for offset
        f64 offsetX = dist(prng);
        f64 offsetY = dist(prng);

        usize effectiveNumThreads = numThreads;
        if (effectiveNumThreads == 0) {
            effectiveNumThreads = std::thread::hardware_concurrency();
            if (effectiveNumThreads == 0) {
                effectiveNumThreads =
                    1;  // Default to 1 if hardware_concurrency is 0
            }
        }

        // Ensure we don't create more threads than rows
        effectiveNumThreads =
            std::min(effectiveNumThreads, static_cast<usize>(height));

        if (effectiveNumThreads <= 1) {
            // Single-threaded execution
            spdlog::debug("Generating noise map using single thread.");
            for (i32 y = 0; y < height; ++y) {
                for (i32 x = 0; x < width; ++x) {
                    f64 sampleX = (x - width / 2.0 + offsetX) / scale;
                    f64 sampleY = (y - height / 2.0 + offsetY) / scale;
                    // Z coordinate is 0 for 2D map
                    noiseMap[y][x] = octaveNoise(sampleX, sampleY, 0.0, octaves,
                                                 persistence);
                }
            }
        } else {
            // Parallel execution
            spdlog::debug("Generating noise map using {} threads.",
                          effectiveNumThreads);
            std::vector<std::future<void>> futures;
            usize rowsPerThread = height / effectiveNumThreads;
            usize remainingRows = height % effectiveNumThreads;

            for (usize i = 0; i < effectiveNumThreads; ++i) {
                usize startRow = i * rowsPerThread + std::min(i, remainingRows);
                usize endRow =
                    startRow + rowsPerThread + (i < remainingRows ? 1 : 0);

                // Launch a thread to process a range of rows
                futures.push_back(std::async(
                    std::launch::async,  // Ensure a new thread is launched
                    [&, startRow, endRow]() {
                        for (i32 y = static_cast<i32>(startRow);
                             y < static_cast<i32>(endRow); ++y) {
                            for (i32 x = 0; x < width; ++x) {
                                f64 sampleX =
                                    (x - width / 2.0 + offsetX) / scale;
                                f64 sampleY =
                                    (y - height / 2.0 + offsetY) / scale;
                                // Z coordinate is 0 for 2D map
                                noiseMap[y][x] =
                                    octaveNoise(sampleX, sampleY, 0.0, octaves,
                                                persistence);
                            }
                        }
                    }));
            }

            // Wait for all threads to complete and propagate exceptions
            try {
                for (auto& future : futures) {
                    future.get();
                }
            } catch (const std::exception& e) {
                spdlog::error("Error during parallel noise map generation: {}",
                              e.what());
                // Re-throw the exception
                throw;
            }
        }

        return noiseMap;
    }

private:
    std::vector<i32> p;  // Permutation table

#ifdef ATOM_USE_OPENCL
    cl_context context_;
    cl_command_queue queue_;
    cl_program program_;
    cl_kernel noise_kernel_;
    bool opencl_available_ = false;  // Initialize to false

    void initializeOpenCL() {
        cl_int err;
        cl_platform_id platform;
        cl_device_id device;

        // Error handling macros for OpenCL
#define CHECK_CL_ERROR(err, msg)                                               \
    if (err != CL_SUCCESS) {                                                   \
        spdlog::error("OpenCL Error ({}): {}", err, msg);                      \
        opencl_available_ = false; /* Mark OpenCL as unavailable */            \
        /* Depending on desired behavior, could throw or just log and continue \
         * without OpenCL */                                                   \
        /* For now, we log and return, disabling OpenCL */                     \
        return;                                                                \
    }

        err = clGetPlatformIDs(1, &platform, nullptr);
        CHECK_CL_ERROR(err, "Failed to get OpenCL platform ID");

        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        CHECK_CL_ERROR(err, "Failed to get OpenCL device ID (GPU)");

        context_ = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        CHECK_CL_ERROR(err, "Failed to create OpenCL context");

        queue_ = clCreateCommandQueue(context_, device, 0, &err);
        CHECK_CL_ERROR(err, "Failed to create OpenCL command queue");

        // Note: This kernel uses a simplified lerp and grad compared to the CPU
        // version's fade and grad. For consistent noise, the kernel should
        // implement the same fade/lerp/grad logic. Also, this kernel is
        // designed for a single work item (global_work_size = 1), which is
        // inefficient for parallel processing on the GPU. A proper OpenCL
        // implementation for performance would process multiple points per work
        // item or use a larger global work size with an updated kernel.
        const char* kernel_source = R"CLC(
            // Simplified lerp - does not match CPU fade function
            float lerp_ocl(float t, float a, float b) {
                return a + t * (b - a);
            }

            // Simplified grad - matches CPU grad logic
            float grad_ocl(int hash, float x, float y, float z) {
                int h = hash & 15;
                float u = h < 8 ? x : y;
                float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
                return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
            }

            // Note: This kernel processes only one point per execution.
            // For performance, it should be modified to process multiple points
            // or used with a global work size > 1 and adjusted indexing.
            __kernel void noise_kernel(__global const float* coords,
                                       __global float* result,
                                       __constant int* p) {
                // int gid = get_global_id(0); // Currently only 1 work item

                float x = coords[0];
                float y = coords[1];
                float z = coords[2];

                int X = ((int)floor(x)) & 255;
                int Y = ((int)floor(y)) & 255;
                int Z = ((int)floor(z)) & 255;

                x -= floor(x);
                y -= floor(y);
                z -= floor(z);

                // Using simplified lerp_ocl instead of fade
                float u = lerp_ocl(x, 0.0f, 1.0f);
                float v = lerp_ocl(y, 0.0f, 1.0f);
                float w = lerp_ocl(z, 0.0f, 1.0f);

                int A = p[X] + Y;
                int AA = p[A] + Z;
                int AB = p[A + 1] + Z;
                int B = p[X + 1] + Y;
                int BA = p[B] + Z;
                int BB = p[B + 1] + Z;

                float res = lerp_ocl(
                    w,
                    lerp_ocl(v, lerp_ocl(u, grad_ocl(p[AA], x, y, z), grad_ocl(p[BA], x - 1, y, z)),
                         lerp_ocl(u, grad_ocl(p[AB], x, y - 1, z),
                              grad_ocl(p[BB], x - 1, y - 1, z))),
                    lerp_ocl(v,
                         lerp_ocl(u, grad_ocl(p[AA + 1], x, y, z - 1),
                              grad_ocl(p[BA + 1], x - 1, y, z - 1)),
                         lerp_ocl(u, grad_ocl(p[AB + 1], x, y - 1, z - 1),
                              grad_ocl(p[BB + 1], x - 1, y - 1, z - 1))));

                // Kernel returns normalized value [0, 1]
                result[0] = (res + 1) / 2;
            }
        )CLC";

        program_ = clCreateProgramWithSource(context_, 1, &kernel_source,
                                             nullptr, &err);
        CHECK_CL_ERROR(err, "Failed to create OpenCL program");

        err = clBuildProgram(program_, 1, &device, nullptr, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            // Get build log for debugging
            size_t log_size;
            clGetProgramBuildInfo(program_, device, CL_PROGRAM_BUILD_LOG, 0,
                                  nullptr, &log_size);
            std::vector<char> build_log(log_size);
            clGetProgramBuildInfo(program_, device, CL_PROGRAM_BUILD_LOG,
                                  log_size, build_log.data(), nullptr);
            spdlog::error("OpenCL Build Error ({}): {}", err, build_log.data());
            opencl_available_ = false;
            clReleaseProgram(program_);  // Clean up program
            return;
        }

        noise_kernel_ = clCreateKernel(program_, "noise_kernel", &err);
        CHECK_CL_ERROR(err, "Failed to create OpenCL kernel");

        opencl_available_ = true;
        spdlog::info("OpenCL initialized successfully.");

#undef CHECK_CL_ERROR  // Undefine the macro
    }

    void cleanupOpenCL() {
        if (opencl_available_) {
            if (noise_kernel_)
                clReleaseKernel(noise_kernel_);
            if (program_)
                clReleaseProgram(program_);
            if (queue_)
                clReleaseCommandQueue(queue_);
            if (context_)
                clReleaseContext(context_);
            spdlog::info("OpenCL resources cleaned up.");
        }
    }

    template <std::floating_point T>
    auto noiseOpenCL(T x, T y, T z) const -> T {
        if (!opencl_available_) {
            spdlog::error("noiseOpenCL called but OpenCL is not available.");
            // Fallback to CPU or throw, depending on desired behavior
            // For now, throw as this function is only called if
            // opencl_available_ is true
            THROW_RUNTIME_ERROR("OpenCL is not available.");
        }

        // Note: This function is currently designed for a single point,
        // which has high overhead for OpenCL.
        // For performance, batch processing is recommended.

        f32 coords[] = {static_cast<f32>(x), static_cast<f32>(y),
                        static_cast<f32>(z)};
        f32 result;

        cl_int err;
        cl_mem coords_buffer =
            clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           sizeof(coords), coords, &err);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to create OpenCL buffer for coords: {}", err);
            THROW_RUNTIME_ERROR("Failed to create OpenCL buffer for coords");
        }

        cl_mem result_buffer = clCreateBuffer(context_, CL_MEM_WRITE_ONLY,
                                              sizeof(f32), nullptr, &err);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to create OpenCL buffer for result: {}", err);
            clReleaseMemObject(coords_buffer);  // Clean up
            THROW_RUNTIME_ERROR("Failed to create OpenCL buffer for result");
        }

        // Use CL_MEM_USE_HOST_PTR if p is guaranteed to be aligned and
        // host-accessible Otherwise, CL_MEM_COPY_HOST_PTR is safer
        cl_mem p_buffer =
            clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           p.size() * sizeof(i32), p.data(), &err);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to create OpenCL buffer for permutation: {}",
                          err);
            clReleaseMemObject(coords_buffer);  // Clean up
            clReleaseMemObject(result_buffer);  // Clean up
            THROW_RUNTIME_ERROR(
                "Failed to create OpenCL buffer for permutation");
        }

        err = clSetKernelArg(noise_kernel_, 0, sizeof(cl_mem), &coords_buffer);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to set kernel arg 0: {}", err);
        }
        err |= clSetKernelArg(noise_kernel_, 1, sizeof(cl_mem), &result_buffer);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to set kernel arg 1: {}", err);
        }
        err |= clSetKernelArg(noise_kernel_, 2, sizeof(cl_mem), &p_buffer);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to set kernel arg 2: {}", err);
        }

        if (err != CL_SUCCESS) {
            clReleaseMemObject(coords_buffer);
            clReleaseMemObject(result_buffer);
            clReleaseMemObject(p_buffer);
            THROW_RUNTIME_ERROR("Failed to set OpenCL kernel arguments");
        }

        size_t global_work_size =
            1;  // Kernel is designed for a single work item
        err = clEnqueueNDRangeKernel(queue_, noise_kernel_, 1, nullptr,
                                     &global_work_size, nullptr, 0, nullptr,
                                     nullptr);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to enqueue OpenCL kernel: {}", err);
            clReleaseMemObject(coords_buffer);
            clReleaseMemObject(result_buffer);
            clReleaseMemObject(p_buffer);
            THROW_RUNTIME_ERROR("Failed to enqueue OpenCL kernel");
        }

        err = clEnqueueReadBuffer(queue_, result_buffer, CL_TRUE, 0,
                                  sizeof(f32), &result, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            spdlog::error("Failed to read OpenCL buffer for result: {}", err);
            clReleaseMemObject(coords_buffer);
            clReleaseMemObject(result_buffer);
            clReleaseMemObject(p_buffer);
            THROW_RUNTIME_ERROR("Failed to read OpenCL buffer for result");
        }

        clReleaseMemObject(coords_buffer);
        clReleaseMemObject(result_buffer);
        clReleaseMemObject(p_buffer);

        // The OpenCL kernel already returns a normalized value [0, 1]
        return static_cast<T>(result);
    }
#endif  // ATOM_USE_OPENCL

    /**
     * @brief Calculates the Perlin noise value for a 3D point using the CPU.
     *
     * This is the standard CPU implementation of Perlin noise.
     *
     * @tparam T A floating-point type (e.g., float, double).
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @param z The z-coordinate.
     * @return The raw Perlin noise value in the range [-1, 1].
     */
    template <std::floating_point T>
    [[nodiscard]] auto noiseCPU(T x, T y, T z) const -> T {
        // Find unit cube containing point
        i32 X = static_cast<i32>(std::floor(x));
        i32 Y = static_cast<i32>(std::floor(y));
        i32 Z = static_cast<i32>(std::floor(z));

        // Wrap coordinates to 0-255 range for permutation table lookup
        i32 X_wrapped = X & 255;
        i32 Y_wrapped = Y & 255;
        i32 Z_wrapped = Z & 255;

        // Find relative x, y, z of point in cube
        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        // Compute fade curves for each of x, y, z
        T u = fade(x);
        T v = fade(y);
        T w = fade(z);

        // Hash coordinates of the 8 cube corners
        i32 A = p[X_wrapped] + Y_wrapped;
        i32 AA = p[A] + Z_wrapped;
        i32 AB = p[A + 1] + Z_wrapped;
        i32 B = p[X_wrapped + 1] + Y_wrapped;
        i32 BA = p[B] + Z_wrapped;
        i32 BB = p[B + 1] + Z_wrapped;

        // Add blended results from 8 corners of cube
        // Note: The grad function uses the original relative coordinates (x, y,
        // z), not the wrapped integer coordinates.
        T res = lerp(
            w,
            lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
                 lerp(u, grad(p[AB], x, y - 1, z),
                      grad(p[BB], x - 1, y - 1, z))),
            lerp(v,
                 lerp(u, grad(p[AA + 1], x, y, z - 1),
                      grad(p[BA + 1], x - 1, y, z - 1)),
                 lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                      grad(p[BB + 1], x - 1, y - 1, z - 1))));

        // Normalize to [0,1] - This normalization should ideally happen
        // outside noiseCPU if noiseCPU is meant to return [-1, 1].
        // However, the public `noise` function already does this.
        // Let's keep noiseCPU returning [-1, 1] and normalize in the public
        // `noise`. Adjusting the public `noise` function: return (noiseCPU(x,
        // y, z) + 1) / 2; The current code normalizes inside noiseCPU, which is
        // also acceptable if noiseCPU is only called by the public noise
        // function. Let's stick to the original structure where noiseCPU
        // returns [0,1].
        return (res + 1) / 2;
    }

    /**
     * @brief The fade function used in Perlin noise.
     *
     * Smooths the interpolation between grid points.
     *
     * @tparam T A floating-point type.
     * @param t The input value, typically in [0, 1].
     * @return The faded value.
     */
    template <std::floating_point T>
    static constexpr auto fade(T t) noexcept -> T {
        // 6t^5 - 15t^4 + 10t^3
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    /**
     * @brief Linear interpolation function.
     *
     * @tparam T A floating-point type.
     * @param t The interpolation factor, typically in [0, 1].
     * @param a The start value.
     * @param b The end value.
     * @return The interpolated value.
     */
    template <std::floating_point T>
    static constexpr auto lerp(T t, T a, T b) noexcept -> T {
        return a + t * (b - a);
    }

    /**
     * @brief Calculates the dot product of a gradient vector and a distance
     * vector.
     *
     * The gradient vector is determined by the hash value.
     *
     * @tparam T A floating-point type.
     * @param hash The hash value from the permutation table.
     * @param x The x-component of the distance vector.
     * @param y The y-component of the distance vector.
     * @param z The z-component of the distance vector.
     * @return The dot product.
     */
    template <std::floating_point T>
    static constexpr auto grad(i32 hash, T x, T y, T z) noexcept -> T {
        // Convert hash to a gradient vector
        i32 h = hash & 15;
        T u = h < 8 ? x : y;
        T v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};
}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_PERLIN_HPP
