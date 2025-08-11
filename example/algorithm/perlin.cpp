#include "atom/algorithm/perlin.hpp"
#include <iostream>
#include <vector>

int main() {
    using namespace atom::algorithm;

    // Create a PerlinNoise object with a specific seed
    unsigned int seed = 42;
    PerlinNoise perlin(seed);

    // Generate Perlin noise for a single point (x, y, z)
    double x = 1.5;
    double y = 2.5;
    double z = 3.5;
    double noiseValue = perlin.noise(x, y, z);
    std::cout << "Perlin Noise at (" << x << ", " << y << ", " << z
              << "): " << noiseValue << std::endl;

    // Generate octave Perlin noise for a single point (x, y, z)
    int octaves = 4;
    double persistence = 0.5;
    double octaveNoiseValue = perlin.octaveNoise(x, y, z, octaves, persistence);
    std::cout << "Octave Perlin Noise at (" << x << ", " << y << ", " << z
              << ") with " << octaves << " octaves and persistence "
              << persistence << ": " << octaveNoiseValue << std::endl;

    // Generate a noise map
    int width = 5;
    int height = 5;
    double scale = 10.0;
    std::vector<std::vector<double>> noiseMap = perlin.generateNoiseMap(
        width, height, scale, octaves, persistence, seed);

    // Print the noise map
    std::cout << "Noise Map:" << std::endl;
    for (const auto& row : noiseMap) {
        for (const auto& value : row) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
