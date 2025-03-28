#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/utils/lcg.hpp"

// Helper function to print statistics of generated values
template <typename T>
void printStatistics(const std::vector<T>& values, const std::string& title) {
    if (values.empty()) {
        std::cout << "No data to analyze for " << title << std::endl;
        return;
    }

    // Calculate mean
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    // Calculate variance and standard deviation
    double variance = 0.0;
    for (const auto& value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= values.size();
    double stdDev = std::sqrt(variance);

    // Calculate min and max
    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());

    // Output statistics
    std::cout << "=== " << title << " Statistics ===" << std::endl;
    std::cout << "Count: " << values.size() << std::endl;
    std::cout << "Min: " << *minIt << std::endl;
    std::cout << "Max: " << *maxIt << std::endl;
    std::cout << "Mean: " << mean << std::endl;
    std::cout << "Standard Deviation: " << stdDev << std::endl;
    std::cout << "=======================" << std::endl << std::endl;
}

// Helper function to demonstrate multithreaded random generation
void runInMultipleThreads(atom::utils::LCG& lcg, int threadCount) {
    std::cout << "\n=== Multithreading Example ===" << std::endl;

    std::vector<std::thread> threads;
    std::vector<std::vector<double>> threadResults(threadCount);

    // Create threads that generate random numbers
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&lcg, i, &threadResults]() {
            std::vector<double> results;
            for (int j = 0; j < 1000; ++j) {
                results.push_back(lcg.nextDouble());
            }
            threadResults[i] = results;
            std::cout << "Thread " << i << " completed generating "
                      << results.size() << " random numbers" << std::endl;
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Calculate statistics for each thread's results
    for (int i = 0; i < threadCount; ++i) {
        printStatistics(threadResults[i],
                        "Thread " + std::to_string(i) + " Results");
    }

    std::cout << "All threads completed successfully" << std::endl;
}

// Helper function to test statistical distributions
void testDistributions(atom::utils::LCG& lcg) {
    std::cout << "\n=== Statistical Distributions Examples ===" << std::endl;

    const int sampleSize = 10000;

    // Test Gaussian distribution
    {
        std::vector<double> gaussianSamples;
        for (int i = 0; i < sampleSize; ++i) {
            gaussianSamples.push_back(lcg.nextGaussian(10.0, 2.0));
        }
        printStatistics(gaussianSamples,
                        "Gaussian Distribution (mean=10, stddev=2)");
    }

    // Test Exponential distribution
    {
        std::vector<double> expSamples;
        for (int i = 0; i < sampleSize; ++i) {
            expSamples.push_back(lcg.nextExponential(0.5));
        }
        printStatistics(expSamples, "Exponential Distribution (lambda=0.5)");
    }

    // Test Poisson distribution
    {
        std::vector<double> poissonSamples;
        for (int i = 0; i < sampleSize; ++i) {
            poissonSamples.push_back(lcg.nextPoisson(5.0));
        }
        printStatistics(poissonSamples, "Poisson Distribution (lambda=5)");
    }

    // Test Beta distribution
    {
        std::vector<double> betaSamples;
        for (int i = 0; i < sampleSize; ++i) {
            betaSamples.push_back(lcg.nextBeta(2.0, 5.0));
        }
        printStatistics(betaSamples, "Beta Distribution (alpha=2, beta=5)");
    }

    // Test Gamma distribution
    {
        std::vector<double> gammaSamples;
        for (int i = 0; i < sampleSize; ++i) {
            gammaSamples.push_back(lcg.nextGamma(2.0, 1.5));
        }
        printStatistics(gammaSamples,
                        "Gamma Distribution (shape=2, scale=1.5)");
    }
}

// Helper function to demonstrate discrete distributions
void testDiscreteDistributions(atom::utils::LCG& lcg) {
    std::cout << "\n=== Discrete Distributions Examples ===" << std::endl;

    const int sampleSize = 10000;

    // Test Bernoulli distribution
    {
        int trueCount = 0;
        for (int i = 0; i < sampleSize; ++i) {
            if (lcg.nextBernoulli(0.7)) {
                trueCount++;
            }
        }
        double observedProbability =
            static_cast<double>(trueCount) / sampleSize;
        std::cout << "Bernoulli Distribution (p=0.7):" << std::endl;
        std::cout << "True count: " << trueCount << " out of " << sampleSize
                  << std::endl;
        std::cout << "Observed probability: " << observedProbability
                  << std::endl
                  << std::endl;
    }

    // Test Geometric distribution
    {
        std::vector<double> geometricSamples;
        for (int i = 0; i < sampleSize; ++i) {
            geometricSamples.push_back(lcg.nextGeometric(0.3));
        }
        printStatistics(geometricSamples, "Geometric Distribution (p=0.3)");
    }

    // Test Chi-Squared distribution
    {
        std::vector<double> chiSquaredSamples;
        for (int i = 0; i < sampleSize; ++i) {
            chiSquaredSamples.push_back(lcg.nextChiSquared(4.0));
        }
        printStatistics(chiSquaredSamples, "Chi-Squared Distribution (df=4)");
    }

    // Test Hypergeometric distribution
    {
        std::vector<double> hypergeometricSamples;
        for (int i = 0; i < sampleSize; ++i) {
            hypergeometricSamples.push_back(lcg.nextHypergeometric(50, 20, 10));
        }
        printStatistics(hypergeometricSamples,
                        "Hypergeometric Distribution (N=50, K=20, n=10)");
    }

    // Test Discrete distribution with weights
    {
        std::vector<double> weights = {10.0, 20.0, 5.0, 15.0, 25.0};
        std::map<int, int> discreteResults;

        for (int i = 0; i < sampleSize; ++i) {
            int outcome = lcg.nextDiscrete(weights);
            discreteResults[outcome]++;
        }

        std::cout << "Discrete Distribution with weights [10, 20, 5, 15, 25]:"
                  << std::endl;
        for (const auto& [outcome, count] : discreteResults) {
            double percentage = 100.0 * count / sampleSize;
            std::cout << "Outcome " << outcome << ": " << count << " times ("
                      << std::fixed << std::setprecision(2) << percentage
                      << "%)" << std::endl;
        }
        std::cout << std::endl;
    }

    // Test Multinomial distribution
    {
        std::vector<double> probabilities = {0.2, 0.5, 0.3};
        auto multinomialResult = lcg.nextMultinomial(1000, probabilities);

        std::cout << "Multinomial Distribution (n=1000, p=[0.2, 0.5, 0.3]):"
                  << std::endl;
        for (size_t i = 0; i < multinomialResult.size(); ++i) {
            double percentage = 100.0 * multinomialResult[i] / 1000;
            std::cout << "Category " << i << ": " << multinomialResult[i]
                      << " occurrences (" << std::fixed << std::setprecision(2)
                      << percentage << "%)" << std::endl;
        }
        std::cout << std::endl;
    }
}

// Demonstrate array/collection operations
void testCollectionOperations(atom::utils::LCG& lcg) {
    std::cout << "\n=== Collection Operations Examples ===" << std::endl;

    // Test shuffle
    {
        std::vector<int> numbers(10);
        std::iota(numbers.begin(), numbers.end(), 1);  // Fill with 1-10

        std::cout << "Original vector: ";
        for (int n : numbers) {
            std::cout << n << " ";
        }
        std::cout << std::endl;

        lcg.shuffle(numbers);

        std::cout << "Shuffled vector: ";
        for (int n : numbers) {
            std::cout << n << " ";
        }
        std::cout << std::endl << std::endl;
    }

    // Test sample
    {
        std::vector<std::string> items = {
            "apple", "banana", "cherry",   "date", "elderberry",
            "fig",   "grape",  "honeydew", "kiwi", "lemon"};

        std::cout << "Original items: ";
        for (const auto& item : items) {
            std::cout << item << " ";
        }
        std::cout << std::endl;

        auto samples = lcg.sample(items, 5);

        std::cout << "Sampled items (5): ";
        for (const auto& item : samples) {
            std::cout << item << " ";
        }
        std::cout << std::endl << std::endl;
    }
}

// Demonstrate state saving and loading
void testStateSaving(atom::utils::LCG& lcg) {
    std::cout << "\n=== State Saving/Loading Example ===" << std::endl;

    // Generate some random numbers
    std::vector<double> originalSequence;
    for (int i = 0; i < 5; ++i) {
        double value = lcg.nextDouble();
        originalSequence.push_back(value);
        std::cout << "Original value " << i << ": " << value << std::endl;
    }

    // Save state
    const std::string stateFile = "lcg_state.bin";
    lcg.saveState(stateFile);
    std::cout << "LCG state saved to " << stateFile << std::endl;

    // Generate more random numbers (diverging sequence)
    for (int i = 0; i < 5; ++i) {
        std::cout << "Diverged value " << i << ": " << lcg.nextDouble()
                  << std::endl;
    }

    // Load state back
    lcg.loadState(stateFile);
    std::cout << "LCG state loaded from " << stateFile << std::endl;

    // Generate the same sequence again
    std::vector<double> restoredSequence;
    for (int i = 0; i < 5; ++i) {
        double value = lcg.nextDouble();
        restoredSequence.push_back(value);
        std::cout << "Restored value " << i << ": " << value << std::endl;
    }

    // Verify sequences match
    bool sequencesMatch = true;
    for (size_t i = 0; i < originalSequence.size(); ++i) {
        if (originalSequence[i] != restoredSequence[i]) {
            sequencesMatch = false;
            break;
        }
    }

    std::cout << "Sequences match: " << (sequencesMatch ? "Yes" : "No")
              << std::endl;

    // Clean up
    std::remove(stateFile.c_str());
}

// Main function with LCG usage examples
int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "LCG (Linear Congruential Generator) Usage Examples"
              << std::endl;
    std::cout << "===============================================" << std::endl;

    // Create LCG with default seed (based on current time)
    atom::utils::LCG lcg;
    std::cout << "Created LCG with default seed (time-based)" << std::endl;

    // Create LCG with specific seed for reproducible results
    const uint32_t specificSeed = 12345;
    atom::utils::LCG lcgWithSeed(specificSeed);
    std::cout << "Created LCG with specific seed: " << specificSeed << std::endl
              << std::endl;

    // Basic random number generation
    std::cout << "=== Basic Random Number Generation ===" << std::endl;
    std::cout << "Raw random number: " << lcg.next() << std::endl;
    std::cout << "Random int (0-100): " << lcg.nextInt(0, 100) << std::endl;
    std::cout << "Random double (0-1): " << lcg.nextDouble() << std::endl;
    std::cout << "Random double (5-10): " << lcg.nextDouble(5.0, 10.0)
              << std::endl;
    std::cout << "Random boolean (50% probability): "
              << (lcg.nextBernoulli() ? "true" : "false") << std::endl;
    std::cout << "Random boolean (80% probability): "
              << (lcg.nextBernoulli(0.8) ? "true" : "false") << std::endl;
    std::cout << std::endl;

    // Test all statistical distributions
    testDistributions(lcg);

    // Test discrete distributions
    testDiscreteDistributions(lcg);

    // Test collection operations
    testCollectionOperations(lcg);

    // Test state saving and loading
    testStateSaving(lcgWithSeed);

    // Test multithreading
    runInMultipleThreads(lcg, 4);

    std::cout << "\n=== Error Handling Examples ===" << std::endl;

    // Demonstrate error handling with try-catch blocks
    try {
        // This should throw an exception (min > max)
        lcg.nextInt(100, 50);
    } catch (const std::exception& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    try {
        // This should throw an exception (invalid probability)
        lcg.nextBernoulli(2.0);
    } catch (const std::exception& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    try {
        // This should throw an exception (negative lambda)
        lcg.nextGamma(-1.0, 1.0);
    } catch (const std::exception& e) {
        std::cout << "Expected error caught: " << e.what() << std::endl;
    }

    std::cout << "\nAll LCG examples completed successfully!" << std::endl;

    return 0;
}