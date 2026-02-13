#ifndef ATOM_ALGORITHM_GRAPHICS_PERLIN_OPENCL_HPP
#define ATOM_ALGORITHM_GRAPHICS_PERLIN_OPENCL_HPP

#ifdef ATOM_USE_OPENCL

#include <CL/cl.h>
#include <concepts>
#include <vector>

#include "../core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief OpenCL-accelerated noise computation helper.
 *
 * Encapsulates all OpenCL resources and operations for GPU-accelerated
 * Perlin noise generation. Used by PerlinNoise via composition.
 */
class PerlinNoiseOpenCL {
public:
    PerlinNoiseOpenCL() { initializeOpenCL(); }

    ~PerlinNoiseOpenCL() { cleanupOpenCL(); }

    // Non-copyable
    PerlinNoiseOpenCL(const PerlinNoiseOpenCL&) = delete;
    auto operator=(const PerlinNoiseOpenCL&) -> PerlinNoiseOpenCL& = delete;

    // Movable
    PerlinNoiseOpenCL(PerlinNoiseOpenCL&& other) noexcept
        : context_(other.context_),
          queue_(other.queue_),
          program_(other.program_),
          noise_kernel_(other.noise_kernel_),
          opencl_available_(other.opencl_available_) {
        other.opencl_available_ = false;
    }

    auto operator=(PerlinNoiseOpenCL&& other) noexcept -> PerlinNoiseOpenCL& {
        if (this != &other) {
            cleanupOpenCL();
            context_ = other.context_;
            queue_ = other.queue_;
            program_ = other.program_;
            noise_kernel_ = other.noise_kernel_;
            opencl_available_ = other.opencl_available_;
            other.opencl_available_ = false;
        }
        return *this;
    }

    [[nodiscard]] bool isAvailable() const noexcept {
        return opencl_available_;
    }

    template <std::floating_point T>
    auto computeNoise(T x, T y, T z,
                      const std::vector<i32>& perm) const -> T {
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
                << boost::errinfo_api_function("computeNoise");
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
                << boost::errinfo_api_function("computeNoise");
#else
            THROW_RUNTIME_ERROR("Failed to create OpenCL buffer for result");
#endif
        }

        cl_mem p_buffer =
            clCreateBuffer(context_, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                           perm.size() * sizeof(i32), const_cast<i32*>(perm.data()), &err);
        if (err != CL_SUCCESS) {
#ifdef ATOM_USE_BOOST
            throw boost::enable_error_info(std::runtime_error(
                "Failed to create OpenCL buffer for permutation"))
                << boost::errinfo_api_function("computeNoise");
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
                << boost::errinfo_api_function("computeNoise");
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
                << boost::errinfo_api_function("computeNoise");
#else
            THROW_RUNTIME_ERROR("Failed to read OpenCL buffer for result");
#endif
        }

        clReleaseMemObject(coords_buffer);
        clReleaseMemObject(result_buffer);
        clReleaseMemObject(p_buffer);

        return static_cast<T>(result);
    }

private:
    cl_context context_{};
    cl_command_queue queue_{};
    cl_program program_{};
    cl_kernel noise_kernel_{};
    bool opencl_available_{false};

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

                int A = perm_[X] + Y;
                int AA = perm_[A] + Z;
                int AB = perm_[A + 1] + Z;
                int B = perm_[X + 1] + Y;
                int BA = perm_[B] + Z;
                int BB = perm_[B + 1] + Z;

                float res = lerp(
                    w,
                    lerp(v, lerp(u, grad(perm_[AA], x, y, z), grad(perm_[BA], x - 1, y, z)),
                         lerp(u, grad(perm_[AB], x, y - 1, z),
                              grad(perm_[BB], x - 1, y - 1, z))),
                    lerp(v,
                         lerp(u, grad(perm_[AA + 1], x, y, z - 1),
                              grad(perm_[BA + 1], x - 1, y, z - 1)),
                         lerp(u, grad(perm_[AB + 1], x, y - 1, z - 1),
                              grad(perm_[BB + 1], x - 1, y - 1, z - 1))));
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
            opencl_available_ = false;
        }
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_USE_OPENCL

#endif  // ATOM_ALGORITHM_GRAPHICS_PERLIN_OPENCL_HPP
