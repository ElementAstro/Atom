#ifndef ATOM_ALGORITHM_PERLIN_HPP
#define ATOM_ALGORITHM_PERLIN_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <numeric>
#include <random>
#include <span>
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

    ~PerlinNoise() {
#ifdef ATOM_USE_OPENCL
        cleanupOpenCL();
#endif
    }

    template <std::floating_point T>
    [[nodiscard]] auto noise(T x, T y, T z) const -> T {
#ifdef ATOM_USE_OPENCL
        if (opencl_available_) {
            return noiseOpenCL(x, y, z);
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
        f64 /*lacunarity*/,
        i32 seed = std::default_random_engine::default_seed) const
        -> std::vector<std::vector<f64>> {
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
    std::vector<i32> p;

#ifdef ATOM_USE_OPENCL
    cl_context context_;
    cl_command_queue queue_;
    cl_program program_;
    cl_kernel noise_kernel_;
    bool opencl_available_;

    void initializeOpenCL() {
        cl_int err;
        cl_platform_id platform;
        cl_device_id device;

        err = clGetPlatformIDs(1, &platform, nullptr);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to get OpenCL platform ID"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to get OpenCL platform ID");
#endif
        }

        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to get OpenCL device ID"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to get OpenCL device ID");
#endif
        }

        context_ = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL context"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL context");
#endif
        }

        queue_ = clCreateCommandQueue(context_, device, 0, &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL command queue"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL command queue");
#endif
        }

        const char* kernel_source = R"CLC(
            __kernel void noise_kernel(__global const float* coords,
                                       __global float* result,
                                       __constant int* p) {
                int gid = get_global_id(0);

                float x = coords[gid * 3];
                float y = coords[gid * 3 + 1];
                float z = coords[gid * 3 + 2];

                int X = ((int)floor(x)) & 255;
                int Y = ((int)floor(y)) & 255;
                int Z = ((int)floor(z)) & 255;

                x -= floor(x);
                y -= floor(y);
                z -= floor(z);

                float u = lerp(x, 0.0f, 1.0f); // 简化的fade函数
                float v = lerp(y, 0.0f, 1.0f);
                float w = lerp(z, 0.0f, 1.0f);

                int A = p[X] + Y;
                int AA = p[A] + Z;
                int AB = p[A + 1] + Z;
                int B = p[X + 1] + Y;
                int BA = p[B] + Z;
                int BB = p[B + 1] + Z;

                float res = lerp(
                    w,
                    lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)),
                         lerp(u, grad(p[AB], x, y - 1, z),
                              grad(p[BB], x - 1, y - 1, z))),
                    lerp(v,
                         lerp(u, grad(p[AA + 1], x, y, z - 1),
                              grad(p[BA + 1], x - 1, y, z - 1)),
                         lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                              grad(p[BB + 1], x - 1, y - 1, z - 1))));
                result[gid] = (res + 1) / 2;
            }

            float lerp(float t, float a, float b) {
                return a + t * (b - a);
            }

            float grad(int hash, float x, float y, float z) {
                int h = hash & 15;
                float u = h < 8 ? x : y;
                float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
                return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
            }
        )CLC";

        program_ = clCreateProgramWithSource(context_, 1, &kernel_source,
                                             nullptr, &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL program"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL program");
#endif
        }

        err = clBuildProgram(program_, 1, &device, nullptr, nullptr, nullptr);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to build OpenCL program"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to build OpenCL program");
#endif
        }

        noise_kernel_ = clCreateKernel(program_, "noise_kernel", &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL kernel"))
                << boost::errinfo_api_function("initializeOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL kernel");
#endif
        }

        opencl_available_ = true;
    }

    void cleanupOpenCL() {
        if (opencl_available_) {
            clReleaseKernel(noise_kernel_);
            clReleaseProgram(program_);
            clReleaseCommandQueue(queue_);
            clReleaseContext(context_);
        }
    }

    template <std::floating_point T>
    auto noiseOpenCL(T x, T y, T z) const -> T {
        f32 coords[] = {static_cast<f32>(x), static_cast<f32>(y),
                        static_cast<f32>(z)};
        f32 result;

        cl_int err;
        cl_mem coords_buffer =
            clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           sizeof(coords), coords, &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL buffer for coords"))
                << boost::errinfo_api_function("noiseOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL buffer for coords");
#endif
        }

        cl_mem result_buffer = clCreateBuffer(context_, CL_MEM_WRITE_ONLY,
                                              sizeof(f32), nullptr, &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to create OpenCL buffer for result"))
                << boost::errinfo_api_function("noiseOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL buffer for result");
#endif
        }

        cl_mem p_buffer =
            clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           p.size() * sizeof(i32), p.data(), &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(std::runtime_error(
                "Failed to create OpenCL buffer for permutation"))
                << boost::errinfo_api_function("noiseOpenCL");
#else
            THROW_RUNTIME_ERROR(
                "Failed to create OpenCL buffer for permutation");
#endif
        }

        clSetKernelArg(noise_kernel_, 0, sizeof(cl_mem), &coords_buffer);
        clSetKernelArg(noise_kernel_, 1, sizeof(cl_mem), &result_buffer);
        clSetKernelArg(noise_kernel_, 2, sizeof(cl_mem), &p_buffer);

        size_t global_work_size = 1;
        err = clEnqueueNDRangeKernel(queue_, noise_kernel_, 1, nullptr,
                                     &global_work_size, nullptr, 0, nullptr,
                                     nullptr);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to enqueue OpenCL kernel"))
                << boost::errinfo_api_function("noiseOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to enqueue OpenCL kernel");
#endif
        }

        err = clEnqueueReadBuffer(queue_, result_buffer, CL_TRUE, 0,
                                  sizeof(f32), &result, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(
                std::runtime_error("Failed to read OpenCL buffer for result"))
                << boost::errinfo_api_function("noiseOpenCL");
#else
            THROW_RUNTIME_ERROR("Failed to read OpenCL buffer for result");
#endif
        }

        clReleaseMemObject(coords_buffer);
        clReleaseMemObject(result_buffer);
        clReleaseMemObject(p_buffer);

        return static_cast<T>(result);
    }
#endif  // ATOM_USE_OPENCL

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
#ifdef USE_SIMD
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
        i32 A = p[X] + Y;
        i32 AA = p[A] + Z;
        i32 AB = p[A + 1] + Z;
        i32 B = p[X + 1] + Y;
        i32 BA = p[B] + Z;
        i32 BB = p[B + 1] + Z;

        // Add blended results from 8 corners of cube
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

#endif  // ATOM_ALGORITHM_PERLIN_HPP