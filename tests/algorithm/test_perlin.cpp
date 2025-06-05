#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <string>
#include <vector>
#include "atom/algorithm/perlin.hpp"
#include "spdlog/spdlog.h"

using namespace atom::algorithm;
using namespace std::chrono_literals;

class PerlinNoiseTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    void checkNoiseRange(const std::vector<std::vector<double>>& noiseMap) {
        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                EXPECT_GE(value, 0.0);
                EXPECT_LE(value, 1.0);
            }
        }
    }

    double calculateStandardDeviation(
        const std::vector<std::vector<double>>& noiseMap) {
        double sum = 0.0, sumSquared = 0.0;
        size_t count = 0;
        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                sum += value;
                sumSquared += value * value;
                ++count;
            }
        }
        double mean = sum / count;
        double variance = (sumSquared / count) - (mean * mean);
        return std::sqrt(variance);
    }

    double calculateAverage(const std::vector<std::vector<double>>& noiseMap) {
        double sum = 0.0;
        size_t count = 0;
        for (const auto& row : noiseMap) {
            for (const auto& value : row) {
                sum += value;
                ++count;
            }
        }
        return sum / count;
    }
};

TEST_F(PerlinNoiseTest, NoiseInRange) {
    PerlinNoise noise(42);
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
    float xf = 1.5f, yf = 2.5f, zf = 3.5f;
    float noiseValueFloat = noise.noise(xf, yf, zf);
    double xd = 1.5, yd = 2.5, zd = 3.5;
    double noiseValueDouble = noise.noise(xd, yd, zd);
    EXPECT_NEAR(noiseValueFloat, noiseValueDouble, 1e-6);
    EXPECT_GE(noiseValueFloat, 0.0f);
    EXPECT_LE(noiseValueFloat, 1.0f);
    EXPECT_GE(noiseValueDouble, 0.0);
    EXPECT_LE(noiseValueDouble, 1.0);
}

TEST_F(PerlinNoiseTest, DeterministicOutput) {
    PerlinNoise noise1(123), noise2(123);
    for (double x = -5.0; x <= 5.0; x += 0.5) {
        for (double y = -5.0; y <= 5.0; y += 0.5) {
            for (double z = -5.0; z <= 5.0; z += 0.5) {
                EXPECT_DOUBLE_EQ(noise1.noise(x, y, z), noise2.noise(x, y, z));
            }
        }
    }
    PerlinNoise noise3(456);
    bool foundDifference = false;
    for (double x = -5.0; x <= 5.0 && !foundDifference; x += 0.5) {
        for (double y = -5.0; y <= 5.0 && !foundDifference; y += 0.5) {
            for (double z = -5.0; z <= 5.0; z += 0.5) {
                if (noise1.noise(x, y, z) != noise3.noise(x, y, z)) {
                    foundDifference = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(foundDifference)
        << "Different seeds should produce different noise patterns";
}

TEST_F(PerlinNoiseTest, OctaveNoise) {
    PerlinNoise noise(42);
    double x = 1.5, y = 2.5, z = 3.5;
    EXPECT_DOUBLE_EQ(noise.noise(x, y, z), noise.octaveNoise(x, y, z, 1, 0.5));
    double value = noise.octaveNoise(x, y, z, 5, 0.5);
    EXPECT_GE(value, 0.0);
    EXPECT_LE(value, 1.0);
    double valueLowPersistence = noise.octaveNoise(x, y, z, 5, 0.1);
    double valueHighPersistence = noise.octaveNoise(x, y, z, 5, 0.9);
    EXPECT_NE(valueLowPersistence, valueHighPersistence);
}

TEST_F(PerlinNoiseTest, OctaveNoiseConvergence) {
    PerlinNoise noise(42);
    double x = 1.5, y = 2.5, z = 3.5;
    double value1 = noise.octaveNoise(x, y, z, 10, 0.5);
    double value2 = noise.octaveNoise(x, y, z, 20, 0.5);
    EXPECT_NEAR(value1, value2, 0.01);
}

TEST_F(PerlinNoiseTest, NoiseMapGeneration) {
    PerlinNoise noise(42);
    int width = 64, height = 64;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5, lacunarity = 2.0;
    auto noiseMap = noise.generateNoiseMap(width, height, scale, octaves,
                                           persistence, lacunarity);
    EXPECT_EQ(noiseMap.size(), height);
    EXPECT_EQ(noiseMap[0].size(), width);
    checkNoiseRange(noiseMap);
    double mean = calculateAverage(noiseMap);
    double stdDev = calculateStandardDeviation(noiseMap);
    EXPECT_NEAR(mean, 0.5, 0.1);
    EXPECT_GT(stdDev, 0.05);
    EXPECT_LT(stdDev, 0.4);
}

TEST_F(PerlinNoiseTest, NoiseMapScaleEffect) {
    PerlinNoise noise(42);
    int width = 64, height = 64, octaves = 4;
    double persistence = 0.5, lacunarity = 2.0;
    auto smallScale = noise.generateNoiseMap(width, height, 10.0, octaves,
                                             persistence, lacunarity);
    auto largeScale = noise.generateNoiseMap(width, height, 100.0, octaves,
                                             persistence, lacunarity);
    double smallScaleStdDev = calculateStandardDeviation(smallScale);
    double largeScaleStdDev = calculateStandardDeviation(largeScale);
    EXPECT_GT(smallScaleStdDev, largeScaleStdDev);
}

TEST_F(PerlinNoiseTest, NoiseMapOctaveEffect) {
    PerlinNoise noise(42);
    int width = 64, height = 64;
    double scale = 25.0, persistence = 0.5, lacunarity = 2.0;
    auto lowOctaves = noise.generateNoiseMap(width, height, scale, 1,
                                             persistence, lacunarity);
    auto highOctaves = noise.generateNoiseMap(width, height, scale, 8,
                                              persistence, lacunarity);
    double lowOctavesStdDev = calculateStandardDeviation(lowOctaves);
    double highOctavesStdDev = calculateStandardDeviation(highOctaves);
    EXPECT_NE(lowOctavesStdDev, highOctavesStdDev);
}

TEST_F(PerlinNoiseTest, NoiseMapSeedEffect) {
    int width = 64, height = 64;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5, lacunarity = 2.0;
    PerlinNoise noise1(42), noise2(123);
    auto noiseMap1 = noise1.generateNoiseMap(width, height, scale, octaves,
                                             persistence, lacunarity);
    auto noiseMap2 = noise2.generateNoiseMap(width, height, scale, octaves,
                                             persistence, lacunarity);
    bool foundDifference = false;
    for (int y = 0; y < height && !foundDifference; ++y) {
        for (int x = 0; x < width; ++x) {
            if (noiseMap1[y][x] != noiseMap2[y][x]) {
                foundDifference = true;
                break;
            }
        }
    }
    EXPECT_TRUE(foundDifference)
        << "Different seeds should produce different noise patterns";
}

TEST_F(PerlinNoiseTest, NoiseContinuity) {
    PerlinNoise noise(42);
    double x = 1.5, y = 2.5, z = 3.5, epsilon = 1e-5;
    double value = noise.noise(x, y, z);
    double valueXDelta = noise.noise(x + epsilon, y, z);
    double valueYDelta = noise.noise(x, y + epsilon, z);
    double valueZDelta = noise.noise(x, y, z + epsilon);
    EXPECT_NEAR(value, valueXDelta, 1e-3);
    EXPECT_NEAR(value, valueYDelta, 1e-3);
    EXPECT_NEAR(value, valueZDelta, 1e-3);
}

TEST_F(PerlinNoiseTest, IntegerBoundaryConsistency) {
    PerlinNoise noise(42);
    double nearInt = 2.999999, atInt = 3.0, afterInt = 3.000001;
    double valueNear = noise.noise(nearInt, 0.0, 0.0);
    double valueAt = noise.noise(atInt, 0.0, 0.0);
    double valueAfter = noise.noise(afterInt, 0.0, 0.0);
    EXPECT_NE(valueNear, valueAt);
    EXPECT_NE(valueAt, valueAfter);
    EXPECT_NEAR(valueNear, valueAt, 0.1);
    EXPECT_NEAR(valueAt, valueAfter, 0.1);
}

TEST_F(PerlinNoiseTest, PerformanceSinglePoint) {
    PerlinNoise noise(42);
    auto start = std::chrono::high_resolution_clock::now();
    constexpr int iterations = 100000;
    double sum = 0.0;
    for (int i = 0; i < iterations; ++i) {
        double value = noise.noise(i * 0.01, i * 0.02, i * 0.03);
        sum += value;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Generated {} noise values in {}ms", iterations, duration);
    spdlog::info("Average time per value: {}ms",
                 static_cast<double>(duration) / iterations);
    spdlog::info("Checksum (to prevent optimization): {}", sum);
}

TEST_F(PerlinNoiseTest, PerformanceNoiseMap) {
    PerlinNoise noise(42);
    int width = 256, height = 256;
    double scale = 25.0;
    int octaves = 4;
    double persistence = 0.5, lacunarity = 2.0;
    auto start = std::chrono::high_resolution_clock::now();
    auto noiseMap = noise.generateNoiseMap(width, height, scale, octaves,
                                           persistence, lacunarity);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Generated {}x{} noise map in {}ms", width, height, duration);
    EXPECT_EQ(noiseMap.size(), height);
    EXPECT_EQ(noiseMap[0].size(), width);
}

TEST_F(PerlinNoiseTest, ThreadSafety) {
    PerlinNoise noise(42);
    constexpr int numThreads = 8, iterationsPerThread = 1000;
    std::vector<std::future<std::vector<double>>> futures;
    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&noise, t]() {
            std::vector<double> results;
            results.reserve(iterationsPerThread);
            for (int i = 0; i < iterationsPerThread; ++i) {
                double x = t * 0.1 + i * 0.001;
                double y = t * 0.2 + i * 0.002;
                double z = t * 0.3 + i * 0.003;
                results.push_back(noise.noise(x, y, z));
            }
            return results;
        }));
    }
    for (auto& future : futures) {
        auto results = future.get();
        EXPECT_EQ(results.size(), iterationsPerThread);
        for (auto value : results) {
            EXPECT_GE(value, 0.0);
            EXPECT_LE(value, 1.0);
        }
    }
    PerlinNoise verifyNoise(42);
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
TEST_F(PerlinNoiseTest, OpenCLSupport) {
    try {
        PerlinNoise noise(42);
        double x = 1.5, y = 2.5, z = 3.5;
        double valueDefault = noise.noise(x, y, z);
        EXPECT_NO_THROW(noise.noise(2.5, 3.5, 4.5));
        spdlog::info("OpenCL may be available. Sample noise value: {}",
                     valueDefault);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "OpenCL not available: " << e.what();
    }
}

TEST_F(PerlinNoiseTest, OpenCLPerformance) {
    try {
        PerlinNoise noise(42);
        constexpr int iterations = 10000;
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
        spdlog::info(
            "Generated {} OpenCL noise values in {}ms (avg: {}ms per value)",
            iterations, duration, static_cast<double>(duration) / iterations);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "OpenCL not available: " << e.what();
    }
}
#endif

TEST_F(PerlinNoiseTest, DataTypeConsistency) {
    PerlinNoise noise(42);
    float xf = 1.5f, yf = 2.5f, zf = 3.5f;
    double xd = 1.5, yd = 2.5, zd = 3.5;
    float floatResult = noise.noise(xf, yf, zf);
    double doubleResult = noise.noise(xd, yd, zd);
    EXPECT_NEAR(floatResult, doubleResult, 1e-6);
    int xi = 1, yi = 2, zi = 3;
    double intResult =
        noise.noise(static_cast<double>(xi), static_cast<double>(yi),
                    static_cast<double>(zi));
    EXPECT_GE(intResult, 0.0);
    EXPECT_LE(intResult, 1.0);
}

TEST_F(PerlinNoiseTest, ExtremeParameters) {
    PerlinNoise noise(42);
    double largeX = 1e6, largeY = 1e6, largeZ = 1e6;
    double largeResult = noise.noise(largeX, largeY, largeZ);
    EXPECT_GE(largeResult, 0.0);
    EXPECT_LE(largeResult, 1.0);
    double smallX = 1e-6, smallY = 1e-6, smallZ = 1e-6;
    double smallResult = noise.noise(smallX, smallY, smallZ);
    EXPECT_GE(smallResult, 0.0);
    EXPECT_LE(smallResult, 1.0);
    int highOctaves = 20;
    double lowPersistence = 0.01;
    double extremeResult =
        noise.octaveNoise(1.5, 2.5, 3.5, highOctaves, lowPersistence);
    EXPECT_GE(extremeResult, 0.0);
    EXPECT_LE(extremeResult, 1.0);
    auto extremeNoiseMap = noise.generateNoiseMap(32, 32, 0.1, 10, 0.1, 4.0);
    checkNoiseRange(extremeNoiseMap);
}

TEST_F(PerlinNoiseTest, SpatialFrequency) {
    PerlinNoise noise(42);
    int width = 128, height = 1;
    std::vector<std::vector<double>> lowFreq =
        noise.generateNoiseMap(width, height, 100.0, 1, 0.5, 2.0);
    std::vector<std::vector<double>> highFreq =
        noise.generateNoiseMap(width, height, 10.0, 1, 0.5, 2.0);
    int lowFreqCrossings = 0, highFreqCrossings = 0;
    for (int x = 1; x < width; ++x) {
        if ((lowFreq[0][x] - 0.5) * (lowFreq[0][x - 1] - 0.5) <= 0)
            lowFreqCrossings++;
        if ((highFreq[0][x] - 0.5) * (highFreq[0][x - 1] - 0.5) <= 0)
            highFreqCrossings++;
    }
    EXPECT_GT(highFreqCrossings, lowFreqCrossings);
}

TEST_F(PerlinNoiseTest, Reproducibility) {
    PerlinNoise noise1(123), noise2(456), noise3(123);
    std::vector<double> testPoints = {noise1.noise(1.5, 2.5, 3.5),
                                      noise1.noise(4.2, 5.7, 6.1),
                                      noise1.noise(-1.2, -3.4, 2.8)};
    std::vector<double> differentSeedPoints = {noise2.noise(1.5, 2.5, 3.5),
                                               noise2.noise(4.2, 5.7, 6.1),
                                               noise2.noise(-1.2, -3.4, 2.8)};
    std::vector<double> sameSeedPoints = {noise3.noise(1.5, 2.5, 3.5),
                                          noise3.noise(4.2, 5.7, 6.1),
                                          noise3.noise(-1.2, -3.4, 2.8)};
    bool allDifferent = false;
    for (size_t i = 0; i < testPoints.size(); ++i) {
        if (std::abs(testPoints[i] - differentSeedPoints[i]) > 1e-10) {
            allDifferent = true;
            break;
        }
    }
    EXPECT_TRUE(allDifferent)
        << "Different seeds should produce different noise values";
    for (size_t i = 0; i < testPoints.size(); ++i) {
        EXPECT_DOUBLE_EQ(testPoints[i], sameSeedPoints[i]);
    }
}
