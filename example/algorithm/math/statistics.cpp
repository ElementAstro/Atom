/*
 * statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating statistics from atom/algorithm/math/statistics.hpp
 */

#include "atom/algorithm/math/statistics.hpp"

#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;

// Helper function to print vectortemplate <typename T>
void printVector(const std::vector<T>& vec, const std::string& name) {
    std::cout << name << ": [";
    for (usize i = 0; i < vec.size(); ++i) {
        std::cout << std::fixed << std::setprecision(2) << vec[i];
        if (i < vec.size() - 1)
            std::cout << ", ";
    }
    std::cout << "]\n";
}

// Generate random data with normal distributionstd::vector<f64>
// generateNormalData(usize size, f64 mean, f64 stddev) {
std::random_device rd;
std::mt19937 gen(rd());
std::normal_distribution<f64> dis(mean, stddev);

std::vector<f64> data(size);
for (auto& val : data) {
    val = dis(gen);
}
return data;
}

// Generate random data with uniform distributionstd::vector<f64>
// generateUniformData(usize size, f64 min_val, f64 max_val) {
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<f64> dis(min_val, max_val);

std::vector<f64> data(size);
for (auto& val : data) {
    val = dis(gen);
}
return data;
}

// Demonstrate basic descriptive statisticsvoid demonstrateDescriptiveStats() {
std::cout << "\n=== Descriptive Statistics ===\n";

using Stats = Statistics<f64>;

// Sample data
std::vector<f64> data = {2.5, 3.7, 4.2, 5.1, 6.3, 7.8, 8.4, 9.1, 10.2, 11.5};
printVector(data, "Data");

std::cout << "\nStatistics:\n";
std::cout << "  Mean: " << std::fixed << std::setprecision(4)
          << Stats::mean(data) << "\n";
std::cout << "  Median: " << Stats::median(data) << "\n";
std::cout << "  Variance (sample): " << Stats::variance(data, true) << "\n";
std::cout << "  Variance (population): " << Stats::variance(data, false)
          << "\n";
std::cout << "  Std Dev (sample): " << Stats::standardDeviation(data, true)
          << "\n";
std::cout << "  Std Dev (population): " << Stats::standardDeviation(data, false)
          << "\n";

// Mode
std::vector<f64> data_with_mode = {1, 2, 2, 3, 3, 3, 4, 4, 5};
printVector(data_with_mode, "\nData with mode");
auto modes = Stats::mode(data_with_mode);
std::cout << "  Mode(s): ";
for (auto m : modes)
    std::cout << m << " ";
std::cout << "\n";
}

// Demonstrate skewness and kurtosisvoid demonstrateSkewnessKurtosis() {
std::cout << "\n=== Skewness and Kurtosis ===\n";

using Stats = Statistics<f64>;

// Symmetric data (low skewness)
std::vector<f64> symmetric = {1, 2, 3, 4, 5, 6, 7, 8, 9};
printVector(symmetric, "Symmetric data");
std::cout << "  Skewness: " << std::fixed << std::setprecision(4)
          << Stats::skewness(symmetric) << "\n";
std::cout << "  Kurtosis: " << Stats::kurtosis(symmetric) << "\n";

// Right-skewed data
std::vector<f64> right_skewed = {1, 1, 2, 2, 2, 3, 3, 4, 5, 10, 15, 20};
printVector(right_skewed, "\nRight-skewed data");
std::cout << "  Skewness: " << Stats::skewness(right_skewed) << "\n";
std::cout << "  Kurtosis: " << Stats::kurtosis(right_skewed) << "\n";

// Left-skewed data
std::vector<f64> left_skewed = {1, 5, 10, 15, 18, 18, 19, 19, 19, 20, 20};
printVector(left_skewed, "\nLeft-skewed data");
std::cout << "  Skewness: " << Stats::skewness(left_skewed) << "\n";
std::cout << "  Kurtosis: " << Stats::kurtosis(left_skewed) << "\n";
}

// Demonstrate correlation and covariancevoid demonstrateCorrelation() {
std::cout << "\n=== Correlation and Covariance ===\n";

using Stats = Statistics<f64>;

// Perfectly correlated data
std::vector<f64> x1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
std::vector<f64> y1 = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};  // y = 2x

std::cout << "Perfect positive correlation (y = 2x):\n";
printVector(x1, "  X");
printVector(y1, "  Y");
std::cout << "  Covariance: " << std::fixed << std::setprecision(4)
          << Stats::covariance(x1, y1) << "\n";
std::cout << "  Correlation: " << Stats::correlation(x1, y1) << "\n";

// Negative correlation
std::vector<f64> y2 = {20, 18, 16, 14, 12, 10, 8, 6, 4, 2};  // y = 22 - 2x

std::cout << "\nPerfect negative correlation (y = 22 - 2x):\n";
printVector(y2, "  Y");
std::cout << "  Covariance: " << Stats::covariance(x1, y2) << "\n";
std::cout << "  Correlation: " << Stats::correlation(x1, y2) << "\n";

// No correlation (random)
std::vector<f64> y3 = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};

std::cout << "\nWeak/no correlation (random Y):\n";
printVector(y3, "  Y");
std::cout << "  Covariance: " << Stats::covariance(x1, y3) << "\n";
std::cout << "  Correlation: " << Stats::correlation(x1, y3) << "\n";
}

// Demonstrate percentiles and quartilesvoid demonstratePercentilesQuartiles() {
std::cout << "\n=== Percentiles and Quartiles ===\n";

using Stats = Statistics<f64>;

std::vector<f64> data = {1,  5,  10, 15, 20, 25, 30, 35, 40, 45,
                         50, 55, 60, 65, 70, 75, 80, 85, 90, 95};
printVector(data, "Data");

std::cout << "\nPercentiles:\n";
for (f64 p : {10.0, 25.0, 50.0, 75.0, 90.0}) {
    std::cout << "  P" << static_cast<int>(p) << ": " << std::fixed
              << std::setprecision(2) << Stats::percentile(data, p) << "\n";
}

std::cout << "\nQuartiles:\n";
std::cout << "  Q1 (25th): " << Stats::percentile(data, 25.0) << "\n";
std::cout << "  Q2 (50th/Median): " << Stats::percentile(data, 50.0) << "\n";
std::cout << "  Q3 (75th): " << Stats::percentile(data, 75.0) << "\n";
std::cout << "  IQR: "
          << (Stats::percentile(data, 75.0) - Stats::percentile(data, 25.0))
          << "\n";
}

// Demonstrate range and min/maxvoid demonstrateRangeMinMax() {
std::cout << "\n=== Range, Min, Max ===\n";

using Stats = Statistics<f64>;

std::vector<f64> data = {-5.5, 2.3, 8.7, -1.2, 15.4, 3.3, -8.9, 12.1};
printVector(data, "Data");

// Use std::ranges for min/max since Statistics class doesn't have these
auto [min_it, max_it] = std::ranges::minmax_element(data);
f64 min_val = *min_it;
f64 max_val = *max_it;
f64 range_val = max_val - min_val;

std::cout << "\nStatistics:\n";
std::cout << "  Min: " << std::fixed << std::setprecision(2) << min_val << "\n";
std::cout << "  Max: " << max_val << "\n";
std::cout << "  Range: " << range_val << "\n";
}

// Demonstrate z-score normalizationvoid demonstrateZScore() {
std::cout << "\n=== Z-Score Normalization ===\n";

using Stats = Statistics<f64>;

std::vector<f64> data = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
printVector(data, "Original data");

f64 mean = Stats::mean(data);
f64 stddev = Stats::standardDeviation(data);

std::cout << "Mean: " << mean << ", Std Dev: " << stddev << "\n";

auto z_scores = Stats::zScores(data);
printVector(z_scores, "Z-scores");

// Verify z-scores have mean ~0 and std dev ~1
std::cout << "Z-score mean: " << Stats::mean(z_scores) << "\n";
std::cout << "Z-score std dev: " << Stats::standardDeviation(z_scores) << "\n";
}

// Demonstrate with larger datasetvoid demonstrateLargeDataset() {
std::cout << "\n=== Large Dataset Analysis ===\n";

using Stats = Statistics<f64>;

// Generate large normal distribution
auto data = generateNormalData(10000, 50.0, 15.0);

std::cout << "Generated 10,000 samples from N(50, 15):\n";
std::cout << "  Sample mean: " << std::fixed << std::setprecision(4)
          << Stats::mean(data) << " (expected: 50)\n";
std::cout << "  Sample std dev: " << Stats::standardDeviation(data)
          << " (expected: 15)\n";
std::cout << "  Sample median: " << Stats::median(data) << " (expected: ~50)\n";
std::cout << "  Sample skewness: " << Stats::skewness(data)
          << " (expected: ~0)\n";
std::cout << "  Sample kurtosis: " << Stats::kurtosis(data)
          << " (expected: ~3 for normal)\n";
}

// Benchmark statistics operationsvoid benchmarkStatistics() {
std::cout << "\n=== Statistics Benchmark ===\n";

using Stats = Statistics<f64>;

auto data = generateUniformData(100000, 0.0, 100.0);
std::cout << "Dataset size: " << data.size() << " elements\n\n";

// Benchmark mean
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 100; ++i) {
    volatile f64 m = Stats::mean(data);
    (void)m;
}
auto end = std::chrono::high_resolution_clock::now();
auto duration =
    std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Mean (100 iterations): " << duration.count() << " us\n";

// Benchmark variance
start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 100; ++i) {
    volatile f64 v = Stats::variance(data);
    (void)v;
}
end = std::chrono::high_resolution_clock::now();
duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Variance (100 iterations): " << duration.count() << " us\n";

// Benchmark median (involves sorting)
start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 10; ++i) {
    volatile f64 med = Stats::median(data);
    (void)med;
}
end = std::chrono::high_resolution_clock::now();
duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Median (10 iterations): " << duration.count() << " us\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Statistics Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateDescriptiveStats();
        demonstrateSkewnessKurtosis();
        demonstrateCorrelation();
        demonstratePercentilesQuartiles();
        demonstrateRangeMinMax();
        demonstrateZScore();
        demonstrateLargeDataset();
        benchmarkStatistics();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
