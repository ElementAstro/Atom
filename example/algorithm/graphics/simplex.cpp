/*
 * simplex.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating Simplex noise from atom/algorithm/graphics/simplex.hpp
 */

#include "atom/algorithm/graphics/simplex.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace atom::algorithm;

// Helper function to map noise value to ASCII character
char noiseToChar(f64 value) {
    // Normalize from [-1, 1] to [0, 1]
    f64 normalized = (value + 1.0) / 2.0;

    // Map to ASCII gradient
    const char* gradient = " .:-=+*#%@";
    int index = static_cast<int>(normalized * 9);
    index = std::clamp(index, 0, 9);
    return gradient[index];
}

// Demonstrate basic 2D simplex noise
void demonstrateSimplex2D() {
    std::cout << "\n=== 2D Simplex Noise ===\n";

    SimplexNoise noise(12345);

    std::cout << "\nSample 2D noise values:\n";
    for (f64 y = 0.0; y < 1.0; y += 0.2) {
        for (f64 x = 0.0; x < 1.0; x += 0.2) {
            f64 value = noise.noise2D(x * 4.0, y * 4.0);
            std::cout << std::fixed << std::setprecision(3) << std::setw(8)
                      << value;
        }
        std::cout << "\n";
    }

    // Generate ASCII art visualization
    std::cout << "\n2D Simplex Noise Visualization (40x20):\n";
    std::cout << std::string(42, '-') << "\n";

    for (int y = 0; y < 20; ++y) {
        std::cout << "|";
        for (int x = 0; x < 40; ++x) {
            f64 nx = static_cast<f64>(x) / 10.0;
            f64 ny = static_cast<f64>(y) / 10.0;
            f64 value = noise.noise2D(nx, ny);
            std::cout << noiseToChar(value);
        }
        std::cout << "|\n";
    }
    std::cout << std::string(42, '-') << "\n";
}

// Demonstrate 3D simplex noise
void demonstrateSimplex3D() {
    std::cout << "\n=== 3D Simplex Noise ===\n";

    SimplexNoise noise(54321);

    std::cout << "\nSample 3D noise values at different Z slices:\n";

    for (f64 z = 0.0; z <= 1.0; z += 0.5) {
        std::cout << "\nZ = " << z << ":\n";
        for (f64 y = 0.0; y < 1.0; y += 0.25) {
            for (f64 x = 0.0; x < 1.0; x += 0.25) {
                f64 value = noise.noise3D(x * 4.0, y * 4.0, z * 4.0);
                std::cout << std::fixed << std::setprecision(3) << std::setw(8)
                          << value;
            }
            std::cout << "\n";
        }
    }
}

// Demonstrate octave noise (fractal noise)
void demonstrateOctaveNoise() {
    std::cout << "\n=== Octave (Fractal) Simplex Noise ===\n";

    SimplexNoise noise(99999);

    std::cout << "\nComparing different octave counts:\n";
    std::cout << "Position (0.5, 0.5, 0.0):\n";

    for (int octaves = 1; octaves <= 6; ++octaves) {
        f64 value = noise.octaveNoise2D(0.5, 0.5, octaves, 0.5);
        std::cout << "  " << octaves << " octave(s): " << std::fixed
                  << std::setprecision(4) << value << "\n";
    }

    // Visualize octave noise
    std::cout << "\nOctave noise visualization (4 octaves, persistence 0.5):\n";
    std::cout << std::string(42, '-') << "\n";

    for (int y = 0; y < 15; ++y) {
        std::cout << "|";
        for (int x = 0; x < 40; ++x) {
            f64 nx = static_cast<f64>(x) / 10.0;
            f64 ny = static_cast<f64>(y) / 10.0;
            f64 value = noise.octaveNoise2D(nx, ny, 4, 0.5);
            std::cout << noiseToChar(value);
        }
        std::cout << "|\n";
    }
    std::cout << std::string(42, '-') << "\n";
}

// Demonstrate noise animation (time-varying)
void demonstrateNoiseAnimation() {
    std::cout << "\n=== Animated Simplex Noise ===\n";
    std::cout << "Showing 5 frames of animated noise:\n";

    SimplexNoise noise(11111);

    for (int frame = 0; frame < 5; ++frame) {
        f64 time = static_cast<f64>(frame) * 0.3;
        std::cout << "\nFrame " << frame << " (t=" << std::fixed
                  << std::setprecision(1) << time << "):\n";
        std::cout << std::string(22, '-') << "\n";

        for (int y = 0; y < 8; ++y) {
            std::cout << "|";
            for (int x = 0; x < 20; ++x) {
                f64 nx = static_cast<f64>(x) / 5.0;
                f64 ny = static_cast<f64>(y) / 5.0;
                f64 value = noise.noise3D(nx, ny, time);
                std::cout << noiseToChar(value);
            }
            std::cout << "|\n";
        }
        std::cout << std::string(22, '-') << "\n";
    }
}

// Demonstrate different seeds
void demonstrateDifferentSeeds() {
    std::cout << "\n=== Different Seeds Comparison ===\n";

    std::vector<u32> seeds = {0, 12345, 99999, 42};

    for (u32 seed : seeds) {
        SimplexNoise noise(seed);
        std::cout << "\nSeed " << seed << ":\n";

        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 20; ++x) {
                f64 nx = static_cast<f64>(x) / 5.0;
                f64 ny = static_cast<f64>(y) / 5.0;
                f64 value = noise.noise2D(nx, ny);
                std::cout << noiseToChar(value);
            }
            std::cout << "\n";
        }
    }
}

// Benchmark simplex noise performance
void benchmarkSimplexNoise() {
    std::cout << "\n=== Simplex Noise Performance Benchmark ===\n";

    SimplexNoise noise(12345);
    constexpr int ITERATIONS = 100000;

    // Benchmark 2D noise
    auto start = std::chrono::high_resolution_clock::now();
    f64 sum2d = 0.0;
    for (int i = 0; i < ITERATIONS; ++i) {
        f64 x = static_cast<f64>(i % 100) / 10.0;
        f64 y = static_cast<f64>(i / 100) / 10.0;
        sum2d += noise.noise2D(x, y);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration2d =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "2D noise: " << ITERATIONS << " calls in "
              << duration2d.count() << " us ("
              << (ITERATIONS * 1000000.0 / duration2d.count())
              << " calls/sec)\n";

    // Benchmark 3D noise
    start = std::chrono::high_resolution_clock::now();
    f64 sum3d = 0.0;
    for (int i = 0; i < ITERATIONS; ++i) {
        f64 x = static_cast<f64>(i % 100) / 10.0;
        f64 y = static_cast<f64>((i / 100) % 100) / 10.0;
        f64 z = static_cast<f64>(i / 10000) / 10.0;
        sum3d += noise.noise3D(x, y, z);
    }
    end = std::chrono::high_resolution_clock::now();
    auto duration3d =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "3D noise: " << ITERATIONS << " calls in "
              << duration3d.count() << " us ("
              << (ITERATIONS * 1000000.0 / duration3d.count())
              << " calls/sec)\n";

    // Benchmark octave noise
    start = std::chrono::high_resolution_clock::now();
    f64 sum_octave = 0.0;
    for (int i = 0; i < ITERATIONS / 10; ++i) {
        f64 x = static_cast<f64>(i % 100) / 10.0;
        f64 y = static_cast<f64>(i / 100) / 10.0;
        sum_octave += noise.octaveNoise2D(x, y, 4, 0.5);
    }
    end = std::chrono::high_resolution_clock::now();
    auto duration_octave =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Octave noise (4 octaves): " << (ITERATIONS / 10)
              << " calls in " << duration_octave.count() << " us ("
              << ((ITERATIONS / 10) * 1000000.0 / duration_octave.count())
              << " calls/sec)\n";

    // Prevent optimization
    std::cout << "(Checksum: " << (sum2d + sum3d + sum_octave) << ")\n";
}

// Generate terrain-like noise map
void demonstrateTerrainGeneration() {
    std::cout << "\n=== Terrain Generation Example ===\n";

    SimplexNoise noise(42);

    std::cout << "\nTerrain height map (using octave noise):\n";
    std::cout << "Legend: ' '=water, '.'=beach, '-'=plains, "
              << "'+'=hills, '#'=mountains\n\n";

    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 50; ++x) {
            f64 nx = static_cast<f64>(x) / 15.0;
            f64 ny = static_cast<f64>(y) / 15.0;

            // Generate terrain height using octave noise
            f64 height = noise.octaveNoise2D(nx, ny, 6, 0.5);

            // Map height to terrain type
            char terrain;
            if (height < -0.3) {
                terrain = ' ';  // Deep water
            } else if (height < -0.1) {
                terrain = '~';  // Shallow water
            } else if (height < 0.0) {
                terrain = '.';  // Beach
            } else if (height < 0.3) {
                terrain = '-';  // Plains
            } else if (height < 0.6) {
                terrain = '+';  // Hills
            } else {
                terrain = '#';  // Mountains
            }
            std::cout << terrain;
        }
        std::cout << "\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Simplex Noise Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateSimplex2D();
        demonstrateSimplex3D();
        demonstrateOctaveNoise();
        demonstrateNoiseAnimation();
        demonstrateDifferentSeeds();
        benchmarkSimplexNoise();
        demonstrateTerrainGeneration();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
