#include "atom/algorithm/convolve.hpp"

#include <complex>
#include <iostream>
#include <vector>

int main() {
    // Example usage of 1D convolution
    {
        std::vector<double> input = {1, 2, 3, 4, 5};
        std::vector<double> kernel = {1, 0, -1};
        std::vector<double> result = atom::algorithm::convolve(input, kernel);

        std::cout << "1D Convolution result: ";
        for (double val : result) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Example usage of 1D deconvolution
    {
        std::vector<double> input = {1, 2, 3, 4, 5};
        std::vector<double> kernel = {1, 0, -1};
        std::vector<double> result = atom::algorithm::deconvolve(input, kernel);

        std::cout << "1D Deconvolution result: ";
        for (double val : result) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Example usage of 2D convolution
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel = {
            {1, 0, -1}, {1, 0, -1}, {1, 0, -1}};
        std::vector<std::vector<double>> result =
            atom::algorithm::convolve2D(input, kernel);

        std::cout << "2D Convolution result: " << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << val << " ";
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

        std::cout << "2D Deconvolution result: " << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of 2D Discrete Fourier Transform (DFT)
    {
        std::vector<std::vector<double>> input = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<std::complex<double>>> result =
            atom::algorithm::dfT2D(input);

        std::cout << "2D DFT result: " << std::endl;
        for (const auto& row : result) {
            for (const auto& val : row) {
                std::cout << val << " ";
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

        std::cout << "2D IDFT result: " << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of generating a Gaussian kernel
    {
        int size = 3;
        double sigma = 1.0;
        std::vector<std::vector<double>> kernel =
            atom::algorithm::generateGaussianKernel(size, sigma);

        std::cout << "Gaussian Kernel: " << std::endl;
        for (const auto& row : kernel) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }

    // Example usage of applying a Gaussian filter
    {
        std::vector<std::vector<double>> image = {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        std::vector<std::vector<double>> kernel =
            atom::algorithm::generateGaussianKernel(3, 1.0);
        std::vector<std::vector<double>> result =
            atom::algorithm::applyGaussianFilter(image, kernel);

        std::cout << "Gaussian Filter result: " << std::endl;
        for (const auto& row : result) {
            for (double val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }

    return 0;
}