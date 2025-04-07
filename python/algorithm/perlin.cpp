#include "atom/algorithm/perlin.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(perlin, m) {
    m.doc() = "Perlin noise generation module for the atom package";

    // Register exception translations
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

    // PerlinNoise class binding
    py::class_<atom::algorithm::PerlinNoise>(
        m, "PerlinNoise",
        R"(Implements the Perlin noise algorithm for procedural texture generation.

Perlin noise is a gradient noise developed by Ken Perlin that is commonly used to generate 
procedural textures, natural-looking terrain, clouds, and other visual effects.

Args:
    seed: A seed value for the random number generator. Default uses system seed.

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> # Create a noise generator with a specific seed
    >>> noise = PerlinNoise(42)
    >>> # Generate a noise value at coordinates (0.5, 0.3, 0.7)
    >>> value = noise.noise(0.5, 0.3, 0.7)
)")
        .def(py::init<unsigned int>(),
             py::arg("seed") = std::default_random_engine::default_seed,
             "Constructs a PerlinNoise object with the given seed.")
        .def(
            "noise",
            [](const atom::algorithm::PerlinNoise& self, double x, double y,
               double z) { return self.noise(x, y, z); },
            py::arg("x"), py::arg("y"), py::arg("z"),
            R"(Generates a 3D Perlin noise value.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space
    z: Z-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
        .def(
            "noise",
            [](const atom::algorithm::PerlinNoise& self, double x, double y) {
                return self.noise(x, y, 0.0);
            },
            py::arg("x"), py::arg("y"),
            R"(Generates a 2D Perlin noise value.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
        .def(
            "noise",
            [](const atom::algorithm::PerlinNoise& self, double x) {
                return self.noise(x, 0.0, 0.0);
            },
            py::arg("x"),
            R"(Generates a 1D Perlin noise value.

Args:
    x: X-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
        .def("octave_noise", &atom::algorithm::PerlinNoise::octaveNoise<double>,
             py::arg("x"), py::arg("y"), py::arg("z"), py::arg("octaves"),
             py::arg("persistence"),
             R"(Generates octave noise by combining multiple noise frequencies.

Octave noise produces more natural-looking results by summing multiple frequencies
of noise with decreasing amplitude.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space
    z: Z-coordinate in noise space
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)

Returns:
    A combined noise value in the range [0, 1]

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> noise = PerlinNoise(42)
    >>> # Generate 4 octaves of noise with 0.5 persistence
    >>> value = noise.octave_noise(0.5, 0.3, 0.7, 4, 0.5)
)")
        .def("generate_noise_map",
             &atom::algorithm::PerlinNoise::generateNoiseMap, py::arg("width"),
             py::arg("height"), py::arg("scale"), py::arg("octaves"),
             py::arg("persistence"), py::arg("lacunarity") = 0.0,
             py::arg("seed") = std::default_random_engine::default_seed,
             R"(Generates a 2D noise map.

Creates a 2D grid of Perlin noise values, useful for generating terrain heightmaps,
textures, or other procedural content.

Args:
    width: Width of the noise map in pixels
    height: Height of the noise map in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    lacunarity: Frequency multiplier between octaves (not used in current implementation)
    seed: Random seed for offset generation (default uses system seed)

Returns:
    A 2D vector containing the noise values

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> import matplotlib.pyplot as plt
    >>> import numpy as np
    >>> noise = PerlinNoise(42)
    >>> # Generate a 256x256 noise map
    >>> noise_map = noise.generate_noise_map(256, 256, 50.0, 4, 0.5)
    >>> # Convert to numpy array for visualization
    >>> plt.imshow(np.array(noise_map), cmap='terrain')
    >>> plt.colorbar()
)");

    // Utility functions for numpy integration
    m.def(
        "generate_noise_array",
        [](int width, int height, double scale, int octaves, double persistence,
           unsigned int seed) {
            // Create PerlinNoise generator
            atom::algorithm::PerlinNoise noise(seed);

            // Generate noise map
            auto noise_map = noise.generateNoiseMap(
                width, height, scale, octaves, persistence, 0.0, seed);

            // Convert to numpy array
            py::array_t<double> result({height, width});
            auto buf = result.request();
            double* ptr = static_cast<double*>(buf.ptr);

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    ptr[y * width + x] = noise_map[y][x];
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"),
        py::arg("seed") = std::default_random_engine::default_seed,
        R"(Generates a 2D numpy array of Perlin noise.

A convenience function that creates a PerlinNoise object and generates a noise map
directly as a numpy array.

Args:
    width: Width of the noise map in pixels
    height: Height of the noise map in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    seed: Random seed for the noise generator and offset generation

Returns:
    A 2D numpy array containing the noise values

Examples:
    >>> from atom.algorithm.perlin import generate_noise_array
    >>> import matplotlib.pyplot as plt
    >>> # Generate a 256x256 noise map with 4 octaves
    >>> noise_array = generate_noise_array(256, 256, 50.0, 4, 0.5, 42)
    >>> plt.imshow(noise_array, cmap='terrain')
    >>> plt.colorbar()
)");

    m.def(
        "generate_terrain",
        [](int width, int height, double scale, int octaves, double persistence,
           unsigned int seed, std::vector<double> thresholds,
           std::vector<int> colors) {
            // Input validation
            if (thresholds.size() + 1 != colors.size()) {
                throw std::invalid_argument(
                    "Number of thresholds must be one less than number of "
                    "colors");
            }

            // Create PerlinNoise generator
            atom::algorithm::PerlinNoise noise(seed);

            // Generate noise map
            auto noise_map = noise.generateNoiseMap(
                width, height, scale, octaves, persistence, 0.0, seed);

            // Create RGB array for the terrain
            py::array_t<uint8_t> result({height, width, 3});
            auto buf = result.request();
            uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    double value = noise_map[y][x];
                    int color_index =
                        colors.size() - 1;  // Default to last color

                    // Find appropriate color based on thresholds
                    for (size_t i = 0; i < thresholds.size(); i++) {
                        if (value < thresholds[i]) {
                            color_index = i;
                            break;
                        }
                    }

                    int rgb_value = colors[color_index];
                    uint8_t r = (rgb_value >> 16) & 0xFF;
                    uint8_t g = (rgb_value >> 8) & 0xFF;
                    uint8_t b = rgb_value & 0xFF;

                    // Set RGB values in the result array
                    ptr[(y * width + x) * 3 + 0] = r;
                    ptr[(y * width + x) * 3 + 1] = g;
                    ptr[(y * width + x) * 3 + 2] = b;
                }
            }

            return result;
        },
        py::arg("width"), py::arg("height"), py::arg("scale"),
        py::arg("octaves"), py::arg("persistence"),
        py::arg("seed") = std::default_random_engine::default_seed,
        py::arg("thresholds") = std::vector<double>{0.3, 0.4, 0.5, 0.6, 0.7},
        py::arg("colors") = std::vector<int>{0x0000FF, 0x00FFFF, 0x00FF00,
                                             0xFFFF00, 0xA52A2A, 0xFFFFFF},
        R"(Generates a terrain visualization from Perlin noise.

Creates a colored terrain image based on Perlin noise values and the provided thresholds.
This is useful for quick visualization of heightmaps as terrain.

Args:
    width: Width of the terrain in pixels
    height: Height of the terrain in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    seed: Random seed for the noise generator
    thresholds: List of threshold values for terrain types (default: water, shore, grass, mountains, snow)
    colors: List of RGB colors as integers for each terrain type (one more than thresholds)

Returns:
    A 3D numpy array (height, width, RGB) containing the terrain image

Examples:
    >>> from atom.algorithm.perlin import generate_terrain
    >>> import matplotlib.pyplot as plt
    >>> # Generate a 512x512 terrain map
    >>> terrain = generate_terrain(512, 512, 100.0, 6, 0.5, 42)
    >>> plt.figure(figsize=(10, 10))
    >>> plt.imshow(terrain)
    >>> plt.axis('off')
)");

    // Create convenience 2D noise function
    m.def("noise2d", [](double x, double y, unsigned int seed) {
        atom::algorithm::
            Per  // filepath:
                 // d:\msys64\home\qwdma\Atom\python\algorithm\perlin.cpp
#include "atom/algorithm/perlin.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

            namespace py = pybind11;

        PYBIND11_MODULE(perlin, m) {
            m.doc() = "Perlin noise generation module for the atom package";

            // Register exception translations
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

            // PerlinNoise class binding
            py::class_<atom::algorithm::PerlinNoise>(
                m, "PerlinNoise",
                R"(Implements the Perlin noise algorithm for procedural texture generation.

Perlin noise is a gradient noise developed by Ken Perlin that is commonly used to generate 
procedural textures, natural-looking terrain, clouds, and other visual effects.

Args:
    seed: A seed value for the random number generator. Default uses system seed.

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> # Create a noise generator with a specific seed
    >>> noise = PerlinNoise(42)
    >>> # Generate a noise value at coordinates (0.5, 0.3, 0.7)
    >>> value = noise.noise(0.5, 0.3, 0.7)
)")
                .def(py::init<unsigned int>(),
                     py::arg("seed") = std::default_random_engine::default_seed,
                     "Constructs a PerlinNoise object with the given seed.")
                .def(
                    "noise",
                    [](const atom::algorithm::PerlinNoise& self, double x,
                       double y, double z) { return self.noise(x, y, z); },
                    py::arg("x"), py::arg("y"), py::arg("z"),
                    R"(Generates a 3D Perlin noise value.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space
    z: Z-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
                .def(
                    "noise",
                    [](const atom::algorithm::PerlinNoise& self, double x,
                       double y) { return self.noise(x, y, 0.0); },
                    py::arg("x"), py::arg("y"),
                    R"(Generates a 2D Perlin noise value.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
                .def(
                    "noise",
                    [](const atom::algorithm::PerlinNoise& self, double x) {
                        return self.noise(x, 0.0, 0.0);
                    },
                    py::arg("x"),
                    R"(Generates a 1D Perlin noise value.

Args:
    x: X-coordinate in noise space

Returns:
    A noise value in the range [0, 1]
)")
                .def(
                    "octave_noise",
                    &atom::algorithm::PerlinNoise::octaveNoise<double>,
                    py::arg("x"), py::arg("y"), py::arg("z"),
                    py::arg("octaves"), py::arg("persistence"),
                    R"(Generates octave noise by combining multiple noise frequencies.

Octave noise produces more natural-looking results by summing multiple frequencies
of noise with decreasing amplitude.

Args:
    x: X-coordinate in noise space
    y: Y-coordinate in noise space
    z: Z-coordinate in noise space
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)

Returns:
    A combined noise value in the range [0, 1]

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> noise = PerlinNoise(42)
    >>> # Generate 4 octaves of noise with 0.5 persistence
    >>> value = noise.octave_noise(0.5, 0.3, 0.7, 4, 0.5)
)")
                .def("generate_noise_map",
                     &atom::algorithm::PerlinNoise::generateNoiseMap,
                     py::arg("width"), py::arg("height"), py::arg("scale"),
                     py::arg("octaves"), py::arg("persistence"),
                     py::arg("lacunarity") = 0.0,
                     py::arg("seed") = std::default_random_engine::default_seed,
                     R"(Generates a 2D noise map.

Creates a 2D grid of Perlin noise values, useful for generating terrain heightmaps,
textures, or other procedural content.

Args:
    width: Width of the noise map in pixels
    height: Height of the noise map in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    lacunarity: Frequency multiplier between octaves (not used in current implementation)
    seed: Random seed for offset generation (default uses system seed)

Returns:
    A 2D vector containing the noise values

Examples:
    >>> from atom.algorithm.perlin import PerlinNoise
    >>> import matplotlib.pyplot as plt
    >>> import numpy as np
    >>> noise = PerlinNoise(42)
    >>> # Generate a 256x256 noise map
    >>> noise_map = noise.generate_noise_map(256, 256, 50.0, 4, 0.5)
    >>> # Convert to numpy array for visualization
    >>> plt.imshow(np.array(noise_map), cmap='terrain')
    >>> plt.colorbar()
)");

            // Utility functions for numpy integration
            m.def(
                "generate_noise_array",
                [](int width, int height, double scale, int octaves,
                   double persistence, unsigned int seed) {
                    // Create PerlinNoise generator
                    atom::algorithm::PerlinNoise noise(seed);

                    // Generate noise map
                    auto noise_map = noise.generateNoiseMap(
                        width, height, scale, octaves, persistence, 0.0, seed);

                    // Convert to numpy array
                    py::array_t<double> result({height, width});
                    auto buf = result.request();
                    double* ptr = static_cast<double*>(buf.ptr);

                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            ptr[y * width + x] = noise_map[y][x];
                        }
                    }

                    return result;
                },
                py::arg("width"), py::arg("height"), py::arg("scale"),
                py::arg("octaves"), py::arg("persistence"),
                py::arg("seed") = std::default_random_engine::default_seed,
                R"(Generates a 2D numpy array of Perlin noise.

A convenience function that creates a PerlinNoise object and generates a noise map
directly as a numpy array.

Args:
    width: Width of the noise map in pixels
    height: Height of the noise map in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    seed: Random seed for the noise generator and offset generation

Returns:
    A 2D numpy array containing the noise values

Examples:
    >>> from atom.algorithm.perlin import generate_noise_array
    >>> import matplotlib.pyplot as plt
    >>> # Generate a 256x256 noise map with 4 octaves
    >>> noise_array = generate_noise_array(256, 256, 50.0, 4, 0.5, 42)
    >>> plt.imshow(noise_array, cmap='terrain')
    >>> plt.colorbar()
)");

            m.def(
                "generate_terrain",
                [](int width, int height, double scale, int octaves,
                   double persistence, unsigned int seed,
                   std::vector<double> thresholds, std::vector<int> colors) {
                    // Input validation
                    if (thresholds.size() + 1 != colors.size()) {
                        throw std::invalid_argument(
                            "Number of thresholds must be one less than number "
                            "of colors");
                    }

                    // Create PerlinNoise generator
                    atom::algorithm::PerlinNoise noise(seed);

                    // Generate noise map
                    auto noise_map = noise.generateNoiseMap(
                        width, height, scale, octaves, persistence, 0.0, seed);

                    // Create RGB array for the terrain
                    py::array_t<uint8_t> result({height, width, 3});
                    auto buf = result.request();
                    uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);

                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            double value = noise_map[y][x];
                            int color_index =
                                colors.size() - 1;  // Default to last color

                            // Find appropriate color based on thresholds
                            for (size_t i = 0; i < thresholds.size(); i++) {
                                if (value < thresholds[i]) {
                                    color_index = i;
                                    break;
                                }
                            }

                            int rgb_value = colors[color_index];
                            uint8_t r = (rgb_value >> 16) & 0xFF;
                            uint8_t g = (rgb_value >> 8) & 0xFF;
                            uint8_t b = rgb_value & 0xFF;

                            // Set RGB values in the result array
                            ptr[(y * width + x) * 3 + 0] = r;
                            ptr[(y * width + x) * 3 + 1] = g;
                            ptr[(y * width + x) * 3 + 2] = b;
                        }
                    }

                    return result;
                },
                py::arg("width"), py::arg("height"), py::arg("scale"),
                py::arg("octaves"), py::arg("persistence"),
                py::arg("seed") = std::default_random_engine::default_seed,
                py::arg("thresholds") =
                    std::vector<double>{0.3, 0.4, 0.5, 0.6, 0.7},
                py::arg("colors") =
                    std::vector<int>{0x0000FF, 0x00FFFF, 0x00FF00, 0xFFFF00,
                                     0xA52A2A, 0xFFFFFF},
                R"(Generates a terrain visualization from Perlin noise.

Creates a colored terrain image based on Perlin noise values and the provided thresholds.
This is useful for quick visualization of heightmaps as terrain.

Args:
    width: Width of the terrain in pixels
    height: Height of the terrain in pixels
    scale: Scale of the noise (lower values = more zoomed out)
    octaves: Number of frequencies to combine
    persistence: Amplitude multiplier between octaves (0.0-1.0)
    seed: Random seed for the noise generator
    thresholds: List of threshold values for terrain types (default: water, shore, grass, mountains, snow)
    colors: List of RGB colors as integers for each terrain type (one more than thresholds)

Returns:
    A 3D numpy array (height, width, RGB) containing the terrain image

Examples:
    >>> from atom.algorithm.perlin import generate_terrain
    >>> import matplotlib.pyplot as plt
    >>> # Generate a 512x512 terrain map
    >>> terrain = generate_terrain(512, 512, 100.0, 6, 0.5, 42)
    >>> plt.figure(figsize=(10, 10))
    >>> plt.imshow(terrain)
    >>> plt.axis('off')
)");

            // Create convenience 2D noise function
    m.def("noise2d", [](double x, double y, unsigned int seed) {
            atom::algorithm::Per