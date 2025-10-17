#include "atom/algorithm/graphics/image_ops.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cstring>
#include <span>

namespace py = pybind11;

PYBIND11_MODULE(image_ops, m) {
    m.doc() = R"pbdoc(
        Image Processing Operations
        --------------------------

        This module provides fundamental image processing algorithms including:
        - Convolution with custom kernels
        - Gaussian blur and edge detection
        - Brightness and contrast adjustment
        - Histogram operations
        - Morphological operations

        Features:
        - High-performance implementations
        - NumPy array integration
        - Customizable kernels and parameters
        - Support for various data types
        - Memory-efficient operations

        Examples:
            >>> from atom.algorithm.image_ops import ImageOps
            >>> import numpy as np
            >>>
            >>> # Load image as numpy array
            >>> image = np.random.randint(0, 256, (100, 100), dtype=np.uint8)
            >>>
            >>> # Apply Gaussian blur
            >>> blurred = ImageOps.gaussian_blur(image, sigma=2.0)
            >>>
            >>> # Detect edges using Sobel operator
            >>> edges = ImageOps.sobel_edge_detection(image)
            >>>
            >>> # Adjust brightness and contrast
            >>> enhanced = ImageOps.adjust_brightness_contrast(image, brightness=20, contrast=1.2)
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // ImageOps class bindings
    py::class_<atom::algorithm::ImageOps>(m, "ImageOps", R"pbdoc(
        Basic image processing operations.

        This class provides fundamental image processing algorithms optimized
        for performance and ease of use with NumPy arrays.
    )pbdoc")
        .def_static(
            "convolve",
            [](py::array_t<uint8_t> image, py::array_t<float> kernel) {
                py::buffer_info img_buf = image.request();
                py::buffer_info kernel_buf = kernel.request();

                if (img_buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }
                if (kernel_buf.ndim != 2 ||
                    kernel_buf.shape[0] != kernel_buf.shape[1]) {
                    throw std::invalid_argument(
                        "Kernel must be square 2D array");
                }

                int width = static_cast<int>(img_buf.shape[1]);
                int height = static_cast<int>(img_buf.shape[0]);
                int kernel_size = static_cast<int>(kernel_buf.shape[0]);

                if (kernel_size % 2 == 0) {
                    throw std::invalid_argument("Kernel size must be odd");
                }

                uint8_t* img_ptr = static_cast<uint8_t*>(img_buf.ptr);
                float* kernel_ptr = static_cast<float*>(kernel_buf.ptr);

                std::span<const uint8_t> img_span(img_ptr, width * height);
                std::span<const float> kernel_span(kernel_ptr,
                                                   kernel_size * kernel_size);

                auto result = atom::algorithm::ImageOps::convolve(
                    img_span, width, height, kernel_span, kernel_size);

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"), py::arg("kernel"),
            R"pbdoc(
        Apply a convolution kernel to an image.

        Args:
            image: 2D NumPy array representing the image
            kernel: 2D square NumPy array representing the convolution kernel (must be odd size)

        Returns:
            Convolved image as 2D NumPy array

        Examples:
            >>> # Edge detection kernel
            >>> kernel = np.array([[-1, -1, -1], [-1, 8, -1], [-1, -1, -1]], dtype=np.float32)
            >>> edges = ImageOps.convolve(image, kernel)
        )pbdoc")

        .def_static(
            "gaussian_blur",
            [](py::array_t<uint8_t> image, double sigma, int kernel_size) {
                py::buffer_info buf = image.request();

                if (buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }

                int width = static_cast<int>(buf.shape[1]);
                int height = static_cast<int>(buf.shape[0]);
                uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                std::span<const uint8_t> img_span(ptr, width * height);
                auto result = atom::algorithm::ImageOps::gaussianBlur(
                    img_span, width, height, sigma);

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"), py::arg("sigma") = 1.0,
            py::arg("kernel_size") = 0,
            R"pbdoc(
        Apply Gaussian blur to an image.

        Args:
            image: 2D NumPy array representing the image
            sigma: Standard deviation for Gaussian kernel
            kernel_size: Size of the Gaussian kernel (0 for auto-calculation)

        Returns:
            Blurred image as 2D NumPy array

        Examples:
            >>> blurred = ImageOps.gaussian_blur(image, sigma=2.0)
        )pbdoc")

        .def_static(
            "sobel_edge_detection",
            [](py::array_t<uint8_t> image) {
                py::buffer_info buf = image.request();

                if (buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }

                int width = static_cast<int>(buf.shape[1]);
                int height = static_cast<int>(buf.shape[0]);
                uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                std::span<const uint8_t> img_span(ptr, width * height);
                auto result = atom::algorithm::ImageOps::sobelEdgeDetection(
                    img_span, width, height);

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"),
            R"pbdoc(
        Apply Sobel edge detection to an image.

        Args:
            image: 2D NumPy array representing the image

        Returns:
            Edge-detected image as 2D NumPy array

        Examples:
            >>> edges = ImageOps.sobel_edge_detection(image)
        )pbdoc")

        .def_static(
            "laplacian_edge_detection",
            [](py::array_t<uint8_t> image) {
                py::buffer_info buf = image.request();

                if (buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }

                int width = static_cast<int>(buf.shape[1]);
                int height = static_cast<int>(buf.shape[0]);
                uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                std::span<const uint8_t> img_span(ptr, width * height);
                auto result = atom::algorithm::ImageOps::laplacianEdgeDetection(
                    img_span, width, height);

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"),
            R"pbdoc(
        Apply Laplacian edge detection to an image.

        Args:
            image: 2D NumPy array representing the image

        Returns:
            Edge-detected image as 2D NumPy array
        )pbdoc")

        .def_static(
            "adjust_brightness_contrast",
            [](py::array_t<uint8_t> image, int brightness, double contrast) {
                py::buffer_info buf = image.request();

                if (buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }

                int width = static_cast<int>(buf.shape[1]);
                int height = static_cast<int>(buf.shape[0]);
                uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                std::span<const uint8_t> img_span(ptr, width * height);
                auto result =
                    atom::algorithm::ImageOps::adjustBrightnessContrast(
                        img_span, static_cast<float>(brightness),
                        static_cast<float>(contrast));

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"), py::arg("brightness") = 0,
            py::arg("contrast") = 1.0,
            R"pbdoc(
        Adjust brightness and contrast of an image.

        Args:
            image: 2D NumPy array representing the image
            brightness: Brightness adjustment (-255 to 255)
            contrast: Contrast multiplier (0.0 to 3.0, 1.0 = no change)

        Returns:
            Adjusted image as 2D NumPy array

        Examples:
            >>> # Increase brightness and contrast
            >>> enhanced = ImageOps.adjust_brightness_contrast(image, brightness=20, contrast=1.2)
        )pbdoc")

        .def_static(
            "histogram_equalization",
            [](py::array_t<uint8_t> image) {
                py::buffer_info buf = image.request();

                if (buf.ndim != 2) {
                    throw std::invalid_argument("Image must be 2D array");
                }

                int width = static_cast<int>(buf.shape[1]);
                int height = static_cast<int>(buf.shape[0]);
                uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                std::span<const uint8_t> img_span(ptr, width * height);
                auto result =
                    atom::algorithm::ImageOps::histogramEqualization(img_span);

                py::array_t<uint8_t> output =
                    py::array_t<uint8_t>({height, width});
                py::buffer_info out_buf = output.request();
                uint8_t* out_ptr = static_cast<uint8_t*>(out_buf.ptr);

                std::copy(result.begin(), result.end(), out_ptr);
                return output;
            },
            py::arg("image"),
            R"pbdoc(
        Apply histogram equalization to enhance image contrast.

        Args:
            image: 2D NumPy array representing the image

        Returns:
            Histogram-equalized image as 2D NumPy array

        Examples:
            >>> equalized = ImageOps.histogram_equalization(image)
        )pbdoc");

    // Utility functions for common image processing tasks
    m.def(
        "create_gaussian_kernel",
        [](int size, double sigma) {
            if (size % 2 == 0) {
                throw std::invalid_argument("Kernel size must be odd");
            }

            py::array_t<float> kernel = py::array_t<float>({size, size});
            py::buffer_info buf = kernel.request();
            float* ptr = static_cast<float*>(buf.ptr);

            double sum = 0.0;
            int center = size / 2;

            for (int i = 0; i < size; ++i) {
                for (int j = 0; j < size; ++j) {
                    int x = i - center;
                    int y = j - center;
                    double value =
                        std::exp(-(x * x + y * y) / (2.0 * sigma * sigma));
                    ptr[i * size + j] = static_cast<float>(value);
                    sum += value;
                }
            }

            // Normalize kernel
            for (int i = 0; i < size * size; ++i) {
                ptr[i] /= static_cast<float>(sum);
            }

            return kernel;
        },
        py::arg("size"), py::arg("sigma"),
        R"pbdoc(
    Create a Gaussian convolution kernel.

    Args:
        size: Size of the square kernel (must be odd)
        sigma: Standard deviation of the Gaussian

    Returns:
        2D NumPy array representing the normalized Gaussian kernel
    )pbdoc");

    m.def(
        "create_edge_detection_kernel",
        [](const std::string& type) {
            py::array_t<float> kernel;

            if (type == "sobel_x") {
                kernel = py::array_t<float>({3, 3});
                float data[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
                std::memcpy(kernel.mutable_data(), data, sizeof(data));
            } else if (type == "sobel_y") {
                kernel = py::array_t<float>({3, 3});
                float data[] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
                std::memcpy(kernel.mutable_data(), data, sizeof(data));
            } else if (type == "laplacian") {
                kernel = py::array_t<float>({3, 3});
                float data[] = {0, -1, 0, -1, 4, -1, 0, -1, 0};
                std::memcpy(kernel.mutable_data(), data, sizeof(data));
            } else if (type == "edge_enhance") {
                kernel = py::array_t<float>({3, 3});
                float data[] = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
                std::memcpy(kernel.mutable_data(), data, sizeof(data));
            } else {
                throw std::invalid_argument("Unknown kernel type: " + type);
            }

            return kernel;
        },
        py::arg("type"),
        R"pbdoc(
    Create predefined edge detection kernels.

    Args:
        type: Type of kernel ("sobel_x", "sobel_y", "laplacian", "edge_enhance")

    Returns:
        2D NumPy array representing the kernel

    Examples:
        >>> sobel_x = create_edge_detection_kernel("sobel_x")
        >>> edges = ImageOps.convolve(image, sobel_x)
    )pbdoc");

    // Enhanced NumPy integration functions
    m.def(
        "batch_process_images",
        [](py::list images, const std::string& operation, py::kwargs kwargs) {
            std::vector<py::array_t<uint8_t>> results;
            results.reserve(images.size());

            for (auto& img : images) {
                py::array_t<uint8_t> image = img.cast<py::array_t<uint8_t>>();
                py::array_t<uint8_t> result;

                // Convert py::array_t to std::span for the C++ functions
                py::buffer_info buf = image.request();
                std::span<const uint8_t> image_span(
                    static_cast<const uint8_t*>(buf.ptr), buf.size);

                // Extract width and height from the array shape (assuming 2D
                // image)
                int width = 0, height = 0;
                if (buf.ndim == 2) {
                    height = static_cast<int>(buf.shape[0]);
                    width = static_cast<int>(buf.shape[1]);
                } else {
                    throw std::invalid_argument("Expected 2D image array");
                }

                if (operation == "gaussian_blur") {
                    double sigma = kwargs.contains("sigma")
                                       ? kwargs["sigma"].cast<double>()
                                       : 1.0;
                    auto result_span = atom::algorithm::ImageOps::gaussianBlur(
                        image_span, width, height, static_cast<float>(sigma));
                    result = py::array_t<uint8_t>(result_span.size());
                    std::memcpy(result.mutable_data(), result_span.data(),
                                result_span.size());
                } else if (operation == "sobel_edge_detection") {
                    auto result_span =
                        atom::algorithm::ImageOps::sobelEdgeDetection(
                            image_span, width, height);
                    result = py::array_t<uint8_t>(result_span.size());
                    std::memcpy(result.mutable_data(), result_span.data(),
                                result_span.size());
                } else if (operation == "laplacian_edge_detection") {
                    auto result_span =
                        atom::algorithm::ImageOps::laplacianEdgeDetection(
                            image_span, width, height);
                    result = py::array_t<uint8_t>(result_span.size());
                    std::memcpy(result.mutable_data(), result_span.data(),
                                result_span.size());
                } else if (operation == "adjust_brightness_contrast") {
                    int brightness = kwargs.contains("brightness")
                                         ? kwargs["brightness"].cast<int>()
                                         : 0;
                    double contrast = kwargs.contains("contrast")
                                          ? kwargs["contrast"].cast<double>()
                                          : 1.0;
                    auto result_span =
                        atom::algorithm::ImageOps::adjustBrightnessContrast(
                            image_span, static_cast<float>(brightness),
                            static_cast<float>(contrast));
                    result = py::array_t<uint8_t>(result_span.size());
                    std::memcpy(result.mutable_data(), result_span.data(),
                                result_span.size());
                } else if (operation == "histogram_equalization") {
                    auto result_span =
                        atom::algorithm::ImageOps::histogramEqualization(
                            image_span);
                    result = py::array_t<uint8_t>(result_span.size());
                    std::memcpy(result.mutable_data(), result_span.data(),
                                result_span.size());
                } else {
                    throw std::invalid_argument("Unknown operation: " +
                                                operation);
                }

                results.push_back(result);
            }

            return results;
        },
        py::arg("images"), py::arg("operation"),
        R"pbdoc(
    Process multiple images with the same operation efficiently.

    Args:
        images: List of 2D NumPy arrays representing images
        operation: Operation name ("gaussian_blur", "sobel_edge_detection", etc.)
        **kwargs: Operation-specific parameters

    Returns:
        List of processed images as NumPy arrays

    Examples:
        >>> images = [img1, img2, img3]
        >>> blurred = batch_process_images(images, "gaussian_blur", sigma=2.0)
    )pbdoc");

    m.def(
        "compute_image_statistics",
        [](py::array_t<uint8_t> image) {
            py::buffer_info buf = image.request();

            if (buf.ndim != 2) {
                throw std::invalid_argument("Image must be 2D array");
            }

            uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);
            size_t size = buf.shape[0] * buf.shape[1];

            // Compute basic statistics
            uint8_t min_val = *std::min_element(ptr, ptr + size);
            uint8_t max_val = *std::max_element(ptr, ptr + size);

            double mean = 0.0;
            for (size_t i = 0; i < size; ++i) {
                mean += ptr[i];
            }
            mean /= size;

            double variance = 0.0;
            for (size_t i = 0; i < size; ++i) {
                double diff = ptr[i] - mean;
                variance += diff * diff;
            }
            variance /= size;
            double std_dev = std::sqrt(variance);

            // Compute histogram
            std::vector<uint32_t> histogram(256, 0);
            for (size_t i = 0; i < size; ++i) {
                histogram[ptr[i]]++;
            }

            py::dict stats;
            stats["min"] = min_val;
            stats["max"] = max_val;
            stats["mean"] = mean;
            stats["std_dev"] = std_dev;
            stats["variance"] = variance;
            stats["histogram"] = histogram;

            return stats;
        },
        py::arg("image"),
        R"pbdoc(
    Compute comprehensive statistics for an image.

    Args:
        image: 2D NumPy array representing the image

    Returns:
        Dictionary containing min, max, mean, std_dev, variance, and histogram

    Examples:
        >>> stats = compute_image_statistics(image)
        >>> print(f"Mean: {stats['mean']}, Std Dev: {stats['std_dev']}")
    )pbdoc");

    m.def(
        "create_test_image",
        [](int width, int height, const std::string& pattern, int seed) {
            py::array_t<uint8_t> image = py::array_t<uint8_t>({height, width});
            py::buffer_info buf = image.request();
            uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

            std::mt19937 gen(seed);

            if (pattern == "random") {
                std::uniform_int_distribution<> dis(0, 255);
                for (int i = 0; i < height * width; ++i) {
                    ptr[i] = static_cast<uint8_t>(dis(gen));
                }
            } else if (pattern == "gradient_x") {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        ptr[y * width + x] =
                            static_cast<uint8_t>((x * 255) / (width - 1));
                    }
                }
            } else if (pattern == "gradient_y") {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        ptr[y * width + x] =
                            static_cast<uint8_t>((y * 255) / (height - 1));
                    }
                }
            } else if (pattern == "checkerboard") {
                int block_size = std::max(1, std::min(width, height) / 16);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        bool is_white =
                            ((x / block_size) + (y / block_size)) % 2 == 0;
                        ptr[y * width + x] = is_white ? 255 : 0;
                    }
                }
            } else {
                throw std::invalid_argument("Unknown pattern: " + pattern);
            }

            return image;
        },
        py::arg("width"), py::arg("height"), py::arg("pattern") = "random",
        py::arg("seed") = 42,
        R"pbdoc(
    Create test images with various patterns for algorithm testing.

    Args:
        width: Image width
        height: Image height
        pattern: Pattern type ("random", "gradient_x", "gradient_y", "checkerboard")
        seed: Random seed for random pattern

    Returns:
        2D NumPy array representing the test image

    Examples:
        >>> test_img = create_test_image(256, 256, "checkerboard")
        >>> edges = ImageOps.sobel_edge_detection(test_img)
    )pbdoc");
}
