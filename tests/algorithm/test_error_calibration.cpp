#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <future>
#include <random>
#include <thread>
#include "atom/algorithm/error_calibration.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateLinearData(
    size_t n, T slope, T intercept, T noise_level = 0.1) {
    std::vector<T> x(n), y(n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i) / static_cast<T>(n) * 100.0;
        y[i] = slope * x[i] + intercept + dist(gen);
    }
    return {x, y};
}

template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateExponentialData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n), y(n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i) / static_cast<T>(n) * 10.0;
        y[i] = a * std::exp(b * x[i]) * (1.0 + dist(gen));
    }
    return {x, y};
}

template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateLogarithmicData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n), y(n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i + 1) / static_cast<T>(n) * 10.0;
        y[i] = a + b * std::log(x[i]) * (1.0 + dist(gen));
    }
    return {x, y};
}

template <typename T>
std::pair<std::vector<T>, std::vector<T>> generatePowerLawData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n), y(n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i + 1) / static_cast<T>(n) * 10.0;
        y[i] = a * std::pow(x[i], b) * (1.0 + dist(gen));
    }
    return {x, y};
}

class ErrorCalibrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
    const size_t data_size = 100;
    const double test_slope = 2.5;
    const double test_intercept = 1.2;
};

TEST_F(ErrorCalibrationTest, LinearCalibration) {
    auto [measured, actual] =
        generateLinearData<double>(data_size, test_slope, test_intercept);
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(measured, actual);
    double slope = calibrator.getSlope();
    double intercept = calibrator.getIntercept();
    EXPECT_NEAR(slope, test_slope, 0.2);
    EXPECT_NEAR(intercept, test_intercept, 0.2);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
    double test_value = 42.0;
    double expected = test_slope * test_value + test_intercept;
    EXPECT_NEAR(calibrator.apply(test_value), expected, 0.5);
}

TEST_F(ErrorCalibrationTest, PolynomialCalibration) {
    std::vector<double> x(data_size), y(data_size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-0.5, 0.5);
    for (size_t i = 0; i < data_size; ++i) {
        x[i] = static_cast<double>(i) / static_cast<double>(data_size) * 10.0;
        y[i] = 2.0 * x[i] * x[i] + 3.0 * x[i] + 5.0 + dist(gen);
    }
    ErrorCalibration<double> calibrator;
    calibrator.polynomialCalibrate(x, y, 2);
    EXPECT_NEAR(calibrator.getSlope(), 3.0, 1.0);
    EXPECT_NEAR(calibrator.getIntercept(), 5.0, 1.0);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

TEST_F(ErrorCalibrationTest, ExponentialCalibration) {
    double a = 2.0, b = 0.5;
    auto [x, y] = generateExponentialData<double>(data_size, a, b, 0.05);
    ErrorCalibration<double> calibrator;
    calibrator.exponentialCalibrate(x, y);
    EXPECT_NEAR(calibrator.getSlope(), b, 0.2);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

TEST_F(ErrorCalibrationTest, LogarithmicCalibration) {
    double a = 5.0, b = 3.0;
    auto [x, y] = generateLogarithmicData<double>(data_size, a, b, 0.05);
    ErrorCalibration<double> calibrator;
    calibrator.logarithmicCalibrate(x, y);
    EXPECT_NEAR(calibrator.getSlope(), b, 0.5);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

TEST_F(ErrorCalibrationTest, PowerLawCalibration) {
    double a = 2.0, b = 1.5;
    auto [x, y] = generatePowerLawData<double>(data_size, a, b, 0.05);
    ErrorCalibration<double> calibrator;
    calibrator.powerLawCalibrate(x, y);
    EXPECT_NEAR(calibrator.getSlope(), b, 0.2);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

TEST_F(ErrorCalibrationTest, ErrorMetrics) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {3.0, 5.0, 7.0, 9.0, 11.0};
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    EXPECT_NEAR(calibrator.getMse(), 0.0, 1e-10);
    EXPECT_NEAR(calibrator.getMae(), 0.0, 1e-10);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_NEAR(r_squared.value(), 1.0, 1e-10);
    y[2] = 12.0;
    ErrorCalibration<double> calibrator2;
    calibrator2.linearCalibrate(x, y);
    EXPECT_GT(calibrator2.getMse(), 1.0);
    EXPECT_GT(calibrator2.getMae(), 0.5);
    auto r_squared2 = calibrator2.getRSquared();
    ASSERT_TRUE(r_squared2.has_value());
    EXPECT_LT(r_squared2.value(), 0.95);
}

TEST_F(ErrorCalibrationTest, Residuals) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {3.0, 5.1, 6.9, 8.8, 11.2};
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    auto residuals = calibrator.getResiduals();
    ASSERT_EQ(residuals.size(), x.size());
    std::vector<double> expected_residuals;
    for (size_t i = 0; i < x.size(); ++i) {
        double predicted = calibrator.apply(x[i]);
        expected_residuals.push_back(y[i] - predicted);
    }
    for (size_t i = 0; i < residuals.size(); ++i) {
        EXPECT_NEAR(residuals[i], expected_residuals[i], 1e-10);
    }
}

TEST_F(ErrorCalibrationTest, BootstrapConfidenceInterval) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept, 0.2);
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    auto [lower, upper] =
        calibrator.bootstrapConfidenceInterval(x, y, 100, 0.95);
    EXPECT_LE(lower, test_slope);
    EXPECT_GE(upper, test_slope);
    EXPECT_LE(upper - lower, 1.0);
}

TEST_F(ErrorCalibrationTest, OutlierDetection) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    std::vector<double> y = {3.1,  4.9,  7.2,  8.8,  11.1,
                             13.0, 14.9, 17.1, 19.0, 30.0};
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    auto [mean_residual, std_dev, threshold] =
        calibrator.outlierDetection(x, y, 2.0);
    EXPECT_NEAR(mean_residual, 0.0, 1.0);
    EXPECT_GT(std_dev, 1.0);
    EXPECT_DOUBLE_EQ(threshold, 2.0);
    auto residuals = calibrator.getResiduals();
    bool found_outlier = false;
    for (size_t i = 0; i < residuals.size(); ++i) {
        if (std::abs(residuals[i] - mean_residual) > threshold * std_dev) {
            found_outlier = true;
            EXPECT_EQ(i, 9);
        }
    }
    EXPECT_TRUE(found_outlier);
}

TEST_F(ErrorCalibrationTest, CrossValidation) {
    auto [x, y] =
        generateLinearData<double>(50, test_slope, test_intercept, 0.1);
    ErrorCalibration<double> calibrator;
    EXPECT_NO_THROW(calibrator.crossValidation(x, y, 5));
    EXPECT_THROW(calibrator.crossValidation(x, y, 51), std::invalid_argument);
    EXPECT_THROW(calibrator.crossValidation(x, y, 0), std::invalid_argument);
}

TEST_F(ErrorCalibrationTest, ExceptionHandling) {
    std::vector<double> empty;
    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y = {3.0, 5.0, 7.0};
    std::vector<double> mismatched = {1.0, 2.0};
    std::vector<double> with_nan = {
        1.0, std::numeric_limits<double>::quiet_NaN(), 3.0};
    std::vector<double> with_inf = {
        1.0, std::numeric_limits<double>::infinity(), 3.0};
    std::vector<double> negative = {-1.0, -2.0, -3.0};
    ErrorCalibration<double> calibrator;
    EXPECT_THROW(calibrator.linearCalibrate(empty, y), std::invalid_argument);
    EXPECT_THROW(calibrator.linearCalibrate(x, empty), std::invalid_argument);
    EXPECT_THROW(calibrator.linearCalibrate(x, mismatched),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(with_nan, y, 1),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(x, with_inf, 1),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(x, y, 0),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(x, y, 10),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.logarithmicCalibrate(negative, y),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.exponentialCalibrate(x, negative),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.powerLawCalibrate(negative, y),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.powerLawCalibrate(x, negative),
                 std::invalid_argument);
}

TEST_F(ErrorCalibrationTest, VaryingTypes) {
    auto [x_double, y_double] =
        generateLinearData<double>(data_size, test_slope, test_intercept);
    std::vector<float> x_float(x_double.begin(), x_double.end());
    std::vector<float> y_float(y_double.begin(), y_double.end());
    ErrorCalibration<float> calibrator;
    calibrator.linearCalibrate(x_float, y_float);
    float slope = calibrator.getSlope();
    float intercept = calibrator.getIntercept();
    EXPECT_NEAR(slope, static_cast<float>(test_slope), 0.2f);
    EXPECT_NEAR(intercept, static_cast<float>(test_intercept), 0.2f);
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9f);
}

TEST_F(ErrorCalibrationTest, Multithreading) {
    auto [x, y] = generateLinearData<double>(10000, test_slope, test_intercept);
    auto start = std::chrono::high_resolution_clock::now();
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Multithreaded calibration of 10000 points took {} ms",
                 duration);
    EXPECT_NEAR(calibrator.getSlope(), test_slope, 0.1);
    EXPECT_NEAR(calibrator.getIntercept(), test_intercept, 0.1);
}

TEST_F(ErrorCalibrationTest, AsyncCalibration) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept);
    auto task = calibrateAsync(x, y);
    std::this_thread::sleep_for(100ms);
    auto calibrator = task.getResult();
    ASSERT_NE(calibrator, nullptr);
    EXPECT_NEAR(calibrator->getSlope(), test_slope, 0.2);
    EXPECT_NEAR(calibrator->getIntercept(), test_intercept, 0.2);
    delete calibrator;
}

TEST_F(ErrorCalibrationTest, ThreadSafety) {
    const int num_threads = 4;
    std::vector<std::pair<std::vector<double>, std::vector<double>>> datasets;
    for (int i = 0; i < num_threads; ++i) {
        datasets.push_back(generateLinearData<double>(data_size, test_slope + i,
                                                      test_intercept + i));
    }
    std::vector<std::future<void>> futures;
    std::vector<ErrorCalibration<double>> calibrators(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&calibrators, &datasets, i]() {
                calibrators[i].linearCalibrate(datasets[i].first,
                                               datasets[i].second);
            }));
    }
    for (auto& future : futures) {
        future.get();
    }
    for (int i = 0; i < num_threads; ++i) {
        EXPECT_NEAR(calibrators[i].getSlope(), test_slope + i, 0.2);
        EXPECT_NEAR(calibrators[i].getIntercept(), test_intercept + i, 0.2);
    }
}

TEST_F(ErrorCalibrationTest, MemoryManagement) {
    auto [x, y] = generateLinearData<double>(10000, test_slope, test_intercept);
    for (int i = 0; i < 10; ++i) {
        ErrorCalibration<double> calibrator;
        calibrator.linearCalibrate(x, y);
        auto residuals = calibrator.getResiduals();
        EXPECT_EQ(residuals.size(), x.size());
    }
}

TEST_F(ErrorCalibrationTest, PlotResiduals) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept);
    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);
    std::string tempFilename = "/tmp/residuals_test.csv";
    EXPECT_NO_THROW(calibrator.plotResiduals(tempFilename));
    std::ifstream file(tempFilename);
    EXPECT_TRUE(file.good());
    std::string line;
    size_t lineCount = 0;
    while (std::getline(file, line)) {
        lineCount++;
    }
    EXPECT_EQ(lineCount, data_size + 1);
    std::remove(tempFilename.c_str());
}

TEST_F(ErrorCalibrationTest, EdgeCases) {
    std::vector<double> constant_x(10, 5.0);
    std::vector<double> y = {10.0, 10.1, 9.9,  10.2, 9.8,
                             10.3, 9.7,  10.4, 9.6,  10.5};
    ErrorCalibration<double> calibrator;
    EXPECT_THROW(calibrator.linearCalibrate(constant_x, y), std::runtime_error);
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> perfect_y = {3.0, 5.0, 7.0, 9.0, 11.0};
    EXPECT_NO_THROW(calibrator.linearCalibrate(x, perfect_y));
    EXPECT_DOUBLE_EQ(calibrator.getSlope(), 2.0);
    EXPECT_DOUBLE_EQ(calibrator.getIntercept(), 1.0);
}

TEST_F(ErrorCalibrationTest, PerformanceBenchmark) {
    std::vector<size_t> sizes = {1000, 10000, 50000};
    for (auto size : sizes) {
        auto [x, y] =
            generateLinearData<double>(size, test_slope, test_intercept);
        auto start = std::chrono::high_resolution_clock::now();
        ErrorCalibration<double> calibrator;
        calibrator.linearCalibrate(x, y);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        spdlog::info("Linear calibration of {} points took {} ms", size,
                     duration);
    }
}
