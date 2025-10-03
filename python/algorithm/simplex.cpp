#include "atom/algorithm/graphics/simplex.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(simplex, m) {
    m.doc() = R"pbdoc(
        Simplex Noise Generation
        -----------------------

        This module provides high-quality simplex noise generation for procedural content creation.
        Simplex noise is an improved version of Perlin noise with better visual quality and performance.

        Features:
        - 2D, 3D, and 4D simplex noise generation
        - Fractal noise with multiple octaves
        - Configurable parameters (persistence, lacunarity)
        - Optimized algorithms for high performance
        - Seamless tiling support
        - Gradient-based noise for smooth transitions

        Applications:
        - Procedural terrain generation
        - Texture synthesis
        - Cloud and weather simulation
        - Organic pattern generation
        - Game world generation
        - Visual effects and animation

        Examples:
            >>> from atom.algorithm.simplex import SimplexNoise
            >>> import numpy as np
            >>> 
            >>> # Create noise generator with seed
            >>> noise = SimplexNoise(12345)
            >>> 
            >>> # Generate 2D noise
            >>> value = noise.noise2d(0.5, 0.3)
            >>> 
            >>> # Generate fractal noise with multiple octaves
            >>> fractal_value = noise.fractal2d(0.5, 0.3, octaves=4, persistence=0.5)
            >>> 
            >>> # Generate noise map for terrain
            >>> width, height = 256, 256
            >>> noise_map = noise.generate_noise_map_2d(width, height, scale=0.1)
            >>> 
            >>> # Generate 3D noise for volumetric effects
            >>> volume_value = noise.noise3d(0.5, 0.3, 0.7)
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

    // SimplexNoise class bindings
    py::class_<atom::algorithm::SimplexNoise>(m, "SimplexNoise", R"pbdoc(
        High-quality simplex noise generator.
        
        Simplex noise is an improved version of Perlin noise that provides better
        visual quality with fewer directional artifacts and better performance
        in higher dimensions.
    )pbdoc")
        .def(py::init<uint32_t>(), py::arg("seed") = 0,
             R"pbdoc(
             Initialize simplex noise generator with optional seed.
             
             Args:
                 seed: Random seed for reproducible noise generation (default: 0)
             )pbdoc")
        
        .def("noise2d", [](const atom::algorithm::SimplexNoise& self, double x, double y) {
            return self.noise2D(x, y);
        }, py::arg("x"), py::arg("y"),
        R"pbdoc(
        Generate 2D simplex noise at the given coordinates.
        
        Args:
            x: X coordinate
            y: Y coordinate
            
        Returns:
            Noise value in range [-1, 1]
            
        Examples:
            >>> noise = SimplexNoise(42)
            >>> value = noise.noise2d(1.5, 2.3)
        )pbdoc")
        
        .def("noise3d", [](const atom::algorithm::SimplexNoise& self, double x, double y, double z) {
            return self.noise3D(x, y, z);
        }, py::arg("x"), py::arg("y"), py::arg("z"),
        R"pbdoc(
        Generate 3D simplex noise at the given coordinates.
        
        Args:
            x: X coordinate
            y: Y coordinate
            z: Z coordinate
            
        Returns:
            Noise value in range [-1, 1]
        )pbdoc")
        
        .def("noise4d", [](const atom::algorithm::SimplexNoise& self, double x, double y, double z, double w) {
            return self.noise4D(x, y, z, w);
        }, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"),
        R"pbdoc(
        Generate 4D simplex noise at the given coordinates.
        
        Args:
            x: X coordinate
            y: Y coordinate
            z: Z coordinate
            w: W coordinate
            
        Returns:
            Noise value in range [-1, 1]
        )pbdoc")
        
        .def("fractal2d", [](const atom::algorithm::SimplexNoise& self, double x, double y, 
                            int octaves, double persistence, double lacunarity) {
            return self.fractal2D(x, y, octaves, persistence, lacunarity);
        }, py::arg("x"), py::arg("y"), py::arg("octaves"), py::arg("persistence"), 
           py::arg("lacunarity") = 2.0,
        R"pbdoc(
        Generate fractal 2D noise using multiple octaves.
        
        Fractal noise combines multiple octaves of noise at different frequencies
        and amplitudes to create more complex, natural-looking patterns.
        
        Args:
            x: X coordinate
            y: Y coordinate
            octaves: Number of noise octaves to combine
            persistence: Amplitude multiplier for each octave (typically 0.5)
            lacunarity: Frequency multiplier for each octave (typically 2.0)
            
        Returns:
            Fractal noise value
            
        Examples:
            >>> noise = SimplexNoise(123)
            >>> # Generate terrain-like noise
            >>> terrain = noise.fractal2d(x, y, octaves=6, persistence=0.5)
        )pbdoc")
        
        .def("fractal3d", [](const atom::algorithm::SimplexNoise& self, double x, double y, double z,
                            int octaves, double persistence, double lacunarity) {
            return self.fractal3D(x, y, z, octaves, persistence, lacunarity);
        }, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("octaves"), 
           py::arg("persistence"), py::arg("lacunarity") = 2.0,
        R"pbdoc(
        Generate fractal 3D noise using multiple octaves.
        
        Args:
            x: X coordinate
            y: Y coordinate
            z: Z coordinate
            octaves: Number of noise octaves to combine
            persistence: Amplitude multiplier for each octave
            lacunarity: Frequency multiplier for each octave
            
        Returns:
            Fractal noise value
        )pbdoc")
        
        .def("generate_noise_map_2d", [](const atom::algorithm::SimplexNoise& self,
                                        int width, int height, double scale,
                                        int octaves, double persistence, double lacunarity) {
            // Generate 2D noise map
            py::array_t<double> result = py::array_t<double>({height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);
            
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    double sample_x = x * scale;
                    double sample_y = y * scale;
                    
                    double noise_value;
                    if (octaves > 1) {
                        noise_value = self.fractal2D(sample_x, sample_y, octaves, persistence, lacunarity);
                    } else {
                        noise_value = self.noise2D(sample_x, sample_y);
                    }
                    
                    ptr[y * width + x] = noise_value;
                }
            }
            
            return result;
        }, py::arg("width"), py::arg("height"), py::arg("scale"), 
           py::arg("octaves") = 1, py::arg("persistence") = 0.5, py::arg("lacunarity") = 2.0,
        R"pbdoc(
        Generate a 2D noise map as a NumPy array.
        
        Args:
            width: Width of the noise map
            height: Height of the noise map
            scale: Scaling factor for noise coordinates (smaller = more zoomed out)
            octaves: Number of fractal octaves (default: 1)
            persistence: Amplitude multiplier for each octave (default: 0.5)
            lacunarity: Frequency multiplier for each octave (default: 2.0)
            
        Returns:
            2D NumPy array of noise values
            
        Examples:
            >>> noise = SimplexNoise(42)
            >>> # Generate terrain heightmap
            >>> heightmap = noise.generate_noise_map_2d(512, 512, 0.01, octaves=6)
            >>> 
            >>> # Generate texture pattern
            >>> texture = noise.generate_noise_map_2d(256, 256, 0.05, octaves=3)
        )pbdoc")
        
        .def("generate_noise_map_3d", [](const atom::algorithm::SimplexNoise& self,
                                        int width, int height, int depth, double scale,
                                        int octaves, double persistence, double lacunarity) {
            // Generate 3D noise map
            py::array_t<double> result = py::array_t<double>({depth, height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);
            
            for (int z = 0; z < depth; ++z) {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        double sample_x = x * scale;
                        double sample_y = y * scale;
                        double sample_z = z * scale;
                        
                        double noise_value;
                        if (octaves > 1) {
                            noise_value = self.fractal3D(sample_x, sample_y, sample_z, octaves, persistence, lacunarity);
                        } else {
                            noise_value = self.noise3D(sample_x, sample_y, sample_z);
                        }
                        
                        ptr[z * height * width + y * width + x] = noise_value;
                    }
                }
            }
            
            return result;
        }, py::arg("width"), py::arg("height"), py::arg("depth"), py::arg("scale"),
           py::arg("octaves") = 1, py::arg("persistence") = 0.5, py::arg("lacunarity") = 2.0,
        R"pbdoc(
        Generate a 3D noise map as a NumPy array.
        
        Args:
            width: Width of the noise map
            height: Height of the noise map
            depth: Depth of the noise map
            scale: Scaling factor for noise coordinates
            octaves: Number of fractal octaves (default: 1)
            persistence: Amplitude multiplier for each octave (default: 0.5)
            lacunarity: Frequency multiplier for each octave (default: 2.0)
            
        Returns:
            3D NumPy array of noise values
            
        Examples:
            >>> noise = SimplexNoise(123)
            >>> # Generate 3D cloud density
            >>> clouds = noise.generate_noise_map_3d(64, 64, 64, 0.1, octaves=4)
        )pbdoc");

    // Utility functions for common noise patterns
    m.def("generate_terrain_heightmap", [](int width, int height, uint32_t seed, 
                                          double scale, int octaves, double persistence) {
        atom::algorithm::SimplexNoise noise(seed);
        return noise.generate_noise_map_2d(width, height, scale, octaves, persistence, 2.0);
    }, py::arg("width"), py::arg("height"), py::arg("seed") = 0, py::arg("scale") = 0.01,
       py::arg("octaves") = 6, py::arg("persistence") = 0.5,
    R"pbdoc(
    Generate a terrain heightmap using optimized parameters.
    
    This is a convenience function that generates terrain-like noise
    with commonly used parameters for landscape generation.
    
    Args:
        width: Width of the heightmap
        height: Height of the heightmap
        seed: Random seed for reproducible generation
        scale: Terrain scale (smaller = larger features)
        octaves: Number of detail levels
        persistence: Detail falloff rate
        
    Returns:
        2D NumPy array representing terrain heights
    )pbdoc");

    m.def("generate_cloud_texture", [](int width, int height, uint32_t seed,
                                      double scale, int octaves) {
        atom::algorithm::SimplexNoise noise(seed);
        return noise.generate_noise_map_2d(width, height, scale, octaves, 0.6, 2.0);
    }, py::arg("width"), py::arg("height"), py::arg("seed") = 0, py::arg("scale") = 0.05,
       py::arg("octaves") = 4,
    R"pbdoc(
    Generate a cloud-like texture pattern.
    
    Args:
        width: Width of the texture
        height: Height of the texture
        seed: Random seed
        scale: Cloud scale
        octaves: Number of detail levels
        
    Returns:
        2D NumPy array representing cloud density
    )pbdoc");
}
