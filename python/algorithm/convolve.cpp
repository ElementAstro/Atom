// convolve_bindings.cpp
#include "atom/algorithm/convolve.hpp"
#include <pybind11/complex.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;
using namespace pybind11::literals;

// Helper function to convert numpy array to vector of vectors
std::vector<std::vector<double>> numpy_to_vector2d(py::array_t<double> array) {
    if (array.ndim() != 2)
        throw std::runtime_error("Array must be 2-dimensional");

    auto arr = array.unchecked<2>();
    std::vector<std::vector<double>> result(arr.shape(0));

    for (ssize_t i = 0; i < arr.shape(0); i++) {
        result[i].resize(arr.shape(1));
        for (ssize_t j = 0; j < arr.shape(1); j++) {
            result[i][j] = arr(i, j);
        }
    }

    return result;
}

// Helper function to convert vector of vectors to numpy array
py::array_t<double> vector2d_to_numpy(
    const std::vector<std::vector<double>>& vec) {
    if (vec.empty())
        return py::array_t<double>({0, 0});

    py::array_t<double> result({static_cast<ssize_t>(vec.size()),
                                static_cast<ssize_t>(vec[0].size())});
    auto arr = result.mutable_unchecked<2>();

    for (ssize_t i = 0; i < arr.shape(0); i++) {
        for (ssize_t j = 0; j < arr.shape(1); j++) {
            arr(i, j) = vec[i][j];
        }
    }

    return result;
}

// Helper function to convert complex vectors to numpy array
py::array_t<std::complex<double>> complex_vector2d_to_numpy(
    const std::vector<std::vector<std::complex<double>>>& vec) {
    if (vec.empty())
        return py::array_t<std::complex<double>>({0, 0});

    py::array_t<std::complex<double>> result(
        {static_cast<ssize_t>(vec.size()),
         static_cast<ssize_t>(vec[0].size())});
    auto arr = result.mutable_unchecked<2>();

    for (ssize_t i = 0; i < arr.shape(0); i++) {
        for (ssize_t j = 0; j < arr.shape(1); j++) {
            arr(i, j) = vec[i][j];
        }
    }

    return result;
}

// Helper function to convert numpy array to vector of vectors of complex
std::vector<std::vector<std::complex<double>>> numpy_to_complex_vector2d(
    py::array_t<std::complex<double>> array) {
    if (array.ndim() != 2)
        throw std::runtime_error("Array must be 2-dimensional");

    auto arr = array.unchecked<2>();
    std::vector<std::vector<std::complex<double>>> result(arr.shape(0));

    for (ssize_t i = 0; i < arr.shape(0); i++) {
        result[i].resize(arr.shape(1));
        for (ssize_t j = 0; j < arr.shape(1); j++) {
            result[i][j] = arr(i, j);
        }
    }

    return result;
}

PYBIND11_MODULE(convolve, m) {
    m.doc() = R"pbdoc(
        Convolution and Deconvolution Operations
        ----------------------------------------

        This module provides functions for performing 2D convolution and deconvolution
        operations on signals or images, with support for multi-threading and
        optional OpenCL acceleration.

        **Key Functions**:
            - convolve_2d: Performs 2D convolution
            - deconvolve_2d: Performs 2D deconvolution
            - dft_2d: Computes 2D Discrete Fourier Transform
            - idft_2d: Computes inverse 2D Discrete Fourier Transform
            - generate_gaussian_kernel: Creates a 2D Gaussian kernel
            - gaussian_blur: Shortcut for applying Gaussian blur
            - detect_edges_sobel: Detects edges using Sobel operators

        **Convenience Functions**:
            - sobel_kernel_x/y: Returns Sobel kernels for edge detection
            - laplacian_kernel: Returns a Laplacian kernel
            - box_blur_kernel: Returns a box blur kernel
            - visualize_kernel: Visualizes a kernel using matplotlib
            - compare_images: Compares original and processed images

        **Dependencies**:
            - numpy: For array manipulation
            - matplotlib: For visualization functions (optional)
    )pbdoc";

    // Register exception translation
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 2D Convolution
    m.def(
        "convolve_2d",
        [](py::array_t<double> input, py::array_t<double> kernel,
           int num_threads) {
            auto cpp_input = numpy_to_vector2d(input);
            auto cpp_kernel = numpy_to_vector2d(kernel);
            auto result =
                atom::algorithm::convolve2D(cpp_input, cpp_kernel, num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("input"), py::arg("kernel"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Performs 2D convolution of an input with a kernel.

        Args:
            input (numpy.ndarray): 2D matrix to be convolved
            kernel (numpy.ndarray): 2D kernel to convolve with
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Result of convolution

        Example:
            >>> import numpy as np
            >>> from atom.algorithm import convolve
            >>> input = np.random.rand(100, 100)
            >>> kernel = np.ones((3, 3)) / 9  # Simple averaging filter
            >>> result = convolve.convolve_2d(input, kernel)
        )pbdoc");

    // 2D Deconvolution
    m.def(
        "deconvolve_2d",
        [](py::array_t<double> signal, py::array_t<double> kernel,
           int num_threads) {
            auto cpp_signal = numpy_to_vector2d(signal);
            auto cpp_kernel = numpy_to_vector2d(kernel);
            auto result = atom::algorithm::deconvolve2D(cpp_signal, cpp_kernel,
                                                        num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("signal"), py::arg("kernel"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Performs 2D deconvolution (inverse of convolution).

        Args:
            signal (numpy.ndarray): 2D matrix signal (result of convolution)
            kernel (numpy.ndarray): 2D kernel used for convolution
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Original input recovered via deconvolution

        Example:
            >>> import numpy as np
            >>> from atom.algorithm import convolve
            >>> original = np.random.rand(100, 100)
            >>> kernel = np.ones((3, 3)) / 9
            >>> convolved = convolve.convolve_2d(original, kernel)
            >>> recovered = convolve.deconvolve_2d(convolved, kernel)
            >>> # recovered should be close to original
        )pbdoc");

    // 2D Discrete Fourier Transform
    m.def(
        "dft_2d",
        [](py::array_t<double> signal, int num_threads) {
            auto cpp_signal = numpy_to_vector2d(signal);
            auto result = atom::algorithm::dfT2D(cpp_signal, num_threads);
            return complex_vector2d_to_numpy(result);
        },
        py::arg("signal"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Computes 2D Discrete Fourier Transform.

        Args:
            signal (numpy.ndarray): 2D input signal in spatial domain
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Frequency domain representation (complex values)

        Example:
            >>> import numpy as np
            >>> from atom.algorithm import convolve
            >>> signal = np.random.rand(64, 64)
            >>> spectrum = convolve.dft_2d(signal)
            >>> # spectrum contains complex values
        )pbdoc");

    // Inverse 2D Discrete Fourier Transform
    m.def(
        "idft_2d",
        [](py::array_t<std::complex<double>> spectrum, int num_threads) {
            auto cpp_spectrum = numpy_to_complex_vector2d(spectrum);
            auto result = atom::algorithm::idfT2D(cpp_spectrum, num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("spectrum"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Computes inverse 2D Discrete Fourier Transform.

        Args:
            spectrum (numpy.ndarray): 2D input in frequency domain (complex values)
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Spatial domain representation (real values)

        Example:
            >>> import numpy as np
            >>> from atom.algorithm import convolve
            >>> signal = np.random.rand(64, 64)
            >>> spectrum = convolve.dft_2d(signal)
            >>> reconstructed = convolve.idft_2d(spectrum)
            >>> # reconstructed should be close to signal
        )pbdoc");

    // Generate Gaussian Kernel
    m.def(
        "generate_gaussian_kernel",
        [](int size, double sigma) {
            auto result = atom::algorithm::generateGaussianKernel(size, sigma);
            return vector2d_to_numpy(result);
        },
        py::arg("size"), py::arg("sigma"),
        R"pbdoc(
        Generates a 2D Gaussian kernel for image filtering.

        Args:
            size (int): Size of the kernel (should be odd)
            sigma (float): Standard deviation of the Gaussian distribution

        Returns:
            numpy.ndarray: Gaussian kernel

        Example:
            >>> from atom.algorithm import convolve
            >>> kernel = convolve.generate_gaussian_kernel(5, 1.0)
            >>> # Use kernel for image filtering
        )pbdoc");

    // Apply Gaussian Filter
    m.def(
        "apply_gaussian_filter",
        [](py::array_t<double> image, py::array_t<double> kernel) {
            auto cpp_image = numpy_to_vector2d(image);
            auto cpp_kernel = numpy_to_vector2d(kernel);
            auto result =
                atom::algorithm::applyGaussianFilter(cpp_image, cpp_kernel);
            return vector2d_to_numpy(result);
        },
        py::arg("image"), py::arg("kernel"),
        R"pbdoc(
        Applies a Gaussian filter to an image.

        Args:
            image (numpy.ndarray): Input image as 2D matrix
            kernel (numpy.ndarray): Gaussian kernel to apply

        Returns:
            numpy.ndarray: Filtered image

        Example:
            >>> import numpy as np
            >>> from atom.algorithm import convolve
            >>> image = np.random.rand(100, 100)
            >>> kernel = convolve.generate_gaussian_kernel(5, 1.0)
            >>> filtered = convolve.apply_gaussian_filter(image, kernel)
        )pbdoc");

#if USE_OPENCL
    // OpenCL-accelerated 2D Convolution
    m.def(
        "convolve_2d_opencl",
        [](py::array_t<double> input, py::array_t<double> kernel,
           int num_threads) {
            auto cpp_input = numpy_to_vector2d(input);
            auto cpp_kernel = numpy_to_vector2d(kernel);
            auto result = atom::algorithm::convolve2DOpenCL(
                cpp_input, cpp_kernel, num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("input"), py::arg("kernel"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Performs 2D convolution using OpenCL acceleration.

        Args:
            input (numpy.ndarray): 2D matrix to be convolved
            kernel (numpy.ndarray): 2D kernel to convolve with
            num_threads (int, optional): Used for fallback if OpenCL fails. Defaults to all available cores.

        Returns:
            numpy.ndarray: Result of convolution
        )pbdoc");

    // OpenCL-accelerated 2D Deconvolution
    m.def(
        "deconvolve_2d_opencl",
        [](py::array_t<double> signal, py::array_t<double> kernel,
           int num_threads) {
            auto cpp_signal = numpy_to_vector2d(signal);
            auto cpp_kernel = numpy_to_vector2d(kernel);
            auto result = atom::algorithm::deconvolve2DOpenCL(
                cpp_signal, cpp_kernel, num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("signal"), py::arg("kernel"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Performs 2D deconvolution using OpenCL acceleration.

        Args:
            signal (numpy.ndarray): 2D matrix signal (result of convolution)
            kernel (numpy.ndarray): 2D kernel used for convolution
            num_threads (int, optional): Used for fallback if OpenCL fails. Defaults to all available cores.

        Returns:
            numpy.ndarray: Original input recovered via deconvolution
        )pbdoc");
#endif

    // Add a utility function to check if OpenCL is available
    m.def(
        "has_opencl_support",
        []() {
#if USE_OPENCL
            return true;
#else
            return false;
#endif
        },
        "Returns True if OpenCL support is available, False otherwise");

    // Add a utility function to check if SIMD is available
    m.def(
        "has_simd_support",
        []() {
#if USE_SIMD
            return true;
#else
            return false;
#endif
        },
        "Returns True if SIMD support is available, False otherwise");

    // Add information about the number of available threads
    m.attr("available_threads") = atom::algorithm::availableThreads;

    // Predefined kernels for common operations
    m.def(
        "sobel_kernel_x",
        []() {
            std::vector<std::vector<double>> kernel = {
                {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
            return vector2d_to_numpy(kernel);
        },
        "Returns a Sobel kernel for x-direction edge detection");

    m.def(
        "sobel_kernel_y",
        []() {
            std::vector<std::vector<double>> kernel = {
                {-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
            return vector2d_to_numpy(kernel);
        },
        "Returns a Sobel kernel for y-direction edge detection");

    m.def(
        "laplacian_kernel",
        []() {
            std::vector<std::vector<double>> kernel = {
                {0, 1, 0}, {1, -4, 1}, {0, 1, 0}};
            return vector2d_to_numpy(kernel);
        },
        "Returns a Laplacian kernel for edge detection");

    m.def(
        "box_blur_kernel",
        [](int size) {
            if (size % 2 == 0) {
                throw std::invalid_argument("Kernel size must be odd");
            }

            std::vector<std::vector<double>> kernel(
                size, std::vector<double>(size, 1.0 / (size * size)));
            return vector2d_to_numpy(kernel);
        },
        py::arg("size") = 3, "Returns a box blur kernel of specified size");

    // Shortcut for Gaussian blur
    m.def(
        "gaussian_blur",
        [](py::array_t<double> image, int kernel_size, double sigma,
           int num_threads) {
            auto kernel =
                atom::algorithm::generateGaussianKernel(kernel_size, sigma);
            auto cpp_image = numpy_to_vector2d(image);
            auto result =
                atom::algorithm::convolve2D(cpp_image, kernel, num_threads);
            return vector2d_to_numpy(result);
        },
        py::arg("image"), py::arg("kernel_size") = 5, py::arg("sigma") = 1.0,
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Applies Gaussian blur to an image.

        This is a convenience function that generates a Gaussian kernel and applies it.

        Args:
            image (numpy.ndarray): Input image as 2D matrix
            kernel_size (int, optional): Size of the Gaussian kernel. Defaults to 5.
            sigma (float, optional): Standard deviation of the Gaussian. Defaults to 1.0.
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Blurred image
       )pbdoc");

    // Edge detection shortcuts
    m.def(
        "detect_edges_sobel",
        [](py::array_t<double> image, int num_threads) {
            auto cpp_image = numpy_to_vector2d(image);

            // Create Sobel kernels
            std::vector<std::vector<double>> kernel_x = {
                {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};

            std::vector<std::vector<double>> kernel_y = {
                {-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

            // Apply both kernels
            auto gradient_x =
                atom::algorithm::convolve2D(cpp_image, kernel_x, num_threads);
            auto gradient_y =
                atom::algorithm::convolve2D(cpp_image, kernel_y, num_threads);

            // Compute magnitude of gradient
            std::vector<std::vector<double>> magnitude(
                gradient_x.size(), std::vector<double>(gradient_x[0].size()));

            for (size_t i = 0; i < gradient_x.size(); i++) {
                for (size_t j = 0; j < gradient_x[0].size(); j++) {
                    magnitude[i][j] =
                        std::sqrt(gradient_x[i][j] * gradient_x[i][j] +
                                  gradient_y[i][j] * gradient_y[i][j]);
                }
            }

            return vector2d_to_numpy(magnitude);
        },
        py::arg("image"),
        py::arg("num_threads") = atom::algorithm::availableThreads,
        R"pbdoc(
        Detects edges in an image using Sobel operators.

        Args:
            image (numpy.ndarray): Input image as 2D matrix
            num_threads (int, optional): Number of threads to use. Defaults to all available cores.

        Returns:
            numpy.ndarray: Edge magnitude image
       )pbdoc");

    // Visualization utilities
    m.def(
        "visualize_kernel",
        [](py::array_t<double> kernel) {
            py::object plt = py::module::import("matplotlib.pyplot");
            plt.attr("figure")();
            plt.attr("imshow")(kernel, py::arg("cmap") = "viridis");
            plt.attr("colorbar")();
            plt.attr("title")("Kernel Visualization");
            plt.attr("show")();
        },
        py::arg("kernel"),
        R"pbdoc(
        Visualizes a convolution kernel using matplotlib.

        Args:
            kernel (numpy.ndarray): 2D kernel to visualize

        Note:
            This function requires matplotlib to be installed.
       )pbdoc");

    // Function to compare original and processed images
    m.def(
        "compare_images",
        [](py::array_t<double> original, py::array_t<double> processed,
           const std::string& title1, const std::string& title2) {
            py::object plt = py::module::import("matplotlib.pyplot");

            plt.attr("figure")(py::arg("figsize") = py::make_tuple(12, 5));

            plt.attr("subplot")(1, 2, 1);
            plt.attr("imshow")(original, py::arg("cmap") = "gray");
            plt.attr("title")(title1);
            plt.attr("axis")("off");

            plt.attr("subplot")(1, 2, 2);
            plt.attr("imshow")(processed, py::arg("cmap") = "gray");
            plt.attr("title")(title2);
            plt.attr("axis")("off");

            plt.attr("tight_layout")();
            plt.attr("show")();
        },
        py::arg("original"), py::arg("processed"),
        py::arg("title1") = "Original", py::arg("title2") = "Processed",
        R"pbdoc(
        Compares original and processed images side by side.

        Args:
            original (numpy.ndarray): Original image
            processed (numpy.ndarray): Processed image
            title1 (str, optional): Title for the original image. Defaults to "Original".
            title2 (str, optional): Title for the processed image. Defaults to "Processed".

        Note:
            This function requires matplotlib to be installed.
       )pbdoc");
}
