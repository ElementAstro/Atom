/*
 * convolve.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Umbrella header for signal processing convolution operations.
Includes all sub-components for backward compatibility.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_CONVOLVE_HPP
#define ATOM_ALGORITHM_SIGNAL_CONVOLVE_HPP

// Common types, concepts, and error definitions
#include "convolve_common.hpp"

// 1D convolution operations
#include "convolution_1d.hpp"

// 2D convolution and deconvolution operations
#include "convolution_2d.hpp"

// Discrete Fourier Transform operations
#include "dft.hpp"

// Gaussian kernel generation and filtering
#include "gaussian_filter.hpp"

// Convolution-based image filters (Sobel, Laplacian, etc.)
#include "convolution_filters.hpp"

// Frequency domain convolution
#include "frequency_domain.hpp"

#endif  // ATOM_ALGORITHM_SIGNAL_CONVOLVE_HPP
