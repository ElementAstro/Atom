#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <map>
#include <sstream>
#include <vector>
#include "atom/algorithm/weight.hpp"
#include "atom/macro.hpp"

using namespace atom::algorithm;

class WeightSelectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    std::map<size_t, int> countSelections(WeightSelector<double>& selector,
                                          size_t numSelections) {
        std::map<size_t, int> counts;
        for (size_t i = 0; i < numSelections; ++i) {
            size_t selectedIndex = selector.select();
            counts[selectedIndex]++;
        }
        return counts;
    }

    void expectDistributionMatchesWeights(const std::map<size_t, int>& counts,
                                          const std::vector<double>& weights,
                                          size_t numSelections,
                                          double marginError = 0.05) {
        double totalWeight = std::reduce(weights.begin(), weights.end());
        for (size_t i = 0; i < weights.size(); ++i) {
            double expectedProbability = weights[i] / totalWeight;
            double actualProbability =
                static_cast<double>(counts.at(i)) / numSelections;
            EXPECT_NEAR(actualProbability, expectedProbability, marginError)
                << "Distribution mismatch at index " << i;
        }
    }
};

TEST_F(WeightSelectorTest, BasicConstruction) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    EXPECT_EQ(selector.size(), weights.size());
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), weights[i]);
    }
    EXPECT_FALSE(selector.getWeight(weights.size()).has_value());
}

TEST_F(WeightSelectorTest, SelectionStrategies) {
    std::vector<double> weights = {10.0, 20.0, 30.0, 40.0};
    const size_t NUM_SELECTIONS = 10000;

    {
        WeightSelector<double> selector(weights);
        auto counts = countSelections(selector, NUM_SELECTIONS);
        expectDistributionMatchesWeights(counts, weights, NUM_SELECTIONS);
    }
    {
        auto strategy = std::make_unique<
            WeightSelector<double>::BottomHeavySelectionStrategy>();
        WeightSelector<double> selector(weights, std::move(strategy));
        auto counts = countSelections(selector, NUM_SELECTIONS);
        for (size_t i = 0; i < weights.size(); ++i) {
            EXPECT_GT(counts[i], 0);
        }
    }
    {
        auto strategy = std::make_unique<
            WeightSelector<double>::TopHeavySelectionStrategy>();
        WeightSelector<double> selector(weights, std::move(strategy));
        auto counts = countSelections(selector, NUM_SELECTIONS);
        for (size_t i = 0; i < weights.size(); ++i) {
            EXPECT_GT(counts[i], 0);
        }
    }
    {
        auto strategy =
            std::make_unique<WeightSelector<double>::RandomSelectionStrategy>(
                weights.size());
        WeightSelector<double> selector(weights, std::move(strategy));
        auto counts = countSelections(selector, NUM_SELECTIONS);
        for (size_t i = 0; i < weights.size(); ++i) {
            double expectedProbability = 1.0 / weights.size();
            double actualProbability =
                static_cast<double>(counts[i]) / NUM_SELECTIONS;
            EXPECT_NEAR(actualProbability, expectedProbability, 0.05);
        }
    }
}

TEST_F(WeightSelectorTest, ModifyWeights) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    selector.updateWeight(1, 10.0);
    {
        auto weight = selector.getWeight(1);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), 10.0);
    }
    selector.addWeight(5.0);
    EXPECT_EQ(selector.size(), weights.size() + 1);
    {
        auto weight = selector.getWeight(weights.size());
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), 5.0);
    }
    selector.removeWeight(2);
    EXPECT_EQ(selector.size(), weights.size());
    {
        auto weight = selector.getWeight(2);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), 4.0);
    }
    std::vector<double> newWeights = {5.0, 6.0, 7.0};
    selector.resetWeights(newWeights);
    EXPECT_EQ(selector.size(), newWeights.size());
    for (size_t i = 0; i < newWeights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_EQ(weight.value(), newWeights[i]);
    }
}

TEST_F(WeightSelectorTest, WeightCalculations) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    EXPECT_EQ(selector.getTotalWeight(), 10.0);
    EXPECT_EQ(selector.getAverageWeight(), 2.5);
    EXPECT_EQ(selector.getMaxWeightIndex(), 3);
    EXPECT_EQ(selector.getMinWeightIndex(), 0);
    selector.normalizeWeights();
    EXPECT_NEAR(selector.getTotalWeight(), 1.0, 1e-10);
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_NEAR(weight.value(), weights[i] / 10.0, 1e-10);
    }
    selector.scaleWeights(2.0);
    EXPECT_NEAR(selector.getTotalWeight(), 2.0, 1e-10);
}

TEST_F(WeightSelectorTest, ApplyFunction) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    selector.applyFunctionToWeights([](double w) -> double { return w * w; });
    for (size_t i = 0; i < weights.size(); ++i) {
        auto weight = selector.getWeight(i);
        ASSERT_TRUE(weight.has_value());
        EXPECT_NEAR(weight.value(), weights[i] * weights[i], 1e-10);
    }
}

TEST_F(WeightSelectorTest, BatchUpdate) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    std::vector<std::pair<size_t, double>> updates = {{0, 10.0}, {2, 30.0}};
    selector.batchUpdateWeights(updates);
    auto weight0 = selector.getWeight(0);
    ASSERT_TRUE(weight0.has_value());
    EXPECT_EQ(weight0.value(), 10.0);
    auto weight2 = selector.getWeight(2);
    ASSERT_TRUE(weight2.has_value());
    EXPECT_EQ(weight2.value(), 30.0);
    auto weight1 = selector.getWeight(1);
    ASSERT_TRUE(weight1.has_value());
    EXPECT_EQ(weight1.value(), 2.0);
}

TEST_F(WeightSelectorTest, SelectMultiple) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    size_t numSelections = 5;
    auto selections = selector.selectMultiple(numSelections);
    EXPECT_EQ(selections.size(), numSelections);
    for (auto idx : selections) {
        EXPECT_LT(idx, weights.size());
    }
}

TEST_F(WeightSelectorTest, WeightedRandomSampler) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double>::WeightedRandomSampler sampler;
    size_t numSamples = 10000;
    auto samples = sampler.sample(weights, numSamples);
    EXPECT_EQ(samples.size(), numSamples);
    std::map<size_t, int> counts;
    for (auto idx : samples) {
        counts[idx]++;
    }
    expectDistributionMatchesWeights(counts, weights, numSamples);
}

TEST_F(WeightSelectorTest, ErrorCases) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    EXPECT_THROW(selector.updateWeight(10, 5.0), std::out_of_range);
    EXPECT_THROW(selector.removeWeight(10), std::out_of_range);
    std::vector<std::pair<size_t, double>> badUpdates = {{0, 10.0}, {10, 30.0}};
    EXPECT_THROW(selector.batchUpdateWeights(badUpdates), std::out_of_range);
    WeightSelector<double> emptySelector(std::vector<double>{});
    EXPECT_THROW(static_cast<void>(emptySelector.getAverageWeight()),
                 std::runtime_error);
    WeightSelector<double> zeroSelector(std::vector<double>{0.0, 0.0, 0.0});
    EXPECT_THROW(static_cast<void>(zeroSelector.select()), std::runtime_error);
}

TEST_F(WeightSelectorTest, PrintWeights) {
    {
        std::vector<double> weights = {1.0, 2.5, 3.75};
        WeightSelector<double> selector(weights);
        std::stringstream ss;
        selector.printWeights(ss);
#ifdef ATOM_USE_BOOST
        EXPECT_FALSE(ss.str().empty());
#else
        EXPECT_EQ(ss.str(), "[1.00, 2.50, 3.75]\n");
#endif
    }
    {
        std::vector<double> emptyWeights;
        WeightSelector<double> selector(emptyWeights);
        std::stringstream ss;
        selector.printWeights(ss);
        EXPECT_EQ(ss.str(), "[]\n");
    }
}

TEST_F(WeightSelectorTest, IntegerWeights) {
    std::vector<int> weights = {1, 2, 3, 4};
    WeightSelector<int> selector(weights);
    EXPECT_EQ(selector.size(), weights.size());
    EXPECT_EQ(selector.getTotalWeight(), 10);
    EXPECT_EQ(selector.getMaxWeightIndex(), 3);
    size_t selectedIndex = selector.select();
    EXPECT_LT(selectedIndex, weights.size());
}

TEST_F(WeightSelectorTest, ChangeStrategy) {
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0};
    WeightSelector<double> selector(weights);
    std::map<size_t, int> defaultCounts;
    for (size_t i = 0; i < 1000; ++i) {
        defaultCounts[selector.select()]++;
    }
    auto randomStrategy =
        std::make_unique<WeightSelector<double>::RandomSelectionStrategy>(
            weights.size());
    selector.setSelectionStrategy(std::move(randomStrategy));
    std::map<size_t, int> randomCounts;
    for (size_t i = 0; i < 1000; ++i) {
        randomCounts[selector.select()]++;
    }
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
    double diffRatio = totalRandomDiff / totalDefaultDiff;
    EXPECT_TRUE(diffRatio < 0.5 || diffRatio > 1.5);
}

TEST_F(WeightSelectorTest, EdgeCaseWeights) {
    {
        std::vector<double> largeWeights = {1e9, 2e9, 3e9};
        WeightSelector<double> selector(largeWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 6e9, 1e-5);
        size_t selectedIndex = selector.select();
        EXPECT_LT(selectedIndex, largeWeights.size());
    }
    {
        std::vector<double> smallWeights = {1e-9, 2e-9, 3e-9};
        WeightSelector<double> selector(smallWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 6e-9, 1e-15);
        size_t selectedIndex = selector.select();
        EXPECT_LT(selectedIndex, smallWeights.size());
    }
    {
        std::vector<double> mixedWeights = {1e-9, 1.0, 1e9};
        WeightSelector<double> selector(mixedWeights);
        EXPECT_NEAR(selector.getTotalWeight(), 1e9 + 1.0 + 1e-9, 1e-5);
        const size_t NUM_SELECTIONS = 1000;
        std::map<size_t, int> counts;
        for (size_t i = 0; i < NUM_SELECTIONS; ++i) {
            counts[selector.select()]++;
        }
        EXPECT_GT(counts[2], NUM_SELECTIONS * 0.99);
    }
}
