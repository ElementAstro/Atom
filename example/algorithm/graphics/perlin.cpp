/**
 * @file perlin.cpp
 * @brief Comprehensive example demonstrating Perlin noise generation
 *
 * This example shows how to:
 * - Generate basic Perlin noise for procedural content
 * - Use octave noise for more complex patterns
 * - Create noise maps for terrain and texture generation
 * - Demonstrate SIMD optimizations for performance
 * - Show different applications and use cases
 * - Handle different scales and parameters
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/perlin.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates basic Perlin noise generation
 */
void demonstrateBasicPerlinNoise() {
    printHeader("Basic Perlin Noise Generation");

    try {
        // Create a PerlinNoise object with a specific seed
        unsigned int seed = 42;
        PerlinNoise perlin(seed);

        std::cout << "Testing Perlin noise at various coordinates:\n";
        std::cout << "Seed: " << seed << "\n\n";

        // Test various coordinates
        std::vector<std::tuple<double, double, double>> testPoints = {
            {0.0, 0.0, 0.0},
            {1.5, 2.5, 3.5},
            {10.0, 20.0, 30.0},
            {0.1, 0.2, 0.3},
            {-5.0, -10.0, -15.0}};

        for (const auto& [x, y, z] : testPoints) {
            double noiseValue = perlin.noise(x, y, z);
            std::cout << "  Point (" << std::fixed << std::setprecision(1) << x
                      << ", " << y << ", " << z << "): " << std::setprecision(6)
                      << noiseValue << "\n";
        }

        // Test 2D noise (z = 0)
        std::cout << "\n2D Perlin noise (z = 0):\n";
        for (double x = 0.0; x <= 2.0; x += 0.5) {
            for (double y = 0.0; y <= 2.0; y += 0.5) {
                double noise2D = perlin.noise(x, y, 0.0);
                std::cout << "  (" << std::setprecision(1) << x << "," << y
                          << "): " << std::setprecision(4) << noise2D << "\n";
            }
        }

        // Test noise range
        std::cout << "\nNoise range analysis (1000 samples):\n";
        double minNoise = 1.0, maxNoise = -1.0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-100.0, 100.0);

        for (int i = 0; i < 1000; ++i) {
            double x = dis(gen);
            double y = dis(gen);
            double z = dis(gen);
            double noise = perlin.noise(x, y, z);
            minNoise = std::min(minNoise, noise);
            maxNoise = std::max(maxNoise, noise);
        }

        std::cout << "  Minimum noise value: " << minNoise << "\n";
        std::cout << "  Maximum noise value: " << maxNoise << "\n";
        std::cout << "  Range: [" << minNoise << ", " << maxNoise << "]\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic Perlin noise demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates octave noise for complex patterns
 */
void demonstrateOctaveNoise() {
    printHeader("Octave Noise for Complex Patterns");

    try {
        unsigned int seed = 12345;
        PerlinNoise perlin(seed);

        std::cout << "Octave noise combines multiple frequencies for richer "
                     "patterns:\n\n";

        // Test different octave configurations
        std::vector<std::tuple<int, double, std::string>> configs = {
            {1, 0.5, "Single octave (basic noise)"},
            {2, 0.5, "Two octaves"},
            {4, 0.5, "Four octaves"},
            {8, 0.5, "Eight octaves"},
            {4, 0.3, "Four octaves, low persistence"},
            {4, 0.7, "Four octaves, high persistence"}};

        double x = 5.0, y = 5.0, z = 5.0;

        for (const auto& [octaves, persistence, description] : configs) {
            double octaveNoise =
                perlin.octaveNoise(x, y, z, octaves, persistence);
            std::cout << description << ":\n";
            std::cout << "  Octaves: " << octaves
                      << ", Persistence: " << persistence << "\n";
            std::cout << "  Noise value: " << std::fixed << std::setprecision(6)
                      << octaveNoise << "\n\n";
        }

        // Demonstrate how octaves affect pattern complexity
        std::cout << "Pattern complexity demonstration (2D slice at z=0):\n";
        std::cout << "Coordinates: ";
        for (double x = 0.0; x <= 4.0; x += 1.0) {
            std::cout << std::setw(8) << std::fixed << std::setprecision(1)
                      << x;
        }
        std::cout << "\n";

        for (int octaves = 1; octaves <= 4; octaves++) {
            std::cout << octaves << " octave" << (octaves > 1 ? "s" : "")
                      << ": ";
            for (double x = 0.0; x <= 4.0; x += 1.0) {
                double noise = perlin.octaveNoise(x, 2.0, 0.0, octaves, 0.5);
                std::cout << std::setw(8) << std::fixed << std::setprecision(4)
                          << noise;
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in octave noise demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates noise map generation for terrain and textures
 */
void demonstrateNoiseMapGeneration() {
    printHeader("Noise Map Generation");

    try {
        unsigned int seed = 54321;
        PerlinNoise perlin(seed);

        std::cout << "Generating noise maps for various applications:\n\n";

        // Small demonstration map
        std::cout << "Small 8x8 terrain heightmap:\n";
        int width = 8, height = 8;
        double scale = 10.0;
        int octaves = 4;
        double persistence = 0.5;

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<double>> noiseMap = perlin.generateNoiseMap(
            width, height, scale, octaves, persistence, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Generation time: " << duration.count() << " μs\n";
        std::cout << "Scale: " << scale << ", Octaves: " << octaves
                  << ", Persistence: " << persistence << "\n\n";

        // Print as heightmap with visual representation
        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                // Convert noise value to height character
                char heightChar;
                if (value < -0.5)
                    heightChar = ' ';
                else if (value < -0.2)
                    heightChar = '.';
                else if (value < 0.0)
                    heightChar = '-';
                else if (value < 0.2)
                    heightChar = '~';
                else if (value < 0.5)
                    heightChar = '^';
                else
                    heightChar = '#';

                std::cout << heightChar << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\nLegend: ' '=deep water, '.'=water, '-'=shore, "
                     "'~'=plains, '^'=hills, '#'=mountains\n";

        // Test different scales
        std::cout << "\nScale comparison (4x4 maps):\n";
        std::vector<double> scales = {1.0, 5.0, 20.0, 50.0};

        for (double testScale : scales) {
            std::cout << "\nScale " << testScale << ":\n";
            auto scaleMap =
                perlin.generateNoiseMap(4, 4, testScale, 3, 0.5, seed);

            for (const auto& row : scaleMap) {
                for (const auto& value : row) {
                    std::cout << std::setw(7) << std::fixed
                              << std::setprecision(3) << value << " ";
                }
                std::cout << "\n";
            }
        }

        // Performance test with larger maps
        std::cout << "\nPerformance test with different map sizes:\n";
        std::vector<std::pair<int, int>> sizes = {
            {16, 16}, {32, 32}, {64, 64}, {128, 128}};

        for (const auto& [w, h] : sizes) {
            start = std::chrono::high_resolution_clock::now();
            auto perfMap = perlin.generateNoiseMap(w, h, 20.0, 4, 0.5, seed);
            end = std::chrono::high_resolution_clock::now();
            auto perfDuration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  " << w << "x" << h
                      << " map: " << perfDuration.count() << " μs\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in noise map demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive Perlin noise capabilities
 */
int main() {
    std::cout << "=== Atom Perlin Noise Comprehensive Example ===\n";
    std::cout
        << "Demonstrating Perlin noise generation for procedural content...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicPerlinNoise();
        demonstrateOctaveNoise();
        demonstrateNoiseMapGeneration();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Perlin Noise Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The Perlin noise algorithm module provides:\n";
        std::cout << "  ✓ High-quality Perlin noise generation\n";
        std::cout << "  ✓ Octave noise for complex patterns\n";
        std::cout << "  ✓ Efficient noise map generation\n";
        std::cout << "  ✓ SIMD optimizations for performance\n";
        std::cout
            << "  ✓ Suitable for terrain, texture, and procedural generation\n";
        std::cout << "  ✓ Configurable parameters for different use cases\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in Perlin noise example: " << e.what()
                  << "\n";
        return 1;
    }
}
