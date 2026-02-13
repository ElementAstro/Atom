#include "atom/algorithm/graphics/perlin.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(perlin, m) {
    m.doc() = R"pbdoc(
        Perlin Noise Generator Module
        ----------------------------

        This module provides a high-performance implementation of Perlin noise,
        with support for multiple octaves, persistence, and GPU acceleration.

        Features:
        - 1D, 2D, and 3D noise generation
        - Octave noise for more natural patterns
        - Noise map generation for terrain or texture creation
        - OpenCL acceleration when available

        Example:
            >>> from atom.algorithm.perlin import PerlinNoise
            >>>
            >>> # Create a noise generator with a specific seed
            >>> noise = PerlinNoise(seed=42)
            >>>
            >>> # Generate a single noise value
            >>> value = noise.noise(1.0, 2.0, 0.5)
            >>>
            >>> # Generate a 2D noise map (e.g., for terrain)
            >>> noise_map = noise.generate_noise_map(256, 256, scale=25.0, octaves=4, persistence=0.5)
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register PerlinNoise class
    py::class_<atom::algorithm::PerlinNoise>(m, "PerlinNoise",
                                             R"pbdoc(
        Perlin noise generator class.

        This class implements the improved Perlin noise algorithm for
        generating coherent noise in 1D, 2D, or 3D space. It can be used
        for procedural generation of terrain, textures, animations, etc.

        Constructor Args:
            seed: Optional random seed for noise generation (default: system random)

        Examples:
            >>> noise = PerlinNoise(seed=42)
            >>> value = noise.noise(x=1.0, y=2.0, z=3.0)
            >>> print(value)  # Value will be between 0.0 and 1.0
        )pbdoc")
        .def(py::init<unsigned int>(),
             py::arg("seed") = std::default_random_engine::default_seed,
             "Initializes the Perlin noise generator with the specified seed.")
        .def(
            "noise",
            [](const atom::algorithm::PerlinNoise& self, double x, double y,
               double z) { return self.noise(x, y, z); },
            py::arg("x"), py::arg("y"), py::arg("z"),
            R"pbdoc(
             Generate a 3D Perlin noise value.

             Args:
                 x: X-coordinate in noise space
                 y: Y-coordinate in noise space
                 z: Z-coordinate in noise space

             Returns:
                 Noise value in range [0.0, 1.0]

             Example:
                 >>> noise = PerlinNoise(seed=42)
                 >>> value = noise.noise(0.5, 1.2, 0.8)
             )pbdoc")
        .def(
            "noise_2d",
            [](const atom::algorithm::PerlinNoise& self, double x, double y) {
                // 2D noise is just 3D noise with z=0
                return self.noise(x, y, 0.0);
            },
            py::arg("x"), py::arg("y"),
            R"pbdoc(
             Generate a 2D Perlin noise value.

             Args:
                 x: X-coordinate in noise space
                 y: Y-coordinate in noise space

             Returns:
                 Noise value in range [0.0, 1.0]

             Example:
                 >>> noise = PerlinNoise(seed=42)
                 >>> value = noise.noise_2d(0.5, 1.2)
             )pbdoc")
        .def(
            "noise_1d",
            [](const atom::algorithm::PerlinNoise& self, double x) {
                // 1D noise is just 3D noise with y=0 and z=0
                return self.noise(x, 0.0, 0.0);
            },
            py::arg("x"),
            R"pbdoc(
             Generate a 1D Perlin noise value.

             Args:
                 x: X-coordinate in noise space

             Returns:
                 Noise value in range [0.0, 1.0]

             Example:
                 >>> noise = PerlinNoise(seed=42)
                 >>> value = noise.noise_1d(0.5)
             )pbdoc")
        .def(
            "octave_noise",
            [](const atom::algorithm::PerlinNoise& self, double x, double y,
               double z, int octaves, double persistence) {
                return self.octaveNoise(x, y, z, octaves, persistence);
            },
            py::arg("x"), py::arg("y"), py::arg("z"), py::arg("octaves"),
            py::arg("persistence"),
            R"pbdoc(
             Generate fractal noise by summing multiple octaves of Perlin noise.

             Args:
                 x: X-coordinate in noise space
                 y: Y-coordinate in noise space
                 z: Z-coordinate in noise space
                 octaves: Number of noise layers to sum
                 persistence: Amplitude multiplier for each octave (0.0-1.0)

             Returns:
                 Octave noise value in range [0.0, 1.0]

             Example:
                 >>> noise = PerlinNoise(seed=42)
                 >>> value = noise.octave_noise(0.5, 1.2, 0.8, octaves=4, persistence=0.5)
             )pbdoc")
        .def(
            "octave_noise_2d",
            [](const atom::algorithm::PerlinNoise& self, double x, double y,
               int octaves, double persistence) {
                return self.octaveNoise(x, y, 0.0, octaves, persistence);
            },
            py::arg("x"), py::arg("y"), py::arg("octaves"),
            py::arg("persistence"),
            R"pbdoc(
             Generate 2D fractal noise by summing multiple octaves of Perlin noise.

             Args:
                 x: X-coordinate in noise space
                 y: Y-coordinate in noise space
                 octaves: Number of noise layers to sum
                 persistence: Amplitude multiplier for each octave (0.0-1.0)

             Returns:
                 Octave noise value in range [0.0, 1.0]
             )pbdoc")
        .def(
            "generate_noise_map",
            [](const atom::algorithm::PerlinNoise& self, int width, int height,
               double scale, int octaves, double persistence, double lacunarity,
               int seed) {
                auto noise_map =
                    self.generateNoiseMap(width, height, scale, octaves,
                                          persistence, lacunarity, seed);

                // Convert to numpy array for Python
                py::array_t<double> result({height, width});
                auto buffer = result.request();
                double* ptr = static_cast<double*>(buffer.ptr);

                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        ptr[y * width + x] = noise_map[y][x];
                    }
                }

                return result;
            },
            py::arg("width"), py::arg("height"), py::arg("scale"),
            py::arg("octaves"), py::arg("persistence"),
            py::arg("lacunarity") = 2.0,
            py::arg("seed") = std::default_random_engine::default_seed,
            R"pbdoc(
             Generate a 2D noise map.

             This is useful for terrain generation, textures, or other 2D applications.

             Args:
                 width: Width of the noise map
                 height: Height of the noise map
                 scale: Zoom level (smaller values = more zoomed out patterns)
                 octaves: Number of summed noise layers
                 persistence: Amplitude reduction per octave (0.0-1.0)
                 lacunarity: Frequency multiplier per octave (default: 2.0)
                 seed: Random seed for noise map generation (default: uses object's seed)

             Returns:
                 2D numpy array of noise values in range [0.0, 1.0]

             Example:
                 >>> noise = PerlinNoise(seed=42)
                 >>> terrain = noise.generate_noise_map(
                 ...     width=256, height=256,
                 ...     scale=50.0, octaves=4, persistence=0.5
                 ... )
                 >>>
                 >>> # You can visualize it with matplotlib:
                 >>> import matplotlib.pyplot as plt
                 >>> plt.imshow(terrain, cmap='terrain')
                 >>> plt.colorbar()
                 >>> plt.show()
             )pbdoc");

    // Utility functions
    m.def(
        "create_fractal_noise",
        [](int width, int height, double scale, int octaves, double persistence,
           double lacunarity, int seed) {
            atom::algorithm::PerlinNoise noise(seed);
            return noise.generateNoiseMap(width, height, scale, octaves,
                                          persistence, lacunarity, seed);
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"), py::arg("lacunarity") = 2.0,
        py::arg("seed") = std::default_random_engine::default_seed,
        R"pbdoc(
          Convenience function to create a fractal noise map in one call.

          Args:
              width: Width of the noise map
              height: Height of the noise map
              scale: Zoom level (smaller values = more zoomed out patterns)
              octaves: Number of summed noise layers
              persistence: Amplitude reduction per octave (0.0-1.0)
              lacunarity: Frequency multiplier per octave (default: 2.0)
              seed: Random seed for noise map generation

          Returns:
              2D numpy array of noise values in range [0.0, 1.0]
          )pbdoc");

    // Additional utility functions for enhanced functionality
    m.def(
        "generate_3d_noise_volume",
        [](int width, int height, int depth, double scale, int octaves,
           double persistence, double lacunarity, int seed) {
            atom::algorithm::PerlinNoise noise(seed);

            // Generate 3D noise volume
            py::array_t<double> result({depth, height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);

            for (int z = 0; z < depth; ++z) {
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        double sample_x = x / scale;
                        double sample_y = y / scale;
                        double sample_z = z / scale;

                        double noise_value = noise.octaveNoise(
                            sample_x, sample_y, sample_z, octaves, persistence);
                        ptr[z * height * width + y * width + x] = noise_value;
                    }
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("depth"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"), py::arg("lacunarity") = 2.0,
        py::arg("seed") = std::default_random_engine::default_seed,
        R"pbdoc(
    Generate a 3D noise volume for volumetric effects.

    Args:
        width: Width of the volume
        height: Height of the volume
        depth: Depth of the volume
        scale: Zoom level for noise sampling
        octaves: Number of noise octaves
        persistence: Amplitude reduction per octave
        lacunarity: Frequency multiplier per octave
        seed: Random seed

    Returns:
        3D NumPy array of noise values

    Examples:
        >>> volume = generate_3d_noise_volume(64, 64, 64, 25.0, 4, 0.5)
        >>> # Use for cloud generation, cave systems, etc.
    )pbdoc");

    m.def(
        "generate_seamless_noise",
        [](int width, int height, double scale, int octaves, double persistence,
           int seed) {
            atom::algorithm::PerlinNoise noise(seed);

            py::array_t<double> result({height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    // Create seamless noise by using trigonometric functions
                    double s = static_cast<double>(x) / width;
                    double t = static_cast<double>(y) / height;

                    double nx = std::cos(s * 2 * M_PI) / (2 * M_PI);
                    double ny = std::cos(t * 2 * M_PI) / (2 * M_PI);
                    double nz = std::sin(s * 2 * M_PI) / (2 * M_PI);
                    double nw = std::sin(t * 2 * M_PI) / (2 * M_PI);

                    double noise_value = noise.octaveNoise(
                        nx * scale, ny * scale, 0.0, octaves, persistence);
                    ptr[y * width + x] = noise_value;
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"),
        py::arg("seed") = std::default_random_engine::default_seed,
        R"pbdoc(
    Generate seamlessly tileable noise texture.

    Args:
        width: Width of the texture
        height: Height of the texture
        scale: Noise scale
        octaves: Number of octaves
        persistence: Amplitude reduction per octave
        seed: Random seed

    Returns:
        2D NumPy array that tiles seamlessly

    Examples:
        >>> seamless = generate_seamless_noise(256, 256, 50.0, 4, 0.5)
        >>> # Perfect for repeating textures
    )pbdoc");

    m.def(
        "generate_ridged_noise",
        [](int width, int height, double scale, int octaves, double persistence,
           int seed) {
            atom::algorithm::PerlinNoise noise(seed);

            py::array_t<double> result({height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    double sample_x = x / scale;
                    double sample_y = y / scale;

                    // Ridged noise: abs(noise) * 2 - 1, then invert
                    double noise_value = noise.octaveNoise(
                        sample_x, sample_y, 0.0, octaves, persistence);
                    noise_value = 1.0 - std::abs(noise_value * 2.0 - 1.0);

                    ptr[y * width + x] = noise_value;
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"),
        py::arg("seed") = std::default_random_engine::default_seed,
        R"pbdoc(
    Generate ridged noise for mountain-like terrain.

    Args:
        width: Width of the noise map
        height: Height of the noise map
        scale: Noise scale
        octaves: Number of octaves
        persistence: Amplitude reduction per octave
        seed: Random seed

    Returns:
        2D NumPy array with ridged noise pattern

    Examples:
        >>> ridged = generate_ridged_noise(512, 512, 100.0, 6, 0.5)
        >>> # Great for mountain ranges and rocky terrain
    )pbdoc");

    m.def(
        "generate_billow_noise",
        [](int width, int height, double scale, int octaves, double persistence,
           int seed) {
            atom::algorithm::PerlinNoise noise(seed);

            py::array_t<double> result({height, width});
            py::buffer_info buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    double sample_x = x / scale;
                    double sample_y = y / scale;

                    // Billow noise: abs(noise) for cloud-like patterns
                    double noise_value = noise.octaveNoise(
                        sample_x, sample_y, 0.0, octaves, persistence);
                    noise_value = std::abs(noise_value * 2.0 - 1.0);

                    ptr[y * width + x] = noise_value;
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"),
        py::arg("seed") = std::default_random_engine::default_seed,
        R"pbdoc(
    Generate billow noise for cloud-like patterns.

    Args:
        width: Width of the noise map
        height: Height of the noise map
        scale: Noise scale
        octaves: Number of octaves
        persistence: Amplitude reduction per octave
        seed: Random seed

    Returns:
        2D NumPy array with billow noise pattern

    Examples:
        >>> billow = generate_billow_noise(256, 256, 75.0, 4, 0.6)
        >>> # Perfect for cloud formations and organic textures
    )pbdoc");

// Hardware acceleration info
#ifdef ATOM_USE_OPENCL
    m.attr("OPENCL_AVAILABLE") = true;
#else
    m.attr("OPENCL_AVAILABLE") = false;
#endif

    // Add version information
    m.attr("__version__") = "1.1.0";
}
