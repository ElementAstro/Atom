# Signal Processing Algorithms

This directory contains algorithms for digital signal processing and analysis.

## Contents

- **`convolve.hpp/cpp`** - Convolution operations for 1D and 2D signals with multiple optimization strategies

## Features

### Convolution Operations

- **1D and 2D Convolution**: Support for both one-dimensional and two-dimensional signal processing
- **Multiple Algorithms**: Direct convolution, FFT-based convolution, and separable convolution
- **Padding Modes**: Zero padding, reflection, and periodic boundary conditions
- **SIMD Optimizations**: Vectorized operations for improved performance
- **Parallel Processing**: Multi-threaded convolution for large signals
- **OpenCL Support**: GPU acceleration when available

### Boundary Handling

- **Zero Padding**: Pad with zeros outside signal boundaries
- **Reflection**: Mirror signal values at boundaries
- **Periodic**: Treat signal as periodic/circular
- **Constant**: Extend with constant values

### Performance Optimizations

- **Algorithm Selection**: Automatically chooses optimal algorithm based on signal and kernel sizes
- **Memory Layout**: Cache-friendly memory access patterns
- **SIMD Instructions**: AVX/SSE optimizations for bulk operations
- **GPU Acceleration**: OpenCL kernels for parallel processing

## Use Cases

- **Image Processing**: Filtering, edge detection, blurring, sharpening
- **Audio Processing**: Digital filters, echo effects, noise reduction
- **Computer Vision**: Feature detection, template matching
- **Scientific Computing**: Signal analysis, data smoothing
- **Machine Learning**: Convolutional neural network layers

## Algorithm Types

### Direct Convolution

- Best for small kernels
- O(N\*M) complexity where N is signal size, M is kernel size
- Cache-friendly for small to medium datasets

### FFT-Based Convolution

- Efficient for large kernels
- O(N log N) complexity using Fast Fourier Transform
- Automatically selected for large kernel sizes

### Separable Convolution

- Optimized for separable 2D kernels
- Reduces 2D convolution to two 1D operations
- Significant performance improvement for applicable kernels

## Usage Examples

```cpp
#include "atom/algorithm/signal/convolve.hpp"

// 1D convolution
std::vector<double> signal = {1.0, 2.0, 3.0, 4.0, 5.0};
std::vector<double> kernel = {0.25, 0.5, 0.25};

atom::algorithm::Convolution1D<double> conv1d;
auto result = conv1d.convolve(signal, kernel);

// 2D convolution with custom padding
std::vector<std::vector<double>> image = /* ... */;
std::vector<std::vector<double>> filter = /* ... */;

atom::algorithm::Convolution2D<double> conv2d;
auto filtered = conv2d.convolve(image, filter,
                               atom::algorithm::PaddingMode::REFLECTION);
```

## Performance Notes

- Algorithm automatically selects optimal implementation based on input sizes
- SIMD optimizations provide 2-4x speedup on compatible hardware
- OpenCL acceleration can provide 10-100x speedup for large signals
- Memory usage is optimized to minimize cache misses

## Dependencies

- Core algorithm components
- Standard C++ library (C++20)
- Optional: OpenCL for GPU acceleration
- Optional: FFTW for FFT-based convolution
