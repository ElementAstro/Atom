# Graphics and Image Processing Algorithms

This directory contains algorithms for graphics processing, image manipulation, and procedural generation.

## Contents

### Flood Fill

- **`grid_concepts.hpp`** - Grid concepts (`Grid`, `SIMDCompatibleGrid`, `ContiguousGrid`, `SpanCompatibleGrid`) and `Connectivity` enum
- **`flood.hpp`** - `FloodFill` class declaration (BFS, DFS, parallel, SIMD, block-optimized)
- **`flood_impl.hpp`** - Template implementations for `FloodFill` (included automatically by `flood.hpp`)
- **`flood.cpp`** - Non-template implementations (SIMD row/block processing, specializations)

### Image Processing

- **`convolution.hpp`** - `Convolution` class: `convolve`, `gaussianBlur`
- **`edge_detection.hpp`** - `EdgeDetection` class: `sobelEdgeDetection`, `laplacianEdgeDetection`
- **`image_adjust.hpp`** - `ImageAdjust` class: `adjustBrightnessContrast`, `threshold`, `invert`
- **`histogram.hpp`** - `Histogram` class: `computeHistogram`, `histogramEqualization`
- **`image_ops.hpp`** - `ImageOps` backward-compatible facade (inherits all 4 image classes above)

### Noise Generation

- **`noise_base.hpp`** - `NoiseBase` base class (permutation tables, fade, lerp, gradient)
- **`perlin.hpp`** - `PerlinNoise` class (CPU noise + OpenCL dispatch)
- **`perlin_opencl.hpp`** - `PerlinNoiseOpenCL` helper (OpenCL-only, used by `PerlinNoise`)
- **`simplex.hpp`** - `SimplexNoise` class (2D/3D simplex + fractal noise)

## Features

### Flood Fill Algorithms

- **Multiple Connectivity**: 4-way and 8-way connectivity options
- **BFS and DFS**: Both breadth-first and depth-first search implementations
- **SIMD Optimizations**: Vectorized operations for bulk pixel processing
- **Parallel Processing**: Multi-threaded flood fill for large images
- **Generic Grid Support**: Works with any 2D grid-like data structure
- **Boundary Checking**: Safe operations with automatic bounds validation

### Perlin Noise

- **Classic Perlin Noise**: Ken Perlin's improved noise algorithm
- **Octave Noise**: Multiple octaves for fractal-like patterns
- **3D Noise**: Support for 3D noise generation
- **Configurable Parameters**: Frequency, amplitude, persistence control
- **Seamless Tiling**: Generate tileable noise patterns
- **OpenCL Acceleration**: GPU-accelerated noise generation when available

## Use Cases

### Flood Fill

- **Image Editing**: Paint bucket tool implementation
- **Game Development**: Area selection, territory marking
- **Computer Vision**: Connected component analysis
- **Geographic Information Systems**: Region identification
- **Medical Imaging**: Organ segmentation and analysis

### Perlin Noise

- **Procedural Terrain**: Height maps for 3D landscapes
- **Texture Generation**: Organic-looking surface patterns
- **Game Development**: Procedural world generation
- **Visual Effects**: Cloud simulation, water surfaces
- **Animation**: Natural-looking motion patterns

## Algorithm Details

### Flood Fill

- **BFS Implementation**: Uses queue for breadth-first traversal
- **DFS Implementation**: Uses stack for depth-first traversal
- **SIMD Processing**: Vectorized color comparison and replacement
- **Memory Optimization**: Efficient visited tracking for large grids
- **Connectivity Patterns**: Configurable neighbor patterns

### Perlin Noise

- **Gradient Vectors**: Pre-computed gradient table for consistency
- **Interpolation**: Smooth interpolation between grid points
- **Octave Layering**: Combines multiple noise frequencies
- **Persistence Control**: Controls amplitude decrease between octaves
- **Lacunarity**: Controls frequency increase between octaves

## Performance Features

- **SIMD Acceleration**: AVX2 optimizations for bulk operations
- **Parallel Processing**: Multi-threaded algorithms for large datasets
- **Memory Efficiency**: Optimized memory access patterns
- **GPU Support**: OpenCL kernels for parallel processing
- **Cache Optimization**: Data structures designed for cache efficiency

## Usage Examples

```cpp
#include "atom/algorithm/graphics/flood.hpp"
#include "atom/algorithm/graphics/perlin.hpp"

// Flood fill on a 2D grid
std::vector<std::vector<int>> grid = /* initialize grid */;
auto filled_count = atom::algorithm::floodFillBFS(
    grid,
    10, 15,        // start position
    old_color,     // target color
    new_color,     // replacement color
    Connectivity::Eight  // 8-way connectivity
);

// Perlin noise generation
atom::algorithm::PerlinNoise noise(12345);  // seed
auto noise_map = noise.generateNoiseMap(
    256, 256,      // width, height
    0.1,           // scale
    4,             // octaves
    0.5,           // persistence
    2.0            // lacunarity
);

// 3D Perlin noise
double noise_value = noise.octaveNoise(x, y, z, 4, 0.5);
```

## Grid Concepts

The flood fill algorithms work with any type that satisfies the Grid concept:

```cpp
template<typename T>
concept Grid = requires(T t, std::size_t i, std::size_t j) {
    { t[i] } -> std::ranges::random_access_range;
    { t[i][j] } -> std::convertible_to<typename T::value_type::value_type>;
    { t.empty() } -> std::same_as<bool>;
};
```

## Performance Considerations

- Flood fill algorithms are optimized for cache locality
- SIMD operations provide significant speedup for large images
- Parallel processing scales well with core count
- Memory usage is optimized to handle large grids efficiently
- OpenCL acceleration can provide 10-100x speedup for suitable workloads

## Dependencies

- Core algorithm components
- Standard C++ library (C++20)
- Optional: OpenCL for GPU acceleration
- Optional: SIMD intrinsics for vectorization
