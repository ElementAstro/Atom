#include "atom/algorithm/weight.hpp"

#include <iostream>
#include <vector>

int main() {
    using namespace atom::algorithm;

    // Define a vector of weights
    std::vector<double> weights = {1.0, 2.0, 3.0, 4.0, 5.0};

    // Create a WeightSelector with default selection strategy
    WeightSelector<double> selector(weights);

    // Select a single index based on weights
    size_t selectedIndex = selector.select();
    std::cout << "Selected index: " << selectedIndex << std::endl;

    // Select multiple indices based on weights
    std::vector<size_t> selectedIndices = selector.selectMultiple(3);
    std::cout << "Selected indices: ";
    for (size_t index : selectedIndices) {
        std::cout << index << " ";
    }
    std::cout << std::endl;

    // Update a specific weight
    selector.updateWeight(2, 10.0);
    std::cout << "Updated weights: ";
    selector.printWeights(std::cout);

    // Add a new weight
    selector.addWeight(6.0);
    std::cout << "Weights after adding a new weight: ";
    selector.printWeights(std::cout);

    // Remove a weight
    selector.removeWeight(1);
    std::cout << "Weights after removing a weight: ";
    selector.printWeights(std::cout);

    // Normalize weights
    selector.normalizeWeights();
    std::cout << "Normalized weights: ";
    selector.printWeights(std::cout);

    // Apply a function to all weights
    selector.applyFunctionToWeights([](double w) { return w * 2; });
    std::cout << "Weights after applying function: ";
    selector.printWeights(std::cout);

    // Batch update weights
    std::vector<std::pair<size_t, double>> updates = {{0, 1.0}, {2, 2.0}};
    selector.batchUpdateWeights(updates);
    std::cout << "Weights after batch update: ";
    selector.printWeights(std::cout);

    // Get a specific weight
    auto weight = selector.getWeight(2);
    if (weight) {
        std::cout << "Weight at index 2: " << *weight << std::endl;
    } else {
        std::cout << "Weight at index 2 not found." << std::endl;
    }

    // Get the index of the maximum weight
    size_t maxWeightIndex = selector.getMaxWeightIndex();
    std::cout << "Index of maximum weight: " << maxWeightIndex << std::endl;

    // Get the index of the minimum weight
    size_t minWeightIndex = selector.getMinWeightIndex();
    std::cout << "Index of minimum weight: " << minWeightIndex << std::endl;

    // Get the total weight
    double totalWeight = selector.getTotalWeight();
    std::cout << "Total weight: " << totalWeight << std::endl;

    // Reset weights
    std::vector<double> newWeights = {0.5, 1.5, 2.5};
    selector.resetWeights(newWeights);
    std::cout << "Weights after reset: ";
    selector.printWeights(std::cout);

    // Scale weights
    selector.scaleWeights(2.0);
    std::cout << "Weights after scaling: ";
    selector.printWeights(std::cout);

    // Get the average weight
    double averageWeight = selector.getAverageWeight();
    std::cout << "Average weight: " << averageWeight << std::endl;

    // Change selection strategy to BottomHeavySelectionStrategy
    selector.setSelectionStrategy(
        std::make_unique<
            WeightSelector<double>::BottomHeavySelectionStrategy>());
    selectedIndex = selector.select();
    std::cout << "Selected index with BottomHeavySelectionStrategy: "
              << selectedIndex << std::endl;

    // Change selection strategy to TopHeavySelectionStrategy
    selector.setSelectionStrategy(
        std::make_unique<TopHeavySelectionStrategy<double>>());
    selectedIndex = selector.select();
    std::cout << "Selected index with TopHeavySelectionStrategy: "
              << selectedIndex << std::endl;

    // Use WeightedRandomSampler to sample indices
    WeightSelector<double>::WeightedRandomSampler sampler;
    std::vector<size_t> sampledIndices = sampler.sample(weights, 3);
    std::cout << "Sampled indices: ";
    for (size_t index : sampledIndices) {
        std::cout << index << " ";
    }
    std::cout << std::endl;

    return 0;
}
