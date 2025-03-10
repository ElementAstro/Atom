#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <string>
#include <vector>

#include "atom/algorithm/perlin.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Test fixture for PerlinNoise tests
class PerlinNoiseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Helper function to check if values are within expected range [0, 1]
    void checkNoiseRange(const std::vector<std::vector<double>>& noiseMap) {
        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                EXPECT_GE(value, 0.0);
                EXPECT_LE(value, 1.0);
            }
        }
    }

    // Helper to calculate standard deviation of noise values
    double calculateStandardDeviation(
        const std::vector<std::vector<double>>& noiseMap) {
        double sum = 0.0;
        double sumSquared = 0.0;
        size_t count = 0;

        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                sum += value;
                sumSquared += value * value;
                count++;
            }
        }

        double mean = sum / count;
        double variance = (sumSquared / count) - (mean * mean);
        return std::sqrt(variance);
    }

    // Helper to calculate average value
    double calculateAverage(const std::vector<std::vector<double>>& noiseMap) {
        double sum = 0.0;
        size_t count = 0;

        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                sum += value;
                count++;
            }
        }

        return sum / count;
    }

    // Helper to visualize noise (useful for debugging but not for automated
    // tests)
    void saveNoiseMapAsPPM(const std::vector<std::vector<double>>& noiseMap,
                           const std::string& filename) {
        int height = noiseMap.size();
        int width = height > 0 ? noiseMap[0].size() : 0;

        std::ofstream file(filename);
        file << "P3\n" << width << " " << height << "\n255\n";

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int value = static_cast<int>(noiseMap[y][x] * 255);
                file << value << " " << value << " " << value << " ";
            }
            file << "\n";
        }
    }
};

// Basic functionality tests
TEST_F(PerlinNoiseTest, NoiseInRange) {
    PerlinNoise noise(42);  // Fixed seed for deterministic tests

    // Test a grid of points
    for (double x = -10.0; x <= 10.0; x += 1.0) {
        for (double y = -10.0; y <= 10.0; y += 1.0) {
            for (double z = -10.0; z <= 10.0; z += 1.0) {
                double value = noise.noise(x, y, z);
                EXPECT_GE(value, 0.0) << "Noise value out of range at (" << x
                                      << ", " << y << ", " << z << ")";
                EXPECT_LE(value, 1.0) << "Noise value out of range at (" << x
                                      << ", " << y << ", " << z << ")";
            }
        }
    }
}

TEST_F(PerlinNoiseTest, FloatAndDoubleSupport) {
    PerlinNoise noise(42);

    // Test with float coordinates
    float xf = 1.5f, yf = 2.5f, zf = 3.5f;
    float noiseValueFloat = noise.noise(xf, yf, zf);

    // Test with double coordinates
    double xd = 1.5, yd = 2.5, zd = 3.5;
    double noiseValueDouble = noise.noise(xd, yd, zd);

    // They should be reasonably close
    EXPECT_NEAR(noiseValueFloat, noiseValueDouble, 1e-6);

    // Both should be in range [0,1]
    EXPECT_GE(noiseValueFloat, 0.0f);
    EXPECT_LE(noiseValueFloat, 1.0f);
    EXPECT_GE(noiseValueDouble, 0.0);
    EXPECT_LE(noiseValueDouble, 1.0);
}

TEST_F(PerlinNoiseTest, DeterministicOutput) {
    // Two noise generators with the same seed should produce identical output
    PerlinNoise noise1(123);
    PerlinNoise noise2(123);

    for (double x = -5.0; x <= 5.0; x += 0.5) {
        for (double y = -5.0; y <= 5.0; y += 0.5) {
            for (double z = -5.0; z <= 5.0; z += 0.5) {
                EXPECT_DOUBLE_EQ(noise1.noise(x, y, z), noise2.noise(x, y, z));
            }
        }
    }

    // Different seeds should (likely) produce different output
    PerlinNoise noise3(456);
    bool foundDifference = false;

    for (double x = -5.0; x <= 5.0; x += 0.5) {
        for (double y = -5.0; y <= 5.0; y += 0.5) {
            for (double z = -5.0; z <= 5.0; z += 0.5) {
                if (noise1.noise(x, y, z) != noise3.noise(x, y, z)) {
                    foundDifference = true;
                    break;
                }
            }
            if (foundDifference)
                break;
        }
        if (foundDifference)
            break;
    }

    EXPECT_TRUE(foundDifference)
        << "Different seeds should produce different noise patterns";
}

// Test octave noise functionality
TEST_F(PerlinNoiseTest, OctaveNoise) {
    PerlinNoise noise(42);

    // Test single octave (should be equivalent to regular noise)
    double x = 1.5, y = 2.5, z = 3.5;
    EXPECT_DOUBLE_EQ(noise.noise(x, y, z), noise.octaveNoise(x, y, z, 1, 0.5));

    // Test multiple octaves
    double value = noise.octaveNoise(x, y, z, 5, 0.5);
    EXPECT_GE(value, 0.0);
    EXPECT_LE(value, 1.0);

    // Test that persistence affects the result
    double valueLowPersistence = noise.octaveNoise(x, y, z, 5, 0.1);
    double valueHighPersistence = noise.octaveNoise(x, y, z, 5, 0.9);

    // Values should be different with different persistence
    EXPECT_NE(valueLowPersistence, valueHighPersistence);
}

TEST_F(PerlinNoiseTest, OctaveNoiseConvergence) {
    PerlinNoise noise(42);

    // Test with extreme number of octaves
    double x = 1.5, y = 2.5, z = 3.5;
    double value1 = noise.octaveNoise(x, y, z, 10, 0.5);
    double value2 = noise.octaveNoise(x, y, z, 20, 0.5);

    // With high octaves and low persistence, additional octaves should have
    // diminishing effect
    EXPECT_NEAR(value1, value2, 0.01);
}

// Test noise map generation
TEST_F(PerlinNoiseTest, NoiseMapGeneration) {
    PerlinNoise noise(42);

    // Generate a small noise map
    int width = 64, height = 64;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5;
    double lacunarity = 2.0;

    auto noiseMap = noise.generateNoiseMap(width, height, scale, octaves,
                                           persistence, lacunarity);

    // Check dimensions
    EXPECT_EQ(noiseMap.size(), height);
    EXPECT_EQ(noiseMap[0].size(), width);

    // Check values are in range [0,1]
    checkNoiseRange(noiseMap);

    // Calculate statistics to verify noise properties
    double mean = calculateAverage(noiseMap);
    double stdDev = calculateStandardDeviation(noiseMap);

    // Good noise should have mean around 0.5 and reasonable variance
    EXPECT_NEAR(mean, 0.5, 0.1);
    EXPECT_GT(stdDev, 0.05);  // Should have some variance
    EXPECT_LT(stdDev, 0.4);   // But not too extreme

    // Optional: Save noise map for visual inspection (disabled in automated
    // tests) saveNoiseMapAsPPM(noiseMap, "noise_map_test.ppm");
}

TEST_F(PerlinNoiseTest, NoiseMapScaleEffect) {
    PerlinNoise noise(42);

    int width = 64, height = 64;
    int octaves = 4;
    double persistence = 0.5;
    double lacunarity = 2.0;

    // Generate noise maps with different scales
    auto smallScale = noise.generateNoiseMap(width, height, 10.0, octaves,
                                             persistence, lacunarity);
    auto largeScale = noise.generateNoiseMap(width, height, 100.0, octaves,
                                             persistence, lacunarity);

    // Calculate standard deviation for each
    double smallScaleStdDev = calculateStandardDeviation(smallScale);
    double largeScaleStdDev = calculateStandardDeviation(largeScale);

    // Larger scale should result in smoother noise (lower standard deviation)
    EXPECT_GT(smallScaleStdDev, largeScaleStdDev);
}

TEST_F(PerlinNoiseTest, NoiseMapOctaveEffect) {
    PerlinNoise noise(42);

    int width = 64, height = 64;
    double scale = 25.0;
    double persistence = 0.5;
    double lacunarity = 2.0;

    // Generate noise maps with different octave counts
    auto lowOctaves = noise.generateNoiseMap(width, height, scale, 1,
                                             persistence, lacunarity);
    auto highOctaves = noise.generateNoiseMap(width, height, scale, 8,
                                              persistence, lacunarity);

    // More octaves should typically result in more detail (higher standard
    // deviation)
    double lowOctavesStdDev = calculateStandardDeviation(lowOctaves);
    double highOctavesStdDev = calculateStandardDeviation(highOctaves);

    // This relationship can be complex, but generally more octaves means more
    // detail
    EXPECT_NE(lowOctavesStdDev, highOctavesStdDev);
}

TEST_F(PerlinNoiseTest, NoiseMapSeedEffect) {
    int width = 64, height = 64;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5;
    double lacunarity = 2.0;

    // Generate noise maps with different seeds
    PerlinNoise noise1(42);
    PerlinNoise noise2(123);

    auto noiseMap1 = noise1.generateNoiseMap(width, height, scale, octaves,
                                             persistence, lacunarity);
    auto noiseMap2 = noise2.generateNoiseMap(width, height, scale, octaves,
                                             persistence, lacunarity);

    // Different seeds should produce different noise patterns
    bool foundDifference = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (noiseMap1[y][x] != noiseMap2[y][x]) {
                foundDifference = true;
                break;
            }
        }
        if (foundDifference)
            break;
    }

    EXPECT_TRUE(foundDifference)
        << "Different seeds should produce different noise patterns";
}

// Test continuity properties of the noise function
TEST_F(PerlinNoiseTest, NoiseContinuity) {
    PerlinNoise noise(42);

    // Test continuity by sampling at very close points
    double x = 1.5, y = 2.5, z = 3.5;
    double epsilon = 1e-5;

    double value = noise.noise(x, y, z);
    double valueXDelta = noise.noise(x + epsilon, y, z);
    double valueYDelta = noise.noise(x, y + epsilon, z);
    double valueZDelta = noise.noise(x, y, z + epsilon);

    // Close points should have similar values (continuity property)
    EXPECT_NEAR(value, valueXDelta, 1e-3);
    EXPECT_NEAR(value, valueYDelta, 1e-3);
    EXPECT_NEAR(value, valueZDelta, 1e-3);
}

// Test consistent behavior at integer boundaries
TEST_F(PerlinNoiseTest, IntegerBoundaryConsistency) {
    PerlinNoise noise(42);

    // Check behavior around integer boundaries
    double nearInt = 2.999999;
    double atInt = 3.0;
    double afterInt = 3.000001;

    double valueNear = noise.noise(nearInt, 0.0, 0.0);
    double valueAt = noise.noise(atInt, 0.0, 0.0);
    double valueAfter = noise.noise(afterInt, 0.0, 0.0);

    // Values should be continuous across integer boundaries, not exactly equal
    EXPECT_NE(valueNear, valueAt);
    EXPECT_NE(valueAt, valueAfter);

    // But they should be reasonably close due to smoothing
    EXPECT_NEAR(valueNear, valueAt, 0.1);
    EXPECT_NEAR(valueAt, valueAfter, 0.1);
}

// Test performance
TEST_F(PerlinNoiseTest, PerformanceSinglePoint) {
    PerlinNoise noise(42);

    auto start = std::chrono::high_resolution_clock::now();

    constexpr int iterations = 100000;
    double sum = 0.0;

    for (int i = 0; i < iterations; ++i) {
        // Use different coordinates to prevent optimization
        double value = noise.noise(i * 0.01, i * 0.02, i * 0.03);
        sum += value;  // Prevent compiler from optimizing away the call
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Output for informational purposes
    std::cout << "Generated " << iterations << " noise values in " << duration
              << "ms" << std::endl;
    std::cout << "Average time per value: "
              << static_cast<double>(duration) / iterations << "ms"
              << std::endl;
    std::cout << "Checksum (to prevent optimization): " << sum << std::endl;

    // No actual test assertion here, just performance measurement
}

TEST_F(PerlinNoiseTest, PerformanceNoiseMap) {
    PerlinNoise noise(42);

    int width = 256, height = 256;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5;
    double lacunarity = 2.0;

    auto start = std::chrono::high_resolution_clock::now();

    auto noiseMap = noise.generateNoiseMap(width, height, scale, octaves,
                                           persistence, lacunarity);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Generated " << width << "x" << height << " noise map in "
              << duration << "ms" << std::endl;

    // Verify the noise map was actually created
    EXPECT_EQ(noiseMap.size(), height);
    EXPECT_EQ(noiseMap[0].size(), width);

    // No specific performance requirement, just informational
}

// Test thread safety
TEST_F(PerlinNoiseTest, ThreadSafety) {
    PerlinNoise noise(42);

    // Generate noise from multiple threads
    constexpr int numThreads = 8;
    constexpr int iterationsPerThread = 1000;

    std::vector<std::future<std::vector<double>>> futures;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&noise, t]() {
            std::vector<double> results;
            results.reserve(iterationsPerThread);

            for (int i = 0; i < iterationsPerThread; ++i) {
                // Use thread ID and iteration for unique coordinates
                double x = t * 0.1 + i * 0.001;
                double y = t * 0.2 + i * 0.002;
                double z = t * 0.3 + i * 0.003;

                results.push_back(noise.noise(x, y, z));
            }

            return results;
        }));
    }

    // Collect and verify results
    std::vector<std::vector<double>> allResults;
    for (auto& future : futures) {
        auto results = future.get();
        EXPECT_EQ(results.size(), iterationsPerThread);

        // Check values are in range
        for (auto value : results) {
            EXPECT_GE(value, 0.0);
            EXPECT_LE(value, 1.0);
        }

        allResults.push_back(std::move(results));
    }

    // Additional verification: for the same input, we should get the same
    // output regardless of which thread calculated it
    PerlinNoise verifyNoise(42);  // Same seed
    double x = 1.5, y = 2.5, z = 3.5;
    double expectedValue = verifyNoise.noise(x, y, z);

    std::vector<std::future<double>> verifyFutures;
    for (int t = 0; t < numThreads; ++t) {
        verifyFutures.push_back(std::async(
            std::launch::async,
            [&verifyNoise, x, y, z]() { return verifyNoise.noise(x, y, z); }));
    }

    for (auto& future : verifyFutures) {
        double result = future.get();
        EXPECT_DOUBLE_EQ(result, expectedValue);
    }
}

#ifdef USE_OPENCL
// OpenCL specific tests
TEST_F(PerlinNoiseTest, OpenCLSupport) {
    try {
        PerlinNoise noise(42);

        // Sample some points
        double x = 1.5, y = 2.5, z = 3.5;
        double valueDefault = noise.noise(x, y, z);

        // Since we can't directly test if OpenCL is being used,
        // we at least verify that calling the function doesn't throw exceptions
        EXPECT_NO_THROW(noise.noise(2.5, 3.5, 4.5));

        // Output for manual verification
        std::cout << "OpenCL may be available. Sample noise value: "
                  << valueDefault << std::endl;
    } catch (const std::exception& e) {
        // If OpenCL initialization fails, this test will be skipped
        GTEST_SKIP() << "OpenCL not available: " << e.what();
    }
}

TEST_F(PerlinNoiseTest, OpenCLPerformance) {
    try {
        PerlinNoise noise(42);

        constexpr int iterations = 10000;

        // Measure performance
        auto start = std::chrono::high_resolution_clock::now();

        double sum = 0.0;
        for (int i = 0; i < iterations; ++i) {
            double value = noise.noise(i * 0.01, i * 0.02, i * 0.03);
            sum += value;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();

        std::cout << "Generated " << iterations << " OpenCL noise values in "
                  << duration
                  << "ms (avg: " << static_cast<double>(duration) / iterations
                  << "ms per value)" << std::endl;

        // No specific performance requirement
    } catch (const std::exception& e) {
        GTEST_SKIP() << "OpenCL not available: " << e.what();
    }
}
#endif

// Test consistency with different data types
TEST_F(PerlinNoiseTest, DataTypeConsistency) {
    PerlinNoise noise(42);

    // Test with various floating-point types
    float xf = 1.5f, yf = 2.5f, zf = 3.5f;
    double xd = 1.5, yd = 2.5, zd = 3.5;

    float floatResult = noise.noise(xf, yf, zf);
    double doubleResult = noise.noise(xd, yd, zd);

    // Should be very close
    EXPECT_NEAR(floatResult, doubleResult, 1e-6);

    // Test with integer inputs (explicitly converted to floating point)
    int xi = 1, yi = 2, zi = 3;
    double intResult =
        noise.noise(static_cast<double>(xi), static_cast<double>(yi),
                    static_cast<double>(zi));

    // Integer inputs should work fine and be in range
    EXPECT_GE(intResult, 0.0);
    EXPECT_LE(intResult, 1.0);
}

// Test with extreme parameter values
TEST_F(PerlinNoiseTest, ExtremeParameters) {
    PerlinNoise noise(42);

    // Test with very large coordinates
    double largeX = 1e6, largeY = 1e6, largeZ = 1e6;
    double largeResult = noise.noise(largeX, largeY, largeZ);

    EXPECT_GE(largeResult, 0.0);
    EXPECT_LE(largeResult, 1.0);

    // Test with very small coordinates
    double smallX = 1e-6, smallY = 1e-6, smallZ = 1e-6;
    double smallResult = noise.noise(smallX, smallY, smallZ);

    EXPECT_GE(smallResult, 0.0);
    EXPECT_LE(smallResult, 1.0);

    // Test octave noise with extreme parameters
    int highOctaves = 20;
    double lowPersistence = 0.01;

    double extremeResult =
        noise.octaveNoise(1.5, 2.5, 3.5, highOctaves, lowPersistence);

    EXPECT_GE(extremeResult, 0.0);
    EXPECT_LE(extremeResult, 1.0);

    // Test noise map with extreme parameters
    auto extremeNoiseMap = noise.generateNoiseMap(32, 32, 0.1, 10, 0.1, 4.0);
    checkNoiseRange(extremeNoiseMap);
}

// Test for spatial frequency properties
TEST_F(PerlinNoiseTest, SpatialFrequency) {
    PerlinNoise noise(42);

    int width = 128, height = 1;  // 1D slice for easier analysis
    std::vector<std::vector<double>> lowFreq =
        noise.generateNoiseMap(width, height, 100.0, 1, 0.5, 2.0);
    std::vector<std::vector<double>> highFreq =
        noise.generateNoiseMap(width, height, 10.0, 1, 0.5, 2.0);

    // Count zero crossings as a simple frequency measure
    int lowFreqCrossings = 0;
    int highFreqCrossings = 0;

    for (int x = 1; x < width; ++x) {
        if ((lowFreq[0][x] - 0.5) * (lowFreq[0][x - 1] - 0.5) <= 0) {
            lowFreqCrossings++;
        }
        if ((highFreq[0][x] - 0.5) * (highFreq[0][x - 1] - 0.5) <= 0) {
            highFreqCrossings++;
        }
    }

    // Higher frequency (smaller scale) should have more zero crossings
    EXPECT_GT(highFreqCrossings, lowFreqCrossings);
}

// Test reproducibility with fixed seeds
TEST_F(PerlinNoiseTest, Reproducibility) {
    // Create noise generators with different seeds
    PerlinNoise noise1(123);
    PerlinNoise noise2(456);
    PerlinNoise noise3(123);  // Same seed as noise1

    std::vector<double> testPoints = {noise1.noise(1.5, 2.5, 3.5),
                                      noise1.noise(4.2, 5.7, 6.1),
                                      noise1.noise(-1.2, -3.4, 2.8)};

    std::vector<double> differentSeedPoints = {noise2.noise(1.5, 2.5, 3.5),
                                               noise2.noise(4.2, 5.7, 6.1),
                                               noise2.noise(-1.2, -3.4, 2.8)};

    std::vector<double> sameSeedPoints = {noise3.noise(1.5, 2.5, 3.5),
                                          noise3.noise(4.2, 5.7, 6.1),
                                          noise3.noise(-1.2, -3.4, 2.8)};

    // Different seeds should produce different values
    bool allDifferent = false;
    for (size_t i = 0; i < testPoints.size(); ++i) {
        if (std::abs(testPoints[i] - differentSeedPoints[i]) > 1e-10) {
            allDifferent = true;
            break;
        }
    }

    EXPECT_TRUE(allDifferent)
        << "Different seeds should produce different noise values";

    // Same seed should reproduce exact same values
    for (size_t i = 0; i < testPoints.size(); ++i) {
        EXPECT_DOUBLE_EQ(testPoints[i], sameSeedPoints[i]);
    }
}
