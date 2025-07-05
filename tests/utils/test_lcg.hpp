// filepath: /home/max/Atom-1/atom/utils/test_lcg.hpp
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

#include "lcg.hpp"

namespace atom::utils::tests {

class LCGTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a fixed seed for deterministic tests
        lcg = std::make_unique<LCG>(42);
    }

    // Helper function to check if a value is within a range
    template <typename T>
    bool isInRange(T value, T min, T max) {
        return value >= min && value <= max;
    }

    // Helper function to check statistical properties
    template <typename F>
    void testDistributionMean(F generator, double expectedMean,
                              double tolerance, int samples = 10000) {
        std::vector<double> values;
        values.reserve(samples);

        for (int i = 0; i < samples; ++i) {
            values.push_back(generator());
        }

        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        double mean = sum / samples;

        EXPECT_NEAR(mean, expectedMean, tolerance)
            << "Mean should be approximately " << expectedMean;
    }

    // Create temporary file for state tests
    std::string createTempFile() {
        std::string filename =
            "lcg_test_state_" + std::to_string(rand()) + ".tmp";
        return filename;
    }

    // Clean up temporary file
    void cleanupTempFile(const std::string& filename) {
        std::remove(filename.c_str());
    }

    std::unique_ptr<LCG> lcg;
};

TEST_F(LCGTest, SeedChangesState) {
    // Generate some initial numbers
    auto val1 = lcg->next();
    auto val2 = lcg->next();

    // Reset with the same seed
    lcg->seed(42);

    // Should get the same sequence
    EXPECT_EQ(lcg->next(), val1) << "Reseeding should reset the sequence";
    EXPECT_EQ(lcg->next(), val2)
        << "Sequence should continue as expected after reset";

    // Different seed should produce different results
    lcg->seed(43);
    EXPECT_NE(lcg->next(), val1)
        << "Different seed should produce different results";
}

TEST_F(LCGTest, NextGeneratesConsistentSequence) {
    std::vector<LCG::result_type> sequence1;
    std::vector<LCG::result_type> sequence2;

    // Generate first sequence
    lcg->seed(42);
    for (int i = 0; i < 100; ++i) {
        sequence1.push_back(lcg->next());
    }

    // Generate second sequence with same seed
    lcg->seed(42);
    for (int i = 0; i < 100; ++i) {
        sequence2.push_back(lcg->next());
    }

    // Sequences should be identical
    EXPECT_EQ(sequence1, sequence2)
        << "Same seed should produce identical sequences";
}

TEST_F(LCGTest, NextIntRangeEnforced) {
    const int min = -10;
    const int max = 10;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        int value = lcg->nextInt(min, max);
        EXPECT_TRUE(isInRange(value, min, max))
            << "nextInt() should return values within the specified range";
    }

    // Test error case
    EXPECT_THROW(lcg->nextInt(10, 5), std::invalid_argument)
        << "nextInt() should throw when min > max";
}

TEST_F(LCGTest, NextDoubleRangeEnforced) {
    const double min = -5.5;
    const double max = 7.8;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        double value = lcg->nextDouble(min, max);
        EXPECT_TRUE(isInRange(value, min, max))
            << "nextDouble() should return values within the specified range";
    }

    // Test default range [0, 1)
    for (int i = 0; i < iterations; ++i) {
        double value = lcg->nextDouble();
        EXPECT_TRUE(isInRange(value, 0.0, 1.0))
            << "nextDouble() should return values within [0, 1) by default";
        EXPECT_LT(value, 1.0)
            << "nextDouble() without args should be less than 1.0";
    }

    // Test error case
    EXPECT_THROW(lcg->nextDouble(10.0, 5.0), std::invalid_argument)
        << "nextDouble() should throw when min >= max";
}

TEST_F(LCGTest, ValidateProbabilityChecksRange) {
    // Valid probabilities
    EXPECT_NO_THROW(lcg->validateProbability(0.0))
        << "0.0 should be a valid probability";
    EXPECT_NO_THROW(lcg->validateProbability(0.5))
        << "0.5 should be a valid probability";
    EXPECT_NO_THROW(lcg->validateProbability(1.0))
        << "1.0 should be a valid probability";

    // Invalid probabilities
    EXPECT_THROW(lcg->validateProbability(-0.1), std::invalid_argument)
        << "Negative probabilities should throw";
    EXPECT_THROW(lcg->validateProbability(1.1), std::invalid_argument)
        << "Probabilities > 1.0 should throw";

    // Test with allowZeroOne = false
    EXPECT_THROW(lcg->validateProbability(0.0, false), std::invalid_argument)
        << "0.0 should not be valid when allowZeroOne is false";
    EXPECT_THROW(lcg->validateProbability(1.0, false), std::invalid_argument)
        << "1.0 should not be valid when allowZeroOne is false";
    EXPECT_NO_THROW(lcg->validateProbability(0.5, false))
        << "0.5 should be valid regardless of allowZeroOne";
}

TEST_F(LCGTest, NextBernoulliDistribution) {
    // Test probability 0.0 should always return false
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(lcg->nextBernoulli(0.0))
            << "nextBernoulli(0.0) should always return false";
    }

    // Test probability 1.0 should always return true
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(lcg->nextBernoulli(1.0))
            << "nextBernoulli(1.0) should always return true";
    }

    // Test distribution with p=0.5
    testDistributionMean([this]() { return lcg->nextBernoulli() ? 1.0 : 0.0; },
                         0.5, 0.05);

    // Test invalid probability
    EXPECT_THROW(lcg->nextBernoulli(-0.1), std::invalid_argument)
        << "Negative probability should throw";
    EXPECT_THROW(lcg->nextBernoulli(1.1), std::invalid_argument)
        << "Probability > 1.0 should throw";
}

TEST_F(LCGTest, NextGaussianDistribution) {
    const double mean = 5.0;
    const double stddev = 2.0;

    // Test mean and stddev
    testDistributionMean(
        [this, mean, stddev]() { return lcg->nextGaussian(mean, stddev); },
        mean, stddev * 0.1);

    // Generate many samples and check variance
    const int samples = 10000;
    std::vector<double> values;
    values.reserve(samples);

    for (int i = 0; i < samples; ++i) {
        values.push_back(lcg->nextGaussian(mean, stddev));
    }

    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double sampleMean = sum / samples;

    double variance = 0;
    for (double value : values) {
        variance += (value - sampleMean) * (value - sampleMean);
    }
    variance /= (samples - 1);

    double sampleStddev = std::sqrt(variance);
    EXPECT_NEAR(sampleStddev, stddev, stddev * 0.2)
        << "Standard deviation should be approximately as specified";

    // Test invalid stddev
    EXPECT_THROW(lcg->nextGaussian(0.0, -1.0), std::invalid_argument)
        << "Negative stddev should throw";
    EXPECT_THROW(lcg->nextGaussian(0.0, 0.0), std::invalid_argument)
        << "Zero stddev should throw";
}

TEST_F(LCGTest, NextPoissonDistribution) {
    const double lambda = 5.0;

    // Test mean (should equal lambda for Poisson)
    testDistributionMean(
        [this, lambda]() {
            return static_cast<double>(lcg->nextPoisson(lambda));
        },
        lambda, lambda * 0.1);

    // Test invalid lambda
    EXPECT_THROW(lcg->nextPoisson(-1.0), std::invalid_argument)
        << "Negative lambda should throw";
    EXPECT_THROW(lcg->nextPoisson(0.0), std::invalid_argument)
        << "Zero lambda should throw";
}

TEST_F(LCGTest, NextExponentialDistribution) {
    const double lambda = 2.0;
    const double expectedMean = 1.0 / lambda;

    // Test mean (should equal 1/lambda for Exponential)
    testDistributionMean(
        [this, lambda]() { return lcg->nextExponential(lambda); }, expectedMean,
        expectedMean * 0.2);

    // Test all values are non-negative
    for (int i = 0; i < 1000; ++i) {
        EXPECT_GE(lcg->nextExponential(lambda), 0.0)
            << "Exponential distribution should produce non-negative values";
    }

    // Test invalid lambda
    EXPECT_THROW(lcg->nextExponential(-1.0), std::invalid_argument)
        << "Negative lambda should throw";
    EXPECT_THROW(lcg->nextExponential(0.0), std::invalid_argument)
        << "Zero lambda should throw";
}

TEST_F(LCGTest, NextGeometricDistribution) {
    const double probability = 0.2;
    const double expectedMean = 1.0 / probability;

    // Test mean (should equal 1/p for Geometric)
    testDistributionMean(
        [this, probability]() {
            return static_cast<double>(lcg->nextGeometric(probability));
        },
        expectedMean, expectedMean * 0.2);

    // Test all values are positive
    for (int i = 0; i < 100; ++i) {
        EXPECT_GT(lcg->nextGeometric(probability), 0)
            << "Geometric distribution should produce positive values";
    }

    // Test invalid probability
    EXPECT_THROW(lcg->nextGeometric(0.0), std::invalid_argument)
        << "Zero probability should throw";
    EXPECT_THROW(lcg->nextGeometric(1.0), std::invalid_argument)
        << "Probability 1.0 should throw";
    EXPECT_THROW(lcg->nextGeometric(-0.1), std::invalid_argument)
        << "Negative probability should throw";
    EXPECT_THROW(lcg->nextGeometric(1.1), std::invalid_argument)
        << "Probability > 1.0 should throw";
}

TEST_F(LCGTest, NextGammaDistribution) {
    const double shape = 2.0;
    const double scale = 3.0;
    const double expectedMean = shape * scale;

    // Test mean (should equal shape*scale for Gamma)
    testDistributionMean(
        [this, shape, scale]() { return lcg->nextGamma(shape, scale); },
        expectedMean, expectedMean * 0.2);

    // Test all values are positive
    for (int i = 0; i < 100; ++i) {
        EXPECT_GT(lcg->nextGamma(shape, scale), 0.0)
            << "Gamma distribution should produce positive values";
    }

    // Test invalid parameters
    EXPECT_THROW(lcg->nextGamma(-1.0, 1.0), std::invalid_argument)
        << "Negative shape should throw";
    EXPECT_THROW(lcg->nextGamma(0.0, 1.0), std::invalid_argument)
        << "Zero shape should throw";
    EXPECT_THROW(lcg->nextGamma(1.0, -1.0), std::invalid_argument)
        << "Negative scale should throw";
    EXPECT_THROW(lcg->nextGamma(1.0, 0.0), std::invalid_argument)
        << "Zero scale should throw";
}

TEST_F(LCGTest, NextBetaDistribution) {
    const double alpha = 2.0;
    const double beta = 3.0;
    const double expectedMean = alpha / (alpha + beta);

    // Test mean (should equal alpha/(alpha+beta) for Beta)
    testDistributionMean(
        [this, alpha, beta]() { return lcg->nextBeta(alpha, beta); },
        expectedMean, 0.1);

    // Test all values are between 0 and 1
    for (int i = 0; i < 1000; ++i) {
        double value = lcg->nextBeta(alpha, beta);
        EXPECT_TRUE(isInRange(value, 0.0, 1.0))
            << "Beta distribution should produce values in range [0, 1]";
    }

    // Special case: alpha=1, beta=1 should be uniform(0,1)
    testDistributionMean([this]() { return lcg->nextBeta(1.0, 1.0); }, 0.5,
                         0.1);

    // Test invalid parameters
    EXPECT_THROW(lcg->nextBeta(-1.0, 1.0), std::invalid_argument)
        << "Negative alpha should throw";
    EXPECT_THROW(lcg->nextBeta(0.0, 1.0), std::invalid_argument)
        << "Zero alpha should throw";
    EXPECT_THROW(lcg->nextBeta(1.0, -1.0), std::invalid_argument)
        << "Negative beta should throw";
    EXPECT_THROW(lcg->nextBeta(1.0, 0.0), std::invalid_argument)
        << "Zero beta should throw";
}

TEST_F(LCGTest, NextChiSquaredDistribution) {
    const double df = 3.0;
    const double expectedMean = df;

    // Test mean (should equal df for Chi-Squared)
    testDistributionMean([this, df]() { return lcg->nextChiSquared(df); },
                         expectedMean, expectedMean * 0.2);

    // Test all values are positive
    for (int i = 0; i < 100; ++i) {
        EXPECT_GT(lcg->nextChiSquared(df), 0.0)
            << "Chi-Squared distribution should produce positive values";
    }

    // Test invalid degrees of freedom
    EXPECT_THROW(lcg->nextChiSquared(-1.0), std::invalid_argument)
        << "Negative degrees of freedom should throw";
    EXPECT_THROW(lcg->nextChiSquared(0.0), std::invalid_argument)
        << "Zero degrees of freedom should throw";
}

TEST_F(LCGTest, NextHypergeometricDistribution) {
    const int total = 100;
    const int success = 40;
    const int draws = 20;
    const double expectedMean = draws * (static_cast<double>(success) / total);

    // Test mean (should equal draws * (success/total) for Hypergeometric)
    testDistributionMean(
        [this]() {
            return static_cast<double>(
                lcg->nextHypergeometric(total, success, draws));
        },
        expectedMean, 1.0);

    // Test bounds
    for (int i = 0; i < 100; ++i) {
        int value = lcg->nextHypergeometric(total, success, draws);
        EXPECT_TRUE(isInRange(value, 0, std::min(success, draws)))
            << "Hypergeometric should be in range [0, min(success, draws)]";
    }

    // Test invalid parameters
    EXPECT_THROW(lcg->nextHypergeometric(-1, 10, 5), std::invalid_argument)
        << "Negative total should throw";
    EXPECT_THROW(lcg->nextHypergeometric(10, -1, 5), std::invalid_argument)
        << "Negative success should throw";
    EXPECT_THROW(lcg->nextHypergeometric(10, 5, -1), std::invalid_argument)
        << "Negative draws should throw";
    EXPECT_THROW(lcg->nextHypergeometric(10, 15, 5), std::invalid_argument)
        << "success > total should throw";
    EXPECT_THROW(lcg->nextHypergeometric(10, 5, 15), std::invalid_argument)
        << "draws > total should throw";
}

TEST_F(LCGTest, NextDiscreteDistribution) {
    std::vector<double> weights = {10, 20, 30, 40};
    const int iterations = 10000;

    // Count occurrences of each index
    std::vector<int> counts(weights.size(), 0);
    for (int i = 0; i < iterations; ++i) {
        int index = lcg->nextDiscrete(weights);
        EXPECT_TRUE(isInRange(index, 0, static_cast<int>(weights.size()) - 1))
            << "nextDiscrete() should return an index within range";
        counts[index]++;
    }

    // Check that distribution is approximately proportional to weights
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (size_t i = 0; i < weights.size(); ++i) {
        double expected = static_cast<double>(iterations) * weights[i] / sum;
        double tolerance =
            std::sqrt(expected) * 3;  // Allow for 3 sigma variation
        EXPECT_NEAR(counts[i], expected, tolerance)
            << "Index " << i
            << " should be selected with frequency proportional to weight";
    }

    // Test with span
    std::array<double, 3> weightsArray = {1.0, 2.0, 3.0};
    std::span<const double> weightsSpan(weightsArray);
    EXPECT_NO_THROW(lcg->nextDiscrete(weightsSpan))
        << "nextDiscrete() should accept std::span";

    // Test empty weights
    std::vector<double> emptyWeights;
    EXPECT_THROW(lcg->nextDiscrete(emptyWeights), std::invalid_argument)
        << "Empty weights should throw";

    // Test negative weights
    std::vector<double> negativeWeights = {1.0, -1.0, 2.0};
    EXPECT_THROW(lcg->nextDiscrete(negativeWeights), std::invalid_argument)
        << "Negative weights should throw";

    // Test all zero weights
    std::vector<double> zeroWeights = {0.0, 0.0, 0.0};
    EXPECT_THROW(lcg->nextDiscrete(zeroWeights), std::invalid_argument)
        << "All-zero weights should throw";
}

TEST_F(LCGTest, NextMultinomialDistribution) {
    std::vector<double> probs = {0.1, 0.3, 0.6};
    int trials = 1000;

    // Test sum of outcomes equals trials
    std::vector<int> result = lcg->nextMultinomial(trials, probs);
    EXPECT_EQ(result.size(), probs.size())
        << "Result size should match probabilities size";

    int sum = std::accumulate(result.begin(), result.end(), 0);
    EXPECT_EQ(sum, trials)
        << "Sum of multinomial outcomes should equal number of trials";

    // Test distribution approximately matches probabilities
    for (size_t i = 0; i < probs.size(); ++i) {
        double expected = trials * probs[i];
        double tolerance =
            std::sqrt(expected) * 3;  // Allow for 3 sigma variation
        EXPECT_NEAR(result[i], expected, tolerance)
            << "Category " << i << " should occur with expected frequency";
    }

    // Test with span
    std::array<double, 2> probsArray = {0.3, 0.7};
    std::span<const double> probsSpan(probsArray);
    EXPECT_NO_THROW(lcg->nextMultinomial(10, probsSpan))
        << "nextMultinomial() should accept std::span";

    // Test invalid parameters
    EXPECT_THROW(lcg->nextMultinomial(-1, probs), std::invalid_argument)
        << "Negative trials should throw";

    std::vector<double> invalidProbs = {0.3, 0.9};  // Sum > 1
    EXPECT_THROW(lcg->nextMultinomial(10, invalidProbs), std::invalid_argument)
        << "Invalid probabilities should throw";

    std::vector<double> emptyProbs;
    EXPECT_THROW(lcg->nextMultinomial(10, emptyProbs), std::invalid_argument)
        << "Empty probabilities should throw";
}

TEST_F(LCGTest, ShuffleFunction) {
    std::vector<int> vec(100);
    std::iota(vec.begin(), vec.end(), 1);  // Fill with 1..100
    std::vector<int> original = vec;

    // Shuffle and verify it changed
    lcg->shuffle(vec);
    EXPECT_NE(vec, original) << "Shuffle should change the order of elements";

    // Verify no elements were lost
    std::sort(vec.begin(), vec.end());
    EXPECT_EQ(vec, original)
        << "Shuffled vector should contain the same elements";

    // Test with array
    std::array<char, 5> arr = {'a', 'b', 'c', 'd', 'e'};
    std::array<char, 5> arrOriginal = arr;

    lcg->shuffle(arr);

    // Sort and check contents
    std::sort(arr.begin(), arr.end());
    std::sort(arrOriginal.begin(), arrOriginal.end());
    EXPECT_EQ(arr, arrOriginal)
        << "Shuffled array should contain the same elements";
}

TEST_F(LCGTest, SampleFunction) {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 1);  // Fill with 1..100

    // Test with valid sample size
    int sampleSize = 20;
    auto sample = lcg->sample(data, sampleSize);

    EXPECT_EQ(sample.size(), static_cast<size_t>(sampleSize))
        << "Sample size should match requested size";

    // Check all elements in sample are from the original data
    for (int value : sample) {
        EXPECT_NE(std::find(data.begin(), data.end(), value), data.end())
            << "Sampled element should be in the original data";
    }

    // Test with sample size = data size
    sample = lcg->sample(data, static_cast<int>(data.size()));
    EXPECT_EQ(sample.size(), data.size())
        << "Sample with full size should match original size";

    std::sort(sample.begin(), sample.end());
    EXPECT_EQ(sample, data)
        << "Full sample should contain all original elements";

    // Test with too large sample size
    EXPECT_THROW(lcg->sample(data, data.size() + 1), std::invalid_argument)
        << "Sample size larger than data size should throw";
}

TEST_F(LCGTest, SaveAndLoadState) {
    // Generate some random numbers
    std::vector<int> sequence1;
    for (int i = 0; i < 10; ++i) {
        sequence1.push_back(lcg->nextInt(1, 1000));
    }

    // Save state
    std::string filename = createTempFile();
    lcg->saveState(filename);

    // Generate more random numbers
    for (int i = 0; i < 10; ++i) {
        lcg->nextInt(1, 1000);
    }

    // Load state
    lcg->loadState(filename);

    // Generate new sequence from loaded state
    std::vector<int> sequence2;
    for (int i = 0; i < 10; ++i) {
        sequence2.push_back(lcg->nextInt(1, 1000));
    }

    // Verify sequences match
    EXPECT_EQ(sequence1, sequence2)
        << "Loading a saved state should restore the RNG sequence";

    // Clean up
    cleanupTempFile(filename);

    // Test error cases
    EXPECT_THROW(lcg->loadState("nonexistent_file.dat"), std::runtime_error)
        << "Loading non-existent file should throw";
}

TEST_F(LCGTest, ThreadSafety) {
    const int NUM_THREADS = 10;
    const int ITERATIONS_PER_THREAD = 10000;

    // Test that we can generate random numbers from multiple threads
    std::vector<std::thread> threads;
    std::vector<std::vector<int>> results(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &results]() {
            std::vector<int>& thread_results = results[t];
            thread_results.reserve(ITERATIONS_PER_THREAD);

            for (int i = 0; i < ITERATIONS_PER_THREAD; ++i) {
                thread_results.push_back(lcg->nextInt(1, 1000));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that each thread got the expected number of results
    for (int t = 0; t < NUM_THREADS; ++t) {
        EXPECT_EQ(results[t].size(), ITERATIONS_PER_THREAD)
            << "Thread " << t
            << " should generate the expected number of random values";
    }
}

}  // namespace atom::utils::tests
