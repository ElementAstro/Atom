/**
 * @file span_example.cpp
 * @brief Comprehensive examples for atom::utils span utilities
 *
 * This example demonstrates all span utility functions including:
 * - Basic operations (sum, contains, sortSpan, filterSpan, countIfSpan)
 * - Min/Max operations (minElementSpan, maxElementSpan, minElementIndex, maxElementIndex)
 * - Statistical functions (mean, median, mode, standardDeviation, variance)
 * - Top/Bottom N elements (topNElements, bottomNElements)
 * - Cumulative operations (cumulativeSum, cumulativeProduct)
 * - Search and predicates (findIndex, allOf, anyOf, noneOf)
 * - Matrix operations (transposeMatrix, normalize)
 * - Vector operations (dotProduct)
 */

#include "atom/utils/container/span.hpp"

#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

template <typename T>
void printVector(const std::string& label, const std::vector<T>& vec) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : vec) {
        if (!first) std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

template <typename T>
void printSpan(const std::string& label, std::span<const T> sp) {
    std::cout << label << ": [";
    bool first = true;
    for (const auto& item : sp) {
        if (!first) std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// ============================================
// 1. Basic Span Operations
// ============================================
void demonstrateBasicOperations() {
    printSection("1. Basic Span Operations");

    std::vector<int> data = {5, 2, 8, 1, 9, 3, 7, 4, 6};
    std::span<const int> dataSpan(data);
    printSpan("Original data", dataSpan);

    // sum - Calculate sum of elements
    std::cout << "\n--- sum ---" << std::endl;
    auto total = sum(dataSpan);
    std::cout << "Sum of all elements: " << total << std::endl;

    // contains - Check if span contains a value
    std::cout << "\n--- contains ---" << std::endl;
    std::cout << "Contains 5? " << (contains(dataSpan, 5) ? "Yes" : "No")
              << std::endl;
    std::cout << "Contains 100? " << (contains(dataSpan, 100) ? "Yes" : "No")
              << std::endl;

    // sortSpan - Sort elements in-place
    std::cout << "\n--- sortSpan ---" << std::endl;
    std::vector<int> toSort = {5, 2, 8, 1, 9, 3, 7, 4, 6};
    printVector("Before sorting", toSort);
    sortSpan(std::span<int>(toSort));
    printVector("After sorting", toSort);

    // filterSpan - Filter elements based on predicate
    std::cout << "\n--- filterSpan ---" << std::endl;
    auto evenNumbers =
        filterSpan(dataSpan, [](int x) { return x % 2 == 0; });
    printVector("Even numbers", evenNumbers);

    auto greaterThan5 =
        filterSpan(dataSpan, [](int x) { return x > 5; });
    printVector("Numbers > 5", greaterThan5);

    // countIfSpan - Count elements matching predicate
    std::cout << "\n--- countIfSpan ---" << std::endl;
    auto evenCount =
        countIfSpan(dataSpan, [](int x) { return x % 2 == 0; });
    std::cout << "Count of even numbers: " << evenCount << std::endl;

    auto positiveCount =
        countIfSpan(dataSpan, [](int x) { return x > 0; });
    std::cout << "Count of positive numbers: " << positiveCount << std::endl;
}

// ============================================
// 2. Min/Max Operations
// ============================================
void demonstrateMinMaxOperations() {
    printSection("2. Min/Max Operations");

    std::vector<int> data = {15, 3, 27, 8, 42, 11, 5, 33, 19};
    std::span<const int> dataSpan(data);
    printSpan("Data", dataSpan);

    // minElementSpan - Find minimum element
    std::cout << "\n--- minElementSpan ---" << std::endl;
    auto minVal = minElementSpan(dataSpan);
    std::cout << "Minimum element: " << minVal << std::endl;

    // maxElementSpan - Find maximum element
    std::cout << "\n--- maxElementSpan ---" << std::endl;
    auto maxVal = maxElementSpan(dataSpan);
    std::cout << "Maximum element: " << maxVal << std::endl;

    // minElementIndex - Find index of minimum element
    std::cout << "\n--- minElementIndex ---" << std::endl;
    auto minIdx = minElementIndex(dataSpan);
    std::cout << "Index of minimum element: " << minIdx << " (value: "
              << data[minIdx] << ")" << std::endl;

    // maxElementIndex - Find index of maximum element
    std::cout << "\n--- maxElementIndex ---" << std::endl;
    auto maxIdx = maxElementIndex(dataSpan);
    std::cout << "Index of maximum element: " << maxIdx << " (value: "
              << data[maxIdx] << ")" << std::endl;
}

// ============================================
// 3. Statistical Functions
// ============================================
void demonstrateStatisticalFunctions() {
    printSection("3. Statistical Functions");

    std::vector<double> data = {12.5, 15.3, 11.8, 14.2, 13.5, 16.1, 10.9, 15.7};
    std::span<const double> dataSpan(data);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Data: [";
    bool first = true;
    for (auto val : data) {
        if (!first) std::cout << ", ";
        std::cout << val;
        first = false;
    }
    std::cout << "]" << std::endl;

    // mean - Calculate arithmetic mean
    std::cout << "\n--- mean ---" << std::endl;
    auto meanVal = mean(dataSpan);
    std::cout << "Mean: " << meanVal << std::endl;

    // median - Find median value
    std::cout << "\n--- median ---" << std::endl;
    auto medianVal = median(dataSpan);
    std::cout << "Median: " << medianVal << std::endl;

    // mode - Find most frequent value
    std::cout << "\n--- mode ---" << std::endl;
    std::vector<int> modeData = {1, 2, 2, 3, 3, 3, 4, 4, 5};
    std::span<const int> modeSpan(modeData);
    printSpan("Mode data", modeSpan);
    auto modeVal = mode(modeSpan);
    std::cout << "Mode: " << modeVal << std::endl;

    // variance - Calculate variance
    std::cout << "\n--- variance ---" << std::endl;
    auto varianceVal = variance(dataSpan);
    std::cout << "Variance: " << varianceVal << std::endl;

    // standardDeviation - Calculate standard deviation
    std::cout << "\n--- standardDeviation ---" << std::endl;
    auto stdDev = standardDeviation(dataSpan);
    std::cout << "Standard Deviation: " << stdDev << std::endl;
}

// ============================================
// 4. Top/Bottom N Elements
// ============================================
void demonstrateTopBottomN() {
    printSection("4. Top/Bottom N Elements");

    std::vector<int> data = {45, 12, 78, 34, 56, 23, 89, 67, 11, 90};
    std::span<const int> dataSpan(data);
    printSpan("Data", dataSpan);

    // topNElements - Get top N largest elements
    std::cout << "\n--- topNElements ---" << std::endl;
    auto top3 = topNElements(dataSpan, 3);
    printVector("Top 3 elements", top3);

    auto top5 = topNElements(dataSpan, 5);
    printVector("Top 5 elements", top5);

    // bottomNElements - Get bottom N smallest elements
    std::cout << "\n--- bottomNElements ---" << std::endl;
    auto bottom3 = bottomNElements(dataSpan, 3);
    printVector("Bottom 3 elements", bottom3);

    auto bottom5 = bottomNElements(dataSpan, 5);
    printVector("Bottom 5 elements", bottom5);
}

// ============================================
// 5. Cumulative Operations
// ============================================
void demonstrateCumulativeOperations() {
    printSection("5. Cumulative Operations");

    std::vector<int> data = {1, 2, 3, 4, 5};
    std::span<const int> dataSpan(data);
    printSpan("Data", dataSpan);

    // cumulativeSum - Running sum
    std::cout << "\n--- cumulativeSum ---" << std::endl;
    auto cumSum = cumulativeSum(dataSpan);
    printVector("Cumulative sum", cumSum);
    std::cout << "  (1, 1+2=3, 3+3=6, 6+4=10, 10+5=15)" << std::endl;

    // cumulativeProduct - Running product
    std::cout << "\n--- cumulativeProduct ---" << std::endl;
    auto cumProd = cumulativeProduct(dataSpan);
    printVector("Cumulative product", cumProd);
    std::cout << "  (1, 1*2=2, 2*3=6, 6*4=24, 24*5=120)" << std::endl;
}

// ============================================
// 6. Search and Predicates
// ============================================
void demonstrateSearchAndPredicates() {
    printSection("6. Search and Predicates");

    std::vector<int> data = {2, 4, 6, 8, 10, 12, 14, 16};
    std::span<const int> dataSpan(data);
    printSpan("Data (all even)", dataSpan);

    // findIndex - Find index of a value
    std::cout << "\n--- findIndex ---" << std::endl;
    auto idx8 = findIndex(dataSpan, 8);
    if (idx8) {
        std::cout << "Index of 8: " << *idx8 << std::endl;
    }

    auto idx7 = findIndex(dataSpan, 7);
    if (!idx7) {
        std::cout << "7 not found in the span" << std::endl;
    }

    // allOf - Check if all elements satisfy predicate
    std::cout << "\n--- allOf ---" << std::endl;
    bool allEven = allOf(dataSpan, [](int x) { return x % 2 == 0; });
    std::cout << "All elements are even? " << (allEven ? "Yes" : "No")
              << std::endl;

    bool allPositive = allOf(dataSpan, [](int x) { return x > 0; });
    std::cout << "All elements are positive? " << (allPositive ? "Yes" : "No")
              << std::endl;

    bool allGreater10 = allOf(dataSpan, [](int x) { return x > 10; });
    std::cout << "All elements > 10? " << (allGreater10 ? "Yes" : "No")
              << std::endl;

    // anyOf - Check if any element satisfies predicate
    std::cout << "\n--- anyOf ---" << std::endl;
    bool anyGreater10 = anyOf(dataSpan, [](int x) { return x > 10; });
    std::cout << "Any element > 10? " << (anyGreater10 ? "Yes" : "No")
              << std::endl;

    bool anyNegative = anyOf(dataSpan, [](int x) { return x < 0; });
    std::cout << "Any negative element? " << (anyNegative ? "Yes" : "No")
              << std::endl;

    // noneOf - Check if no elements satisfy predicate
    std::cout << "\n--- noneOf ---" << std::endl;
    bool noneNegative = noneOf(dataSpan, [](int x) { return x < 0; });
    std::cout << "No negative elements? " << (noneNegative ? "Yes" : "No")
              << std::endl;

    bool noneOdd = noneOf(dataSpan, [](int x) { return x % 2 != 0; });
    std::cout << "No odd elements? " << (noneOdd ? "Yes" : "No") << std::endl;
}

// ============================================
// 7. Matrix and Normalization Operations
// ============================================
void demonstrateMatrixOperations() {
    printSection("7. Matrix and Normalization Operations");

    // transposeMatrix - Transpose a matrix
    std::cout << "--- transposeMatrix ---" << std::endl;
    std::vector<int> matrix = {1, 2, 3, 4, 5, 6};  // 2x3 matrix
    std::cout << "Original 2x3 matrix:" << std::endl;
    std::cout << "  [1, 2, 3]" << std::endl;
    std::cout << "  [4, 5, 6]" << std::endl;

    transposeMatrix(std::span<int>(matrix), 2, 3);

    std::cout << "Transposed 3x2 matrix:" << std::endl;
    std::cout << "  [" << matrix[0] << ", " << matrix[1] << "]" << std::endl;
    std::cout << "  [" << matrix[2] << ", " << matrix[3] << "]" << std::endl;
    std::cout << "  [" << matrix[4] << ", " << matrix[5] << "]" << std::endl;

    // normalize - Normalize values to [0, 1]
    std::cout << "\n--- normalize ---" << std::endl;
    std::vector<double> values = {10.0, 20.0, 30.0, 40.0, 50.0};
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Before normalization: [";
    bool first = true;
    for (auto v : values) {
        if (!first) std::cout << ", ";
        std::cout << v;
        first = false;
    }
    std::cout << "]" << std::endl;

    normalize(std::span<double>(values));

    std::cout << "After normalization:  [";
    first = true;
    for (auto v : values) {
        if (!first) std::cout << ", ";
        std::cout << v;
        first = false;
    }
    std::cout << "]" << std::endl;
    std::cout << "  (min->0.0, max->1.0)" << std::endl;
}

// ============================================
// 8. Vector Operations
// ============================================
void demonstrateVectorOperations() {
    printSection("8. Vector Operations");

    // dotProduct - Calculate dot product of two vectors
    std::cout << "--- dotProduct ---" << std::endl;
    std::vector<double> vec1 = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> vec2 = {5.0, 6.0, 7.0, 8.0};

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Vector 1: [";
    bool first = true;
    for (auto v : vec1) {
        if (!first) std::cout << ", ";
        std::cout << v;
        first = false;
    }
    std::cout << "]" << std::endl;

    std::cout << "Vector 2: [";
    first = true;
    for (auto v : vec2) {
        if (!first) std::cout << ", ";
        std::cout << v;
        first = false;
    }
    std::cout << "]" << std::endl;

    auto dot = dotProduct(std::span<const double>(vec1),
                          std::span<const double>(vec2));
    std::cout << "Dot product: " << dot << std::endl;
    std::cout << "  (1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70)"
              << std::endl;
}

// ============================================
// 9. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("9. Complex Use Cases");

    // Use case 1: Statistical analysis of sensor data
    std::cout << "--- Sensor Data Analysis ---" << std::endl;
    std::vector<double> sensorReadings = {23.5, 24.1, 23.8, 25.2, 24.5,
                                          23.9, 24.8, 25.0, 24.3, 23.7};
    std::span<const double> readings(sensorReadings);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Sensor readings: [";
    bool first = true;
    for (auto r : sensorReadings) {
        if (!first) std::cout << ", ";
        std::cout << r;
        first = false;
    }
    std::cout << "]" << std::endl;

    std::cout << "\nStatistical Summary:" << std::endl;
    std::cout << "  Count: " << sensorReadings.size() << std::endl;
    std::cout << "  Sum: " << sum(readings) << std::endl;
    std::cout << "  Mean: " << mean(readings) << std::endl;
    std::cout << "  Median: " << median(readings) << std::endl;
    std::cout << "  Min: " << minElementSpan(readings) << std::endl;
    std::cout << "  Max: " << maxElementSpan(readings) << std::endl;
    std::cout << "  Std Dev: " << standardDeviation(readings) << std::endl;
    std::cout << "  Variance: " << variance(readings) << std::endl;

    // Use case 2: Outlier detection
    std::cout << "\n--- Outlier Detection ---" << std::endl;
    std::vector<int> measurements = {100, 102, 98, 101, 500, 99, 103, 97, 101};
    std::span<const int> measSpan(measurements);
    printSpan("Measurements", measSpan);

    double meanVal = mean(measSpan);
    double stdDevVal = standardDeviation(measSpan);
    double threshold = 2.0;  // 2 standard deviations

    std::cout << "Mean: " << meanVal << ", Std Dev: " << stdDevVal << std::endl;
    std::cout << "Outliers (beyond " << threshold << " std devs):" << std::endl;

    for (size_t i = 0; i < measurements.size(); ++i) {
        double zScore = std::abs(measurements[i] - meanVal) / stdDevVal;
        if (zScore > threshold) {
            std::cout << "  Index " << i << ": value " << measurements[i]
                      << " (z-score: " << std::setprecision(2) << zScore << ")"
                      << std::endl;
        }
    }

    // Use case 3: Performance ranking
    std::cout << "\n--- Performance Ranking ---" << std::endl;
    std::vector<int> scores = {85, 92, 78, 95, 88, 91, 76, 89, 94, 82};
    std::span<const int> scoresSpan(scores);
    printSpan("Scores", scoresSpan);

    auto top3Scores = topNElements(scoresSpan, 3);
    auto bottom3Scores = bottomNElements(scoresSpan, 3);

    printVector("Top 3 performers", top3Scores);
    printVector("Bottom 3 performers", bottom3Scores);
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Span Utilities Examples" << std::endl;
    std::cout << "  atom::utils::span" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicOperations();
        demonstrateMinMaxOperations();
        demonstrateStatisticalFunctions();
        demonstrateTopBottomN();
        demonstrateCumulativeOperations();
        demonstrateSearchAndPredicates();
        demonstrateMatrixOperations();
        demonstrateVectorOperations();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All span examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
