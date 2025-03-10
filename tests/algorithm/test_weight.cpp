#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <sstream>
#include <vector>

#include "atom/algorithm/weight.hpp"
#include "atom/log/loguru.hpp"
#include "atom/macro.hpp"

using namespace atom::algorithm;

// Test fixture for WeightSelector tests
class WeightSelectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Helper to count occurrences of selected indices
    std::map<size_t, int> countSelections(WeightSelector<double>& selector,
                                          size_t numSelections) {
        std::map<size_t, int> counts;
        for (size_t i = 0; i < numSelections; ++i) {
            size_t selectedIndex = selector.select();
            counts[selectedIndex]++;
        }
        return counts;
    }

    // Helper to check if distribution matches weights approximately
    void expectDistributionMatchesWeights(const std::map<size_t, int>& counts,
                                          const std::vector<double>& weights,
                                          size_t numSelections,
                                          double marginError = 0.05) {
        double totalWeight = std::reduce(weights.begin(), weights.end());

        for (size_t i = 0; i < weights.size(); ++i) {
            double expectedProbability = weights[i] / totalWeight;
            double actualProbability =
                static_cast<double>(counts.at(i)) / numSelections;

            // Allow for some statistical variation
            EXPECT_NEAR(actualProbability, expectedProbability, marginError)
                << "Distribution mismatch at index " << i;
        }
    }
};

// Basic construction and selection test
TEST_F(WeightSelectorTest, BasicConstruction) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Total size should match
    EXPECT_EQ(selector.size(), weights.size());

    // Test getWeight function
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), weights[i]);
    }

    // Test out-of-range index
    EXPECT_FALSE(selector.getWeight(weights.size()).has_value());
}

// Test different selection strategies
TEST_F(WeightSelectorTest, SelectionStrategies) {
    std::vector<double> weights = {10.0, 20.0, 30.0, 40.0};
    const size_t NUM_SELECTIONS = 10000;

    // Default strategy
    {
        WeightSelector<double> selector(weights);
        auto counts = countSelections(selector, NUM_SELECTIONS);
        expectDistributionMatchesWeights(counts, weights, NUM_SELECTIONS);
    }

    // Bottom heavy strategy
    {
        auto strategy = std::make_unique<
            WeightSelector<double>::BottomHeavySelectionStrategy>();
        WeightSelector<double> selector(weights, std::move(strategy));

        // With bottom heavy, lower indices should be selected more often than
        // direct proportion
        auto counts = countSelections(selector, NUM_SELECTIONS);

        // We don't test exact distribution here because it's modified by sqrt,
        // but we can verify all indices are selected
        for (size_t i = 0; i < weights.size(); ++i) {
            EXPECT_GT(counts[i], 0);
        }
    }

    // Top heavy strategy
    {
        auto strategy = std::make_unique<TopHeavySelectionStrategy<double>>();
        WeightSelector<double> selector(weights, std::move(strategy));

        // With top heavy, higher indices should be selected more often
        auto counts = countSelections(selector, NUM_SELECTIONS);

        // Again, we just verify all indices are selected
        for (size_t i = 0; i < weights.size(); ++i) {
            EXPECT_GT(counts[i], 0);
        }
    }

    // Random selection strategy
    {
        auto strategy =
            std::make_unique<WeightSelector<double>::RandomSelectionStrategy>(
                weights.size());
        WeightSelector<double> selector(weights, std::move(strategy));

        auto counts = countSelections(selector, NUM_SELECTIONS);

        // Each index should be selected approximately the same number of times
        for (size_t i = 0; i < weights.size(); ++i) {
            double expectedProbability = 1.0 / weights.size();
            double actualProbability =
                static_cast<double>(counts[i]) / NUM_SELECTIONS;
            EXPECT_NEAR(actualProbability, expectedProbability, 0.05);
        }
    }
}

// Test modifying weights
TEST_F(WeightSelectorTest, ModifyWeights) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Test updateWeight
    selector.updateWeight(1, 10.0);
    {
        auto weight = selector.getWeight(1);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), 10.0);
    }

    // Test addWeight
    selector.addWeight(5.0);
    EXPECT_EQ(selector.size(), weights.size() + 1);
    {
        auto weight = selector.getWeight(weights.size());
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), 5.0);
    }

    // Test removeWeight
    selector.removeWeight(2);
    EXPECT_EQ(selector.size(), weights.size());
    {
        auto weight = selector.getWeight(2);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(),
                  4.0);  // Original index 3 value now at index 2
    }

    // Test resetWeights
    std::vector<double> newWeights = {5.0, 6.0, 7.0};
    selector.resetWeights(newWeights);
    EXPECT_EQ(selector.size(), newWeights.size());
    for (size_t i = 0; i < newWeights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), newWeights[i]);
    }
}

// Test weight calculations
TEST_F(WeightSelectorTest, WeightCalculations) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Test getTotalWeight
    EXPECT_EQ(selector.getTotalWeight(), 10.0);

    // Test getAverageWeight
    EXPECT_EQ(selector.getAverageWeight(), 2.5);

    // Test getMaxWeightIndex
    EXPECT_EQ(selector.getMaxWeightIndex(), 3);

    // Test getMinWeightIndex
    EXPECT_EQ(selector.getMinWeightIndex(), 0);

    // Test normalizeWeights
    selector.normalizeWeights();
    EXPECT_NEAR(selector.getTotalWeight(), 1.0, 1e-10);

    // Check normalized weights
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_NEAR(weight.value(), weights[i] / 10.0, 1e-10);
    }

    // Test scaleWeights
    selector.scaleWeights(2.0);
    EXPECT_NEAR(selector.getTotalWeight(), 2.0, 1e-10);
}

// Test applyFunctionToWeights
TEST_F(WeightSelectorTest, ApplyFunction) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Apply square function
    selector.applyFunctionToWeights([](double w) -> double { return w * w; });

    // Check squared weights
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_NEAR(weight.value(), weights[i] * weights[i], 1e-10);
    }
}

// Test batch update
TEST_F(WeightSelectorTest, BatchUpdate) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    std::vector<std::pair<size_t, double>> updates = {{0, 10.0}, {2, 30.0}};

    selector.batchUpdateWeights(updates);

    // Check updated weights
    auto weight0 = selector.getWeight(0);
    ASSERT_TRUE(weight0.has_value());
    EXPECT_EQ(weight0.value(), 10.0);

    auto weight2 = selector.getWeight(2);
    ASSERT_TRUE(weight2.has_value());
    EXPECT_EQ(weight2.value(), 30.0);

    // Check unchanged weight
    auto weight1 = selector.getWeight(1);
    ASSERT_TRUE(weight1.has_value());
    EXPECT_EQ(weight1.value(), 2.0);
}

// Test select multiple
TEST_F(WeightSelectorTest, SelectMultiple) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    size_t numSelections = 5;
    auto selections = selector.selectMultiple(numSelections);

    // Check correct number of selections
    EXPECT_EQ(selections.size(), numSelections);

    // Check all selections are in valid range
    for (auto idx : selections) {
        EXPECT_LT(idx, weights.size());
    }
}

// Test WeightedRandomSampler
TEST_F(WeightSelectorTest, WeightedRandomSampler) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double>::WeightedRandomSampler sampler;

    size_t numSamples = 10000;
    auto samples = sampler.sample(weights, numSamples);

    // Check correct number of samples
    EXPECT_EQ(samples.size(), numSamples);

    // Count occurrences of each index
    std::map<size_t, int> counts;
    for (auto idx : samples) {
        counts[idx]++;
    }

    // Check distribution approximately matches weights
    expectDistributionMatchesWeights(counts, weights, numSamples);
}

// Test error cases
TEST_F(WeightSelectorTest, ErrorCases) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Out of range index for updateWeight
    EXPECT_THROW(selector.updateWeight(10, 5.0), std::out_of_range);

    // Out of range index for removeWeight
    EXPECT_THROW(selector.removeWeight(10), std::out_of_range);

    // Out of range index in batch update
    std::vector<std::pair<size_t, double>> badUpdates = {
        {0, 10.0}, {10, 30.0}  // This index is out of range
    };
    EXPECT_THROW(selector.batchUpdateWeights(badUpdates), std::out_of_range);

    // Empty weights for getAverageWeight
    WeightSelector<double> emptySelector(std::vector<double>{});
    EXPECT_THROW(ATOM_UNUSED_RESULT(emptySelector.getAverageWeight()), std::runtime_error);

    // Zero total weight for select
    WeightSelector<double> zeroSelector(std::vector<double>{0.0, 0.0, 0.0});
    EXPECT_THROW(zeroSelector.select(), std::runtime_error);
}

// Test printWeights
TEST_F(WeightSelectorTest, PrintWeights) {
    // Test with normal weights
    {
        std::vector<double> weights = {1.0, 2.5, 3.75};
        WeightSelector<double> selector(weights);

        std::stringstream ss;
        selector.printWeights(ss);

#ifdef ATOM_USE_BOOST
        // Using boost format which might have slightly different formatting
        EXPECT_FALSE(ss.str().empty());
#else
        EXPECT_EQ(ss.str(), "[1.00, 2.50, 3.75]\n");
#endif
    }

    // Test with empty weights
    {
        std::vector<double> emptyWeights;
        WeightSelector<double> selector(emptyWeights);

        std::stringstream ss;
        selector.printWeights(ss);
        EXPECT_EQ(ss.str(), "[]\n");
    }
}

// Test with int weights instead of double
TEST_F(WeightSelectorTest, IntegerWeights) {
    std::vector<int> weights = {1, 2, 3, 4};
    WeightSelector<int> selector(weights);

    // Basic checks
    EXPECT_EQ(selector.size(), weights.size());
    EXPECT_EQ(selector.getTotalWeight(), 10);
    EXPECT_EQ(selector.getMaxWeightIndex(), 3);

    // Selection should work with int weights too
    size_t selectedIndex = selector.select();
    EXPECT_LT(selectedIndex, weights.size());
}

// Test changing selection strategy
TEST_F(WeightSelectorTest, ChangeStrategy) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);

    // Initial selections with default strategy
    std::map<size_t, int> defaultCounts;
    for (size_t i = 0; i < 1000; ++i) {
        defaultCounts[selector.select()]++;
    }

    // Change to random strategy
    auto randomStrategy =
        std::make_unique<WeightSelector<double>::RandomSelectionStrategy>(
            weights.size());
    selector.setSelectionStrategy(std::move(randomStrategy));

    // Selections with random strategy
    std::map<size_t, int> randomCounts;
    for (size_t i = 0; i < 1000; ++i) {
        randomCounts[selector.select()]++;
    }

    // Distribution should be different (though this is a statistical test,
    // might occasionally fail)
    double totalDefaultDiff = 0.0;
    double totalRandomDiff = 0.0;
    double totalWeight = std::reduce(weights.begin(), weights.end());

    for (size_t i = 0; i < weights.size(); ++i) {
        double expectedWeightedProb = weights[i] / totalWeight;
        double expectedUniformProb = 1.0 / weights.size();

        double defaultProb = static_cast<double>(defaultCounts[i]) / 1000;
        double randomProb = static_cast<double>(randomCounts[i]) / 1000;

        totalDefaultDiff += std::abs(defaultProb - expectedWeightedProb);
        totalRandomDiff += std::abs(randomProb - expectedUniformProb);
    }

    // The difference patterns should be distinct
    double diffRatio = totalRandomDiff / totalDefaultDiff;
    EXPECT_TRUE(diffRatio < 0.5 || diffRatio > 1.5);
}

// Test with edge case weight values
TEST_F(WeightSelectorTest, EdgeCaseWeights) {
    // Test with very large weights
    {
        std::vector<double> largeWeights = {1e9, 2e9, 3e9};
        WeightSelector<double> selector(largeWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 6e9, 1e-5);

        // Selection should still work
        size_t selectedIndex = selector.select();
        EXPECT_LT(selectedIndex, largeWeights.size());
    }

    // Test with very small weights
    {
        std::vector<double> smallWeights = {1e-9, 2e-9, 3e-9};
        WeightSelector<double> selector(smallWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 6e-9, 1e-15);

        // Selection should still work
        size_t selectedIndex = selector.select();
        EXPECT_LT(selectedIndex, smallWeights.size());
    }

    // Test with weights having very different magnitudes
    {
        std::vector<double> mixedWeights = {1e-9, 1.0, 1e9};
        WeightSelector<double> selector(mixedWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 1e9 + 1.0 + 1e-9, 1e-5);

        // With such different magnitudes, almost all selections should be index
        // 2
        const size_t NUM_SELECTIONS = 1000;
        std::map<size_t, int> counts;
        for (size_t i = 0; i < NUM_SELECTIONS; ++i) {
            counts[selector.select()]++;
        }

        // Index 2 should be selected almost always
        EXPECT_GT(counts[2], NUM_SELECTIONS * 0.99);
    }
}
