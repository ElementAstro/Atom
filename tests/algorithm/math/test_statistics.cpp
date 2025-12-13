#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include "atom/algorithm/math/statistics.hpp"

using namespace atom::algorithm;

class StatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    std::vector<double> simple_data = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> even_data = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> single_value = {5.0};
    std::vector<double> empty_data = {};

    std::vector<double> normal_data = {10.1, 10.3, 10.2, 10.4,
                                       10.3, 10.1, 10.5, 10.2};
    std::vector<double> outliers_data = {1.0, 2.0, 3.0, 4.0, 5.0, 100.0, -50.0};
};

TEST_F(StatisticsTest, Mean) {
    EXPECT_DOUBLE_EQ(StatisticsD::mean(simple_data), 3.0);
    EXPECT_DOUBLE_EQ(StatisticsD::mean(even_data), 2.5);
    EXPECT_DOUBLE_EQ(StatisticsD::mean(single_value), 5.0);
    EXPECT_DOUBLE_EQ(StatisticsD::mean(empty_data), 0.0);
}

TEST_F(StatisticsTest, Median) {
    EXPECT_DOUBLE_EQ(StatisticsD::median(simple_data), 3.0);
    EXPECT_DOUBLE_EQ(StatisticsD::median(even_data), 2.5);
    EXPECT_DOUBLE_EQ(StatisticsD::median(single_value), 5.0);
    EXPECT_DOUBLE_EQ(StatisticsD::median(empty_data), 0.0);

    // Test unsorted data
    std::vector<double> unsorted = {5.0, 1.0, 3.0, 2.0, 4.0};
    EXPECT_DOUBLE_EQ(StatisticsD::median(unsorted), 3.0);
}

TEST_F(StatisticsTest, Mode) {
    std::vector<double> multimodal_data = {1.0, 2.0, 2.0, 3.0, 3.0, 4.0};
    auto modes = StatisticsD::mode(multimodal_data);

    // Should contain both 2.0 and 3.0
    EXPECT_EQ(modes.size(), 2);
    EXPECT_TRUE(std::find(modes.begin(), modes.end(), 2.0) != modes.end());
    EXPECT_TRUE(std::find(modes.begin(), modes.end(), 3.0) != modes.end());

    std::vector<double> single_mode_data = {1.0, 2.0, 2.0, 3.0, 4.0};
    modes = StatisticsD::mode(single_mode_data);
    EXPECT_EQ(modes.size(), 1);
    EXPECT_DOUBLE_EQ(modes[0], 2.0);

    std::vector<double> all_unique = {1.0, 2.0, 3.0, 4.0};
    modes = StatisticsD::mode(all_unique);
    EXPECT_EQ(modes.size(), 4);  // All values appear once

    auto empty_modes = StatisticsD::mode(empty_data);
    EXPECT_TRUE(empty_modes.empty());
}

TEST_F(StatisticsTest, Variance) {
    // Population variance (n denominator)
    double variance_pop = StatisticsD::variance(simple_data, false);
    EXPECT_NEAR(variance_pop, 2.0, 1e-10);

    // Sample variance (n-1 denominator)
    double variance_sample = StatisticsD::variance(simple_data, true);
    EXPECT_NEAR(variance_sample, 2.5, 1e-10);

    // Test with single value
    EXPECT_DOUBLE_EQ(StatisticsD::variance(single_value, false), 0.0);
    EXPECT_DOUBLE_EQ(StatisticsD::variance(single_value, true), 0.0);

    // Test with empty data
    EXPECT_DOUBLE_EQ(StatisticsD::variance(empty_data), 0.0);
}

TEST_F(StatisticsTest, StandardDeviation) {
    double std_dev_pop = StatisticsD::standardDeviation(simple_data, false);
    EXPECT_NEAR(std_dev_pop, std::sqrt(2.0), 1e-10);

    double std_dev_sample = StatisticsD::standardDeviation(simple_data, true);
    EXPECT_NEAR(std_dev_sample, std::sqrt(2.5), 1e-10);
}

TEST_F(StatisticsTest, Skewness) {
    // Symmetric data should have skewness close to 0
    std::vector<double> symmetric = {1.0, 2.0, 3.0, 4.0, 5.0};
    double skew = StatisticsD::skewness(symmetric);
    EXPECT_NEAR(skew, 0.0, 1e-10);

    // Right-skewed data should have positive skewness
    std::vector<double> right_skewed = {1.0, 2.0, 3.0, 4.0, 10.0};
    skew = StatisticsD::skewness(right_skewed);
    EXPECT_GT(skew, 0.0);

    // Left-skewed data should have negative skewness
    std::vector<double> left_skewed = {1.0, 7.0, 8.0, 9.0, 10.0};
    skew = StatisticsD::skewness(left_skewed);
    EXPECT_LT(skew, 0.0);

    // Test with insufficient data
    EXPECT_DOUBLE_EQ(StatisticsD::skewness(single_value), 0.0);
    std::vector<double> two_values = {1.0, 2.0};
    EXPECT_DOUBLE_EQ(StatisticsD::skewness(two_values), 0.0);
}

TEST_F(StatisticsTest, Kurtosis) {
    // Normal distribution should have kurtosis close to 0 (excess kurtosis)
    std::vector<double> normal_like = {10.1, 10.3, 10.2, 10.4,
                                       10.3, 10.1, 10.5, 10.2};
    double kurt = StatisticsD::kurtosis(normal_like);
    EXPECT_NEAR(kurt, 0.0,
                2.0);  // Allow some tolerance due to small sample size

    // Uniform distribution should have negative kurtosis (platykurtic)
    std::vector<double> uniform = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    kurt = StatisticsD::kurtosis(uniform);
    EXPECT_LT(kurt, 0.0);

    // Test with insufficient data
    EXPECT_DOUBLE_EQ(StatisticsD::kurtosis(single_value), 0.0);
    std::vector<double> three_values = {1.0, 2.0, 3.0};
    EXPECT_DOUBLE_EQ(StatisticsD::kurtosis(three_values), 0.0);
}

TEST_F(StatisticsTest, Correlation) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {2.0, 4.0, 6.0, 8.0,
                             10.0};  // Perfect positive correlation

    double corr = StatisticsD::correlation(x, y);
    EXPECT_NEAR(corr, 1.0, 1e-10);

    std::vector<double> y_neg = {-2.0, -4.0, -6.0, -8.0,
                                 -10.0};  // Perfect negative correlation
    corr = StatisticsD::correlation(x, y_neg);
    EXPECT_NEAR(corr, -1.0, 1e-10);

    std::vector<double> y_independent = {1.0, 5.0, 2.0, 8.0,
                                         3.0};  // Low correlation
    corr = StatisticsD::correlation(x, y_independent);
    EXPECT_GT(corr, -1.0);
    EXPECT_LT(corr, 1.0);

    // Test with mismatched sizes
    std::vector<double> wrong_size = {1.0, 2.0, 3.0};
    corr = StatisticsD::correlation(x, wrong_size);
    EXPECT_DOUBLE_EQ(corr, 0.0);

    // Test with empty data
    corr = StatisticsD::correlation(empty_data, empty_data);
    EXPECT_DOUBLE_EQ(corr, 0.0);
}

TEST_F(StatisticsTest, Covariance) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {2.0, 4.0, 6.0, 8.0, 10.0};

    double cov_pop = StatisticsD::covariance(x, y, false);
    double cov_sample = StatisticsD::covariance(x, y, true);

    // Both should be positive since x and y increase together
    EXPECT_GT(cov_pop, 0.0);
    EXPECT_GT(cov_sample, 0.0);

    // Sample covariance should be larger than population covariance
    EXPECT_GT(cov_sample, cov_pop);

    // Test with mismatched sizes
    std::vector<double> wrong_size = {1.0, 2.0, 3.0};
    double cov_wrong = StatisticsD::covariance(x, wrong_size);
    EXPECT_DOUBLE_EQ(cov_wrong, 0.0);
}

TEST_F(StatisticsTest, Percentile) {
    std::vector<double> sorted = {1.0, 2.0, 3.0, 4.0, 5.0,
                                  6.0, 7.0, 8.0, 9.0, 10.0};

    // Test exact percentiles
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 0.0), 1.0);
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 100.0), 10.0);

    // Test intermediate percentiles
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 25.0), 3.25);  // Q1
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 50.0), 5.5);   // Median
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 75.0), 7.75);  // Q3

    // Test edge cases
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(empty_data, 50.0), 0.0);
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, -10.0), 0.0);
    EXPECT_DOUBLE_EQ(StatisticsD::percentile(sorted, 110.0), 0.0);
}

TEST_F(StatisticsTest, InterquartileRange) {
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0,
                                6.0, 7.0, 8.0, 9.0, 10.0};
    double iqr = StatisticsD::interquartileRange(data);

    double expected_iqr = 7.75 - 3.25;  // Q3 - Q1
    EXPECT_DOUBLE_EQ(iqr, expected_iqr);

    // Test with empty data
    EXPECT_DOUBLE_EQ(StatisticsD::interquartileRange(empty_data), 0.0);
}

TEST_F(StatisticsTest, DetectOutliers) {
    std::vector<double> data_with_outliers = {1.0, 2.0,   3.0,  4.0,
                                              5.0, 100.0, -50.0};
    auto outliers = StatisticsD::detectOutliers(data_with_outliers);

    // Should detect 100.0 and -50.0 as outliers
    EXPECT_EQ(outliers.size(), 2);
    EXPECT_TRUE(std::find(outliers.begin(), outliers.end(), 100.0) !=
                outliers.end());
    EXPECT_TRUE(std::find(outliers.begin(), outliers.end(), -50.0) !=
                outliers.end());

    // Test with different multiplier - stricter threshold should find at least
    // as many
    auto outliers_strict = StatisticsD::detectOutliers(data_with_outliers, 1.0);
    EXPECT_GE(outliers_strict.size(), outliers.size());

    // Test with no outliers
    auto no_outliers = StatisticsD::detectOutliers(simple_data);
    EXPECT_TRUE(no_outliers.empty());

    // Test with insufficient data
    auto insufficient_outliers = StatisticsD::detectOutliers({1.0, 2.0});
    EXPECT_TRUE(insufficient_outliers.empty());
}

TEST_F(StatisticsTest, ZScores) {
    std::vector<double> data = {10.0, 20.0, 30.0};
    auto z_scores = StatisticsD::zScores(data);

    EXPECT_EQ(z_scores.size(), 3);

    // Check that mean of z-scores is 0 and standard deviation is ~1
    double mean_z = StatisticsD::mean(z_scores);
    double std_z = StatisticsD::standardDeviation(z_scores, false);

    EXPECT_NEAR(mean_z, 0.0, 1e-10);
    // Note: For small samples, population std dev of z-scores may differ from 1
    EXPECT_NEAR(std_z, 1.0, 0.25);

    // Test with constant data (all same values)
    std::vector<double> constant = {5.0, 5.0, 5.0, 5.0};
    auto z_constant = StatisticsD::zScores(constant);

    // All z-scores should be 0
    for (double z : z_constant) {
        EXPECT_DOUBLE_EQ(z, 0.0);
    }

    // Test with empty data
    auto z_empty = StatisticsD::zScores(empty_data);
    EXPECT_TRUE(z_empty.empty());
}

TEST_F(StatisticsTest, FloatType) {
    std::vector<float> float_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float mean_f = StatisticsF::mean(float_data);
    EXPECT_FLOAT_EQ(mean_f, 3.0f);

    float median_f = StatisticsF::median(float_data);
    EXPECT_FLOAT_EQ(median_f, 3.0f);

    float var_f = StatisticsF::variance(float_data, false);
    EXPECT_FLOAT_EQ(var_f, 2.0f);

    float std_f = StatisticsF::standardDeviation(float_data, false);
    EXPECT_FLOAT_EQ(std_f, std::sqrt(2.0f));
}

TEST_F(StatisticsTest, Precision) {
    // Test with high precision values
    std::vector<double> precise_data = {1.0000001, 2.0000002, 3.0000003,
                                        4.0000004, 5.0000005};

    double mean = StatisticsD::mean(precise_data);
    EXPECT_NEAR(mean, 3.0000003, 1e-10);

    double variance = StatisticsD::variance(precise_data, false);
    EXPECT_NEAR(variance, 2.0, 1e-5);
}

TEST_F(StatisticsTest, LargeData) {
    // Test with larger dataset
    std::vector<double> large_data;
    for (int i = 1; i <= 1000; ++i) {
        large_data.push_back(static_cast<double>(i));
    }

    double mean = StatisticsD::mean(large_data);
    EXPECT_NEAR(mean, 500.5, 1e-10);

    double median = StatisticsD::median(large_data);
    EXPECT_NEAR(median, 500.5, 1e-10);

    double variance = StatisticsD::variance(large_data, true);
    // For 1..n, sample variance = n*(n+1)/12 = 1000*1001/12 = 83416.67
    double expected_variance = 1000.0 * 1001.0 / 12.0;
    EXPECT_NEAR(variance, expected_variance, 1.0);
}

TEST_F(StatisticsTest, RandomData) {
    // Test with random data to ensure robustness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);

    std::vector<double> random_data;
    for (int i = 0; i < 1000; ++i) {
        random_data.push_back(dist(gen));
    }

    // For a normal distribution, mean should be close to 0 and std dev close to
    // 1
    double mean = StatisticsD::mean(random_data);
    double std_dev = StatisticsD::standardDeviation(random_data, true);

    EXPECT_NEAR(mean, 0.0, 0.1);  // Allow some tolerance due to random sampling
    EXPECT_NEAR(std_dev, 1.0, 0.1);

    // Skewness and kurtosis should be close to 0 for normal distribution
    double skewness = StatisticsD::skewness(random_data);
    double kurtosis = StatisticsD::kurtosis(random_data);

    EXPECT_NEAR(skewness, 0.0, 0.5);
    EXPECT_NEAR(kurtosis, 0.0, 1.0);
}

TEST_F(StatisticsTest, Performance) {
    // Test performance with large dataset
    const size_t data_size = 100000;
    std::vector<double> large_data;
    large_data.reserve(data_size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1000.0);

    for (size_t i = 0; i < data_size; ++i) {
        large_data.push_back(dist(gen));
    }

    auto start = std::chrono::high_resolution_clock::now();

    double mean = StatisticsD::mean(large_data);
    double variance = StatisticsD::variance(large_data);
    double std_dev = StatisticsD::standardDeviation(large_data);
    double skewness = StatisticsD::skewness(large_data);
    double kurtosis = StatisticsD::kurtosis(large_data);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Processed {} data points in {} ms", data_size,
                 duration.count());

    // Basic sanity checks
    EXPECT_GE(mean, 0.0);
    EXPECT_LE(mean, 1000.0);
    EXPECT_GE(variance, 0.0);
    EXPECT_GE(std_dev, 0.0);
}
