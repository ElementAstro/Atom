#include "atom/utils/lcg.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Print statistics of generated values
 */
template <typename T>
void printStatistics(const std::vector<T>& values, const std::string& title) {
    if (values.empty()) {
        spdlog::info("No data to analyze for {}", title);
        return;
    }
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    double variance = 0.0;
    for (const auto& value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= values.size();
    double stdDev = std::sqrt(variance);
    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    spdlog::info("=== {} Statistics ===", title);
    spdlog::info("Count: {}", values.size());
    spdlog::info("Min: {}", *minIt);
    spdlog::info("Max: {}", *maxIt);
    spdlog::info("Mean: {}", mean);
    spdlog::info("Standard Deviation: {}", stdDev);
    spdlog::info("=======================\n");
}

/**
 * @brief Demonstrate multithreaded random generation
 */
void runInMultipleThreads(atom::utils::LCG& lcg, int threadCount) {
    spdlog::info("\n=== Multithreading Example ===");
    std::vector<std::thread> threads;
    std::vector<std::vector<double>> threadResults(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&lcg, i, &threadResults]() {
            std::vector<double> results;
            results.reserve(1000);
            for (int j = 0; j < 1000; ++j) {
                results.push_back(lcg.nextDouble());
            }
            threadResults[i] = std::move(results);
            spdlog::info("Thread {} completed generating 1000 random numbers",
                         i);
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    for (int i = 0; i < threadCount; ++i) {
        printStatistics(threadResults[i],
                        "Thread " + std::to_string(i) + " Results");
    }
    spdlog::info("All threads completed successfully");
}

/**
 * @brief Test statistical distributions
 */
void testDistributions(atom::utils::LCG& lcg) {
    spdlog::info("\n=== Statistical Distributions Examples ===");
    const int sampleSize = 10000;

    {
        std::vector<double> gaussianSamples;
        gaussianSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            gaussianSamples.push_back(lcg.nextGaussian(10.0, 2.0));
        }
        printStatistics(gaussianSamples,
                        "Gaussian Distribution (mean=10, stddev=2)");
    }
    {
        std::vector<double> expSamples;
        expSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            expSamples.push_back(lcg.nextExponential(0.5));
        }
        printStatistics(expSamples, "Exponential Distribution (lambda=0.5)");
    }
    {
        std::vector<double> poissonSamples;
        poissonSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            poissonSamples.push_back(lcg.nextPoisson(5.0));
        }
        printStatistics(poissonSamples, "Poisson Distribution (lambda=5)");
    }
    {
        std::vector<double> betaSamples;
        betaSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            betaSamples.push_back(lcg.nextBeta(2.0, 5.0));
        }
        printStatistics(betaSamples, "Beta Distribution (alpha=2, beta=5)");
    }
    {
        std::vector<double> gammaSamples;
        gammaSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            gammaSamples.push_back(lcg.nextGamma(2.0, 1.5));
        }
        printStatistics(gammaSamples,
                        "Gamma Distribution (shape=2, scale=1.5)");
    }
}

/**
 * @brief Demonstrate discrete distributions
 */
void testDiscreteDistributions(atom::utils::LCG& lcg) {
    spdlog::info("\n=== Discrete Distributions Examples ===");
    const int sampleSize = 10000;

    {
        int trueCount = 0;
        for (int i = 0; i < sampleSize; ++i) {
            if (lcg.nextBernoulli(0.7)) {
                trueCount++;
            }
        }
        double observedProbability =
            static_cast<double>(trueCount) / sampleSize;
        spdlog::info("Bernoulli Distribution (p=0.7):");
        spdlog::info("True count: {} out of {}", trueCount, sampleSize);
        spdlog::info("Observed probability: {}\n", observedProbability);
    }
    {
        std::vector<double> geometricSamples;
        geometricSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            geometricSamples.push_back(lcg.nextGeometric(0.3));
        }
        printStatistics(geometricSamples, "Geometric Distribution (p=0.3)");
    }
    {
        std::vector<double> chiSquaredSamples;
        chiSquaredSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            chiSquaredSamples.push_back(lcg.nextChiSquared(4.0));
        }
        printStatistics(chiSquaredSamples, "Chi-Squared Distribution (df=4)");
    }
    {
        std::vector<double> hypergeometricSamples;
        hypergeometricSamples.reserve(sampleSize);
        for (int i = 0; i < sampleSize; ++i) {
            hypergeometricSamples.push_back(lcg.nextHypergeometric(50, 20, 10));
        }
        printStatistics(hypergeometricSamples,
                        "Hypergeometric Distribution (N=50, K=20, n=10)");
    }
    {
        std::vector<double> weights = {10.0, 20.0, 5.0, 15.0, 25.0};
        std::map<int, int> discreteResults;
        for (int i = 0; i < sampleSize; ++i) {
            int outcome = lcg.nextDiscrete(weights);
            discreteResults[outcome]++;
        }
        spdlog::info("Discrete Distribution with weights [10, 20, 5, 15, 25]:");
        for (const auto& [outcome, count] : discreteResults) {
            double percentage = 100.0 * count / sampleSize;
            spdlog::info("Outcome {}: {} times ({:.2f}%)", outcome, count,
                         percentage);
        }
        spdlog::info("");
    }
    {
        std::vector<double> probabilities = {0.2, 0.5, 0.3};
        auto multinomialResult = lcg.nextMultinomial(1000, probabilities);
        spdlog::info("Multinomial Distribution (n=1000, p=[0.2, 0.5, 0.3]):");
        for (size_t i = 0; i < multinomialResult.size(); ++i) {
            double percentage = 100.0 * multinomialResult[i] / 1000;
            spdlog::info("Category {}: {} occurrences ({:.2f}%)", i,
                         multinomialResult[i], percentage);
        }
        spdlog::info("");
    }
}

/**
 * @brief Demonstrate array/collection operations
 */
void testCollectionOperations(atom::utils::LCG& lcg) {
    spdlog::info("\n=== Collection Operations Examples ===");
    {
        std::vector<int> numbers(10);
        std::iota(numbers.begin(), numbers.end(), 1);
        lcg.shuffle(numbers);
    }
    {
        std::vector<std::string> items = {
            "apple", "banana", "cherry",   "date", "elderberry",
            "fig",   "grape",  "honeydew", "kiwi", "lemon"};
        auto samples = lcg.sample(items, 5);
    }
}

/**
 * @brief Demonstrate state saving and loading
 */
void testStateSaving(atom::utils::LCG& lcg) {
    spdlog::info("\n=== State Saving/Loading Example ===");
    std::vector<double> originalSequence;
    for (int i = 0; i < 5; ++i) {
        double value = lcg.nextDouble();
        originalSequence.push_back(value);
        spdlog::info("Original value {}: {}", i, value);
    }
    const std::string stateFile = "lcg_state.bin";
    lcg.saveState(stateFile);
    spdlog::info("LCG state saved to {}", stateFile);
    for (int i = 0; i < 5; ++i) {
        spdlog::info("Diverged value {}: {}", i, lcg.nextDouble());
    }
    lcg.loadState(stateFile);
    spdlog::info("LCG state loaded from {}", stateFile);
    std::vector<double> restoredSequence;
    for (int i = 0; i < 5; ++i) {
        double value = lcg.nextDouble();
        restoredSequence.push_back(value);
        spdlog::info("Restored value {}: {}", i, value);
    }
    bool sequencesMatch =
        std::equal(originalSequence.begin(), originalSequence.end(),
                   restoredSequence.begin());
    spdlog::info("Sequences match: {}", sequencesMatch ? "Yes" : "No");
    std::remove(stateFile.c_str());
}

/**
 * @brief Main function with LCG usage examples
 */
int main() {
    spdlog::info("===============================================");
    spdlog::info("LCG (Linear Congruential Generator) Usage Examples");
    spdlog::info("===============================================");

    atom::utils::LCG lcg;
    spdlog::info("Created LCG with default seed (time-based)");
    const uint32_t specificSeed = 12345;
    atom::utils::LCG lcgWithSeed(specificSeed);
    spdlog::info("Created LCG with specific seed: {}", specificSeed);

    spdlog::info("=== Basic Random Number Generation ===");
    spdlog::info("Raw random number: {}", lcg.next());
    spdlog::info("Random int (0-100): {}", lcg.nextInt(0, 100));
    spdlog::info("Random double (0-1): {}", lcg.nextDouble());
    spdlog::info("Random double (5-10): {}", lcg.nextDouble(5.0, 10.0));
    spdlog::info("Random boolean (50% probability): {}",
                 lcg.nextBernoulli() ? "true" : "false");
    spdlog::info("Random boolean (80% probability): {}",
                 lcg.nextBernoulli(0.8) ? "true" : "false");

    testDistributions(lcg);
    testDiscreteDistributions(lcg);
    testCollectionOperations(lcg);
    testStateSaving(lcgWithSeed);
    runInMultipleThreads(lcg, 4);

    spdlog::info("\n=== Error Handling Examples ===");
    try {
        lcg.nextInt(100, 50);
    } catch (const std::exception& e) {
        spdlog::info("Expected error caught: {}", e.what());
    }
    try {
        lcg.nextBernoulli(2.0);
    } catch (const std::exception& e) {
        spdlog::info("Expected error caught: {}", e.what());
    }
    try {
        lcg.nextGamma(-1.0, 1.0);
    } catch (const std::exception& e) {
        spdlog::info("Expected error caught: {}", e.what());
    }
    spdlog::info("\nAll LCG examples completed successfully!");
    return 0;
}