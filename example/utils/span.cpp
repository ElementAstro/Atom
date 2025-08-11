/*
 * span_example.cpp
 *
 * Copyright (C) 2024 Max Q.
 *
 * Example usage of the atom::utils span utilities
 */

#include "atom/utils/span.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <iomanip>
#include <iostream>
#include <span>
#include <vector>

// Helper function to print vector contents
template <typename T>
void printVector(const std::vector<T>& data, const std::string& label) {
    std::cout << label << ": ";
    fmt::print("[{}]\n", fmt::join(data, ", "));
}

// Helper for optional output
template <typename T>
void printOptional(const std::optional<T>& opt, const std::string& label) {
    std::cout << label << ": ";
    if (opt.has_value()) {
        std::cout << *opt << std::endl;
    } else {
        std::cout << "No value" << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Span Utilities Examples ===\n\n";

    // Create data for examples
    std::vector<int> numbers = {7, 2, 5, 1, 9, 3, 8, 6, 4, 5, 7, 2};
    std::vector<double> measurements = {1.2, 3.5, 2.7, 4.1, 5.6,
                                        2.7, 1.8, 3.9, 4.2};
    std::vector<int> binary = {0, 1, 0, 0, 1, 1, 0, 1, 0, 1};

    std::cout << "Example data sets:\n";
    printVector(numbers, "Numbers");
    printVector(measurements, "Measurements");
    printVector(binary, "Binary values");
    std::cout << std::endl;

    // Create spans from our vectors
    std::span<int> numbersSpan(numbers);
    std::span<double> measurementsSpan(measurements);
    std::span<int> binarySpan(binary);

    std::cout << "Example 1: Basic Statistical Functions\n";

    // Sum
    int numSum = atom::utils::sum(numbersSpan);
    std::cout << "Sum of numbers: " << numSum << std::endl;

    // Mean
    double numMean = atom::utils::mean(numbersSpan);
    std::cout << "Mean of numbers: " << numMean << std::endl;

    // Median
    double numMedian = atom::utils::median(numbersSpan);
    std::cout << "Median of numbers: " << numMedian << std::endl;

    // Mode
    int numMode = atom::utils::mode(numbersSpan);
    std::cout << "Mode of numbers: " << numMode << std::endl;

    // Variance
    double numVariance = atom::utils::variance(numbersSpan);
    std::cout << "Variance of numbers: " << numVariance << std::endl;

    // Standard Deviation
    double numStdDev = atom::utils::standardDeviation(numbersSpan);
    std::cout << "Standard deviation of numbers: " << numStdDev << std::endl;
    std::cout << std::endl;

    std::cout << "Example 2: Element Finding Functions\n";

    // Contains
    bool hasValue = atom::utils::contains(numbersSpan, 5);
    std::cout << "Numbers contains 5: " << (hasValue ? "Yes" : "No")
              << std::endl;

    // Find index
    auto index = atom::utils::findIndex(numbersSpan, 9);
    printOptional(index, "Index of value 9 in numbers");

    auto notFoundIndex = atom::utils::findIndex(numbersSpan, 42);
    printOptional(notFoundIndex, "Index of value 42 in numbers");

    // Min element
    int minElement = atom::utils::minElementSpan(numbersSpan);
    std::cout << "Minimum element in numbers: " << minElement << std::endl;

    // Max element
    int maxElement = atom::utils::maxElementSpan(numbersSpan);
    std::cout << "Maximum element in numbers: " << maxElement << std::endl;

    // Max element index
    size_t maxIndex = atom::utils::maxElementIndex(numbersSpan);
    std::cout << "Index of maximum element in numbers: " << maxIndex
              << std::endl;
    std::cout << std::endl;

    std::cout << "Example 3: Top/Bottom N Elements\n";

    // Top N elements
    auto topThree = atom::utils::topNElements(numbersSpan, 3);
    printVector(topThree, "Top 3 elements from numbers");

    // Bottom N elements
    auto bottomFour = atom::utils::bottomNElements(numbersSpan, 4);
    printVector(bottomFour, "Bottom 4 elements from numbers");
    std::cout << std::endl;

    std::cout << "Example 4: Filtering and Counting\n";

    // Filter span
    auto evenNumbers =
        atom::utils::filterSpan(numbersSpan, [](int n) { return n % 2 == 0; });
    printVector(evenNumbers, "Even numbers from the span");

    // Count if
    size_t oddCount =
        atom::utils::countIfSpan(numbersSpan, [](int n) { return n % 2 != 0; });
    std::cout << "Count of odd numbers in the span: " << oddCount << std::endl;

    // Count ones in binary data
    size_t onesCount =
        atom::utils::countIfSpan(binarySpan, [](int n) { return n == 1; });
    std::cout << "Count of ones in binary data: " << onesCount << std::endl;
    std::cout << std::endl;

    std::cout << "Example 5: Cumulative Operations\n";

    // Cumulative sum
    auto cumulSum = atom::utils::cumulativeSum(numbersSpan);
    printVector(cumulSum, "Cumulative sum of numbers");

    // Cumulative product
    auto cumulProd = atom::utils::cumulativeProduct(
        std::span<int>(numbers.data(), 5));  // Using first 5 elements only
    printVector(cumulProd, "Cumulative product of first 5 numbers");
    std::cout << std::endl;

    std::cout << "Example 6: Sorting and Normalizing\n";

    // Create a copy of the numbers vector for sorting
    std::vector<int> sortableNumbers = numbers;
    std::span<int> sortableSpan(sortableNumbers);

    // Sort span in-place
    atom::utils::sortSpan(sortableSpan);
    printVector(sortableNumbers, "Numbers after sorting");

    // Create a copy for normalization
    std::vector<double> normalizableData = {23.5, 12.7, 45.1, 18.3, 33.9, 27.6};
    printVector(normalizableData, "Original data for normalization");

    std::span<double> normalizableSpan(normalizableData);

    // Normalize the data
    atom::utils::normalize(normalizableSpan);
    printVector(normalizableData, "Normalized data (0-1 range)");
    std::cout << std::endl;

    std::cout << "Example 7: Matrix Operations\n";

    // Create a simple 3x3 matrix stored in row-major order
    std::vector<int> matrix = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    std::cout << "Original 3x3 matrix (row-major):" << std::endl;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            std::cout << std::setw(3) << matrix[i * 3 + j] << " ";
        }
        std::cout << std::endl;
    }

    // Transpose the matrix in-place
    std::span<int> matrixSpan(matrix);
    atom::utils::transposeMatrix(matrixSpan, 3, 3);

    std::cout << "Transposed matrix:" << std::endl;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            std::cout << std::setw(3) << matrix[i * 3 + j] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Example 8: Real-world Applications\n";

    // Scenario: Analyze student test scores
    std::vector<double> testScores = {78.5, 92.0, 65.5, 87.5, 90.0, 81.5,
                                      73.0, 88.5, 95.5, 76.0, 82.5, 91.0};
    std::span<double> scoresSpan(testScores);

    std::cout << "Analysis of Student Test Scores:" << std::endl;
    std::cout << "Number of students: " << scoresSpan.size() << std::endl;
    std::cout << "Average score: " << atom::utils::mean(scoresSpan)
              << std::endl;
    std::cout << "Median score: " << atom::utils::median(scoresSpan)
              << std::endl;
    std::cout << "Highest score: " << atom::utils::maxElementSpan(scoresSpan)
              << std::endl;
    std::cout << "Lowest score: " << atom::utils::minElementSpan(scoresSpan)
              << std::endl;
    std::cout << "Standard deviation: "
              << atom::utils::standardDeviation(scoresSpan) << std::endl;

    // Count students who passed (score >= 70)
    size_t passedCount = atom::utils::countIfSpan(
        scoresSpan, [](double score) { return score >= 70.0; });
    std::cout << "Number of students who passed: " << passedCount << std::endl;

    // Get the top 3 scores
    auto topScores = atom::utils::topNElements(scoresSpan, 3);
    printVector(topScores, "Top 3 scores");

    // Scenario: Price movement analysis
    std::vector<double> stockPrices = {145.2, 146.8, 145.5, 147.3, 149.5, 148.7,
                                       151.2, 153.4, 152.8, 154.1, 153.5};
    std::span<double> pricesSpan(stockPrices);

    // Calculate daily price changes
    std::vector<double> priceChanges(stockPrices.size() - 1);
    for (size_t i = 0; i < priceChanges.size(); ++i) {
        priceChanges[i] = stockPrices[i + 1] - stockPrices[i];
    }
    std::span<double> changesSpan(priceChanges);

    std::cout << "\nStock Price Movement Analysis:" << std::endl;
    printVector(stockPrices, "Stock prices");
    printVector(priceChanges, "Daily price changes");

    std::cout << "Average daily change: " << atom::utils::mean(changesSpan)
              << std::endl;
    std::cout << "Largest daily gain: "
              << atom::utils::maxElementSpan(changesSpan) << std::endl;
    std::cout << "Largest daily loss: "
              << atom::utils::minElementSpan(changesSpan) << std::endl;

    // Count positive days
    size_t positiveDays = atom::utils::countIfSpan(
        changesSpan, [](double change) { return change > 0.0; });
    std::cout << "Number of days with price increase: " << positiveDays
              << std::endl;
    std::cout << "Number of days with price decrease: "
              << (changesSpan.size() - positiveDays) << std::endl;

    // Volatility (standard deviation of price changes)
    std::cout << "Price volatility (std dev of changes): "
              << atom::utils::standardDeviation(changesSpan) << std::endl;

    return 0;
}
