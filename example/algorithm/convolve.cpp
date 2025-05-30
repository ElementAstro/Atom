#include "atom/algorithm/convolve.hpp"

#include <chrono>
#include <complex>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== 1D Convolution and Deconvolution ===" << std::endl;
    // Example usage of 1D convolution using Convolution1D class
    {
        std::vector<double> input = {1, 2, 3, 4, 5};
        std::vector<double> kernel = {1, 0, -1};
        std::vector<double> result =
            atom::algorithm::Convolution1D<double>::convolve(input, kernel);

        std::cout << "Input signal: ";
        for (double val : input) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        std::cout << "Kernel: ";
        for (double val : kernel) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        std::cout << "1D Convolution result: ";
        for (double val : result) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Example usage of 1D deconvolution using Convolution1D class
    {
        std::vector<double> input = {1, 2, 3, 4, 5};
        std::vector<double> kernel = {1, 0, -1};
        std::vector<double> result =
            atom::algorithm::Convolution1D<double>::deconvolve(input, kernel);

        std::cout << "\n1D Deconvolution result: ";
        for (double val : result) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "\n=== 2D Convolution and Deconvolution ===" << std::endl;
    // Example usage of 2D convolution
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {
            {1, 0, -1}, {1, 0, -1}, {1, 0, -1}};

        std::cout << "Input matrix:" << std::endl;
        for (const auto& row : input) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Kernel matrix:" << std::endl;
        for (const auto& row : kernel) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        std::vector<std::vector<double>> result =
            atom::algorithm::convolve2D(input, kernel);

        std::cout << "2D Convolution result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of 2D deconvolution
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {
            {1, 0, -1}, {1, 0, -1}, {1, 0, -1}};
        std::vector<std::vector<double>> result =
            atom::algorithm::deconvolve2D(input, kernel);

        std::cout << "\n2D Deconvolution result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n=== Fourier Transform Operations ===" << std::endl;
    // Example usage of 2D Discrete Fourier Transform (DFT)
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<std::complex<double>>> result =
            atom::algorithm::dfT2D(input);

        std::cout << "2D DFT result (first few elements):" << std::endl;
        for (size_t i = 0; i < std::min(result.size(), size_t(3)); ++i) {
            for (size_t j = 0; j < std::min(result[i].size(), size_t(3)); ++j) {
                std::cout << std::fixed << std::setprecision(1)
                          << result[i][j].real() << "+" << result[i][j].imag()
                          << "i  ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of 2D Inverse Discrete Fourier Transform (IDFT)
    {
        std::vector<std::vector<std::complex<double>>> input = {
            {{1, 0}, {2, 0}, {3, 0}},
            {{4, 0}, {5, 0}, {6, 0}},
            {{7, 0}, {8, 0}, {9, 0}}};
        std::vector<std::vector<double>> result =
            atom::algorithm::idfT2D(input);

        std::cout << "\n2D IDFT result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n=== Gaussian Filtering ===" << std::endl;
    // Example usage of generating a Gaussian kernel
    {
        int size = 5;  // Using a larger kernel for better visualization
        double sigma = 1.0;
        std::vector<std::vector<double>> kernel =
            atom::algorithm::generateGaussianKernel(size, sigma);

        std::cout << "Gaussian Kernel (5x5, sigma=1.0):" << std::endl;
        for (const auto& row : kernel) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(3) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of applying a Gaussian filter
    {
        // Create a simple test image with a central point
        std::vector<std::vector<double>> image(7, std::vector<double>(7, 0.0));
        image[3][3] = 10.0;  // Bright spot in the center

        std::cout << "\nOriginal image:" << std::endl;
        for (const auto& row : image) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }

        std::vector<std::vector<double>> kernel =
            atom::algorithm::generateGaussianKernel(3, 1.0);
        std::vector<std::vector<double>> result =
            atom::algorithm::applyGaussianFilter(image, kernel);

        std::cout << "\nGaussian Filter result (blurred point):" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(2) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n=== Convolution Filters ===" << std::endl;
    // Example usage of ConvolutionFilters class
    {
        std::vector<std::vector<double>> image = {
            {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

        std::cout << "Original image:" << std::endl;
        for (const auto& row : image) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(0) << val << " ";
            }
            std::cout << std::endl;
        }

        // Apply Sobel filter
        std::vector<std::vector<double>> sobelResult =
            atom::algorithm::ConvolutionFilters<double>::applySobel(image);

        std::cout << "\nSobel filter result:" << std::endl;
        for (const auto& row : sobelResult) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }

        // Apply Laplacian filter
        std::vector<std::vector<double>> laplacianResult =
            atom::algorithm::ConvolutionFilters<double>::applyLaplacian(image);

        std::cout << "\nLaplacian filter result:" << std::endl;
        for (const auto& row : laplacianResult) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

#if ATOM_USE_OPENCL
    std::cout << "\n=== OpenCL Accelerated Operations ===" << std::endl;
    // Example usage of OpenCL-accelerated 2D convolution
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {
            {1, 0, -1}, {1, 0, -1}, {1, 0, -1}};

        std::vector<std::vector<double>> result =
            atom::algorithm::convolve2DOpenCL(input, kernel);

        std::cout << "OpenCL 2D Convolution result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of OpenCL-accelerated 2D deconvolution
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {
            {1, 0, -1}, {1, 0, -1}, {1, 0, -1}};

        std::vector<std::vector<double>> result =
            atom::algorithm::deconvolve2DOpenCL(input, kernel);

        std::cout << "\nOpenCL 2D Deconvolution result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }
#endif

    // Example of parallel processing with custom thread count
    std::cout << "\n=== Custom Thread Count Example ===" << std::endl;
    {
        std::vector<std::vector<double>> input(10,
                                               std::vector<double>(10, 1.0));
        std::vector<std::vector<double>> kernel(
            3, std::vector<double>(3, 1.0 / 9.0));  // Box blur kernel

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<double>> result1 =
            atom::algorithm::convolve2D(input, kernel, 1);  // Single thread
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> singleThreadTime =
            end - start;

        start = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<double>> result2 =
            atom::algorithm::convolve2D(input, kernel, 4);  // 4 threads
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> multiThreadTime = end - start;

        std::cout << "Single thread execution time: "
                  << singleThreadTime.count() << " ms" << std::endl;
        std::cout << "Multi-thread execution time: " << multiThreadTime.count()
                  << " ms" << std::endl;

        // Check if results are the same
        bool resultsMatch = true;
        for (size_t i = 0; i < result1.size() && resultsMatch; ++i) {
            for (size_t j = 0; j < result1[i].size() && resultsMatch; ++j) {
                if (std::abs(result1[i][j] - result2[i][j]) > 1e-10) {
                    resultsMatch = false;
                }
            }
        }
        std::cout << "Results match: " << std::boolalpha << resultsMatch
                  << std::endl;
    }

    std::cout << "\n=== Frequency Domain Convolution ===" << std::endl;
    // Example usage of FrequencyDomainConvolution class
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {{1, 0, -1}};

        atom::algorithm::FrequencyDomainConvolution<double> freqConv(
            input.size(), input[0].size(), kernel.size(), kernel[0].size());

        std::vector<std::vector<double>> result =
            freqConv.convolve(input, kernel);

        std::cout << "Frequency domain convolution result:" << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << std::fixed << std::setprecision(1) << val << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n=== Padding Operations ===" << std::endl;
    // Example usage of padding operations
    {
        std::vector<std::vector<double>> input = {{1, 2}, {3, 4}};

        std::cout << "Original matrix:" << std::endl;
        for (const auto& row : input) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        std::vector<std::vector<double>> padded = atom::algorithm::pad2D(
            input, 1, 1, 1, 1, atom::algorithm::PaddingMode::SAME);

        std::cout << "Padded matrix (1x1 padding):" << std::endl;
        for (const auto& row : padded) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }

        // Get output dimensions
        auto [outputHeight, outputWidth] =
            atom::algorithm::getConvolutionOutputDimensions(
                input.size(), input[0].size(), 3, 3);
        std::cout << "Output dimensions for 3x3 kernel: " << outputHeight << "x"
                  << outputWidth << std::endl;
    }

    return 0;
}