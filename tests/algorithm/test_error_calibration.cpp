#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <random>
#include <thread>

#include "atom/algorithm/error_calibration.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper function to generate test data
template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateLinearData(
    size_t n, T slope, T intercept, T noise_level = 0.1) {
    std::vector<T> x(n);
    std::vector<T> y(n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i) / static_cast<T>(n) * 100.0;
        y[i] = slope * x[i] + intercept + dist(gen);
    }

    return {x, y};
}

// Helper function to generate exponential data
template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateExponentialData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n);
    std::vector<T> y(n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<T>(i) / static_cast<T>(n) * 10.0;
        y[i] = a * std::exp(b * x[i]) * (1.0 + dist(gen));
    }

    return {x, y};
}

// Helper function to generate logarithmic data
template <typename T>
std::pair<std::vector<T>, std::vector<T>> generateLogarithmicData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n);
    std::vector<T> y(n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);

    for (size_t i = 0; i < n; ++i) {
        // Ensure x is positive for logarithm
        x[i] = static_cast<T>(i + 1) / static_cast<T>(n) * 10.0;
        y[i] = a + b * std::log(x[i]) * (1.0 + dist(gen));
    }

    return {x, y};
}

// Helper function to generate power law data
template <typename T>
std::pair<std::vector<T>, std::vector<T>> generatePowerLawData(
    size_t n, T a, T b, T noise_level = 0.1) {
    std::vector<T> x(n);
    std::vector<T> y(n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<T> dist(-noise_level, noise_level);

    for (size_t i = 0; i < n; ++i) {
        // Ensure x is positive
        x[i] = static_cast<T>(i + 1) / static_cast<T>(n) * 10.0;
        y[i] = a * std::pow(x[i], b) * (1.0 + dist(gen));
    }

    return {x, y};
}

// Test fixture for ErrorCalibration tests
class ErrorCalibrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Common test data
    const size_t data_size = 100;
    const double test_slope = 2.5;
    const double test_intercept = 1.2;
};

// Basic linear calibration tests
TEST_F(ErrorCalibrationTest, LinearCalibration) {
    auto [measured, actual] =
        generateLinearData<double>(data_size, test_slope, test_intercept);

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(measured, actual);

    double slope = calibrator.getSlope();
    double intercept = calibrator.getIntercept();

    // The calibrated slope and intercept should be close to the original values
    EXPECT_NEAR(slope, test_slope, 0.2);
    EXPECT_NEAR(intercept, test_intercept, 0.2);

    // R-squared should be close to 1.0 for well-fitted data
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);

    // Test apply function
    double test_value = 42.0;
    double expected = test_slope * test_value + test_intercept;
    EXPECT_NEAR(calibrator.apply(test_value), expected, 0.5);
}

// Test polynomial calibration
TEST_F(ErrorCalibrationTest, PolynomialCalibration) {
    // Generate data with a quadratic relationship: y = 2x^2 + 3x + 5
    std::vector<double> x(data_size);
    std::vector<double> y(data_size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-0.5, 0.5);

    for (size_t i = 0; i < data_size; ++i) {
        x[i] = static_cast<double>(i) / static_cast<double>(data_size) * 10.0;
        y[i] = 2.0 * x[i] * x[i] + 3.0 * x[i] + 5.0 + dist(gen);
    }

    ErrorCalibration<double> calibrator;
    // Using degree 2 for quadratic relationship
    calibrator.polynomialCalibrate(x, y, 2);

    // For polynomial calibration, we expect the slope to be close to the
    // first-order coefficient and the intercept to be close to the constant
    // term Note: The implementation assigns first-order coefficient to slope
    // and constant to intercept
    EXPECT_NEAR(calibrator.getSlope(), 3.0, 1.0);
    EXPECT_NEAR(calibrator.getIntercept(), 5.0, 1.0);

    // R-squared should be high for a good fit
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

// Test exponential calibration
TEST_F(ErrorCalibrationTest, ExponentialCalibration) {
    double a = 2.0;
    double b = 0.5;
    auto [x, y] = generateExponentialData<double>(data_size, a, b, 0.05);

    ErrorCalibration<double> calibrator;
    calibrator.exponentialCalibrate(x, y);

    // For exponential model y = a * exp(b*x), the calibrator.getSlope() returns
    // b and calibrator.getIntercept() returns a
    EXPECT_NEAR(calibrator.getSlope(), b, 0.2);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);

    // R-squared should be high for a good fit
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

// Test logarithmic calibration
TEST_F(ErrorCalibrationTest, LogarithmicCalibration) {
    double a = 5.0;
    double b = 3.0;
    auto [x, y] = generateLogarithmicData<double>(data_size, a, b, 0.05);

    ErrorCalibration<double> calibrator;
    calibrator.logarithmicCalibrate(x, y);

    // For logarithmic model y = a + b*ln(x), the calibrator.getSlope() returns
    // b and calibrator.getIntercept() returns a
    EXPECT_NEAR(calibrator.getSlope(), b, 0.5);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);

    // R-squared should be high for a good fit
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

// Test power law calibration
TEST_F(ErrorCalibrationTest, PowerLawCalibration) {
    double a = 2.0;
    double b = 1.5;
    auto [x, y] = generatePowerLawData<double>(data_size, a, b, 0.05);

    ErrorCalibration<double> calibrator;
    calibrator.powerLawCalibrate(x, y);

    // For power law model y = a * x^b, the calibrator.getSlope() returns b
    // and calibrator.getIntercept() returns a
    EXPECT_NEAR(calibrator.getSlope(), b, 0.2);
    EXPECT_NEAR(calibrator.getIntercept(), a, 0.5);

    // R-squared should be high for a good fit
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9);
}

// Test error metrics
TEST_F(ErrorCalibrationTest, ErrorMetrics) {
    // Generate data with known error
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {3.0, 5.0, 7.0, 9.0,
                             11.0};  // Perfect linear relationship y = 2x + 1

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    // For a perfect fit, MSE and MAE should be very close to zero
    EXPECT_NEAR(calibrator.getMse(), 0.0, 1e-10);
    EXPECT_NEAR(calibrator.getMae(), 0.0, 1e-10);

    // R-squared should be very close to 1.0
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_NEAR(r_squared.value(), 1.0, 1e-10);

    // Now add an outlier and check that errors increase
    y[2] = 12.0;  // Significant deviation from the pattern

    ErrorCalibration<double> calibrator2;
    calibrator2.linearCalibrate(x, y);

    // MSE and MAE should be significantly higher
    EXPECT_GT(calibrator2.getMse(), 1.0);
    EXPECT_GT(calibrator2.getMae(), 0.5);

    // R-squared should be less than 1.0
    auto r_squared2 = calibrator2.getRSquared();
    ASSERT_TRUE(r_squared2.has_value());
    EXPECT_LT(r_squared2.value(), 0.95);
}

// Test residuals
TEST_F(ErrorCalibrationTest, Residuals) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {
        3.0, 5.1, 6.9, 8.8,
        11.2};  // Approximately y = 2x + 1 with small deviations

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    auto residuals = calibrator.getResiduals();

    // Check that we have the right number of residuals
    ASSERT_EQ(residuals.size(), x.size());

    // Calculate expected residuals based on the true model (y = 2x + 1)
    std::vector<double> expected_residuals;
    for (size_t i = 0; i < x.size(); ++i) {
        double predicted = calibrator.apply(x[i]);
        expected_residuals.push_back(y[i] - predicted);
    }

    // Check that residuals match expected values
    for (size_t i = 0; i < residuals.size(); ++i) {
        EXPECT_NEAR(residuals[i], expected_residuals[i], 1e-10);
    }
}

// Test bootstrap confidence interval
TEST_F(ErrorCalibrationTest, BootstrapConfidenceInterval) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept, 0.2);

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    auto [lower, upper] =
        calibrator.bootstrapConfidenceInterval(x, y, 100, 0.95);

    // The confidence interval should contain the true slope
    EXPECT_LE(lower, test_slope);
    EXPECT_GE(upper, test_slope);

    // The interval should have reasonable width based on the noise level
    EXPECT_LE(upper - lower,
              1.0);  // This threshold depends on the noise level used
}

// Test outlier detection
TEST_F(ErrorCalibrationTest, OutlierDetection) {
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    std::vector<double> y = {
        3.1,  4.9,  7.2,  8.8,  11.1,
        13.0, 14.9, 17.1, 19.0, 30.0};  // Last point is an outlier

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    auto [mean_residual, std_dev, threshold] =
        calibrator.outlierDetection(x, y, 2.0);

    // Mean residual should be close to zero for a balanced dataset
    EXPECT_NEAR(mean_residual, 0.0, 1.0);

    // Standard deviation should be significant due to the outlier
    EXPECT_GT(std_dev, 1.0);

    // The threshold should be set to the input value
    EXPECT_DOUBLE_EQ(threshold, 2.0);

    // Check residuals to see if the outlier is identified
    auto residuals = calibrator.getResiduals();
    bool found_outlier = false;
    for (size_t i = 0; i < residuals.size(); ++i) {
        if (std::abs(residuals[i] - mean_residual) > threshold * std_dev) {
            found_outlier = true;
            EXPECT_EQ(i, 9);  // The outlier is at index 9 (10th element)
        }
    }
    EXPECT_TRUE(found_outlier);
}

// Test cross-validation
TEST_F(ErrorCalibrationTest, CrossValidation) {
    auto [x, y] =
        generateLinearData<double>(50, test_slope, test_intercept, 0.1);

    ErrorCalibration<double> calibrator;

    // This should run without exceptions
    EXPECT_NO_THROW(calibrator.crossValidation(x, y, 5));

    // Test with invalid k
    EXPECT_THROW(calibrator.crossValidation(x, y, 51),
                 std::invalid_argument);  // k > data size
    EXPECT_THROW(calibrator.crossValidation(x, y, 0),
                 std::invalid_argument);  // k = 0
}

// Test exception handling for invalid inputs
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

    // Test empty inputs
    EXPECT_THROW(calibrator.linearCalibrate(empty, y), std::invalid_argument);
    EXPECT_THROW(calibrator.linearCalibrate(x, empty), std::invalid_argument);

    // Test mismatched sizes
    EXPECT_THROW(calibrator.linearCalibrate(x, mismatched),
                 std::invalid_argument);

    // Test NaN and infinity
    EXPECT_THROW(calibrator.polynomialCalibrate(with_nan, y, 1),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(x, with_inf, 1),
                 std::invalid_argument);

    // Test invalid degree for polynomial
    EXPECT_THROW(calibrator.polynomialCalibrate(x, y, 0),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.polynomialCalibrate(x, y, 10),
                 std::invalid_argument);  // degree > data size

    // Test negative values for log/exp models
    EXPECT_THROW(calibrator.logarithmicCalibrate(negative, y),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.exponentialCalibrate(x, negative),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.powerLawCalibrate(negative, y),
                 std::invalid_argument);
    EXPECT_THROW(calibrator.powerLawCalibrate(x, negative),
                 std::invalid_argument);
}

// Test varying types (using float instead of double)
TEST_F(ErrorCalibrationTest, VaryingTypes) {
    auto [x_double, y_double] =
        generateLinearData<double>(data_size, test_slope, test_intercept);

    // Convert to float
    std::vector<float> x_float(x_double.begin(), x_double.end());
    std::vector<float> y_float(y_double.begin(), y_double.end());

    ErrorCalibration<float> calibrator;
    calibrator.linearCalibrate(x_float, y_float);

    float slope = calibrator.getSlope();
    float intercept = calibrator.getIntercept();

    // The calibrated slope and intercept should be close to the original values
    EXPECT_NEAR(slope, static_cast<float>(test_slope), 0.2f);
    EXPECT_NEAR(intercept, static_cast<float>(test_intercept), 0.2f);

    // R-squared should be close to 1.0 for well-fitted data
    auto r_squared = calibrator.getRSquared();
    ASSERT_TRUE(r_squared.has_value());
    EXPECT_GT(r_squared.value(), 0.9f);
}

// Test multithreading
TEST_F(ErrorCalibrationTest, Multithreading) {
    // Create a large dataset to ensure multithreading benefits are visible
    auto [x, y] = generateLinearData<double>(10000, test_slope, test_intercept);

    // Measure time for a calibration that should use multiple threads
    auto start = std::chrono::high_resolution_clock::now();

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Just log the duration rather than asserting - performance varies by
    // hardware
    LOG_F(INFO, "Multithreaded calibration of 10000 points took %lld ms",
          duration);

    // Verify correctness of results
    EXPECT_NEAR(calibrator.getSlope(), test_slope, 0.1);
    EXPECT_NEAR(calibrator.getIntercept(), test_intercept, 0.1);
}

// Test the async calibration functionality using coroutines
TEST_F(ErrorCalibrationTest, AsyncCalibration) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept);

    // Start the async calibration
    auto task = calibrateAsync(x, y);

    // Wait a bit for the background thread to complete
    std::this_thread::sleep_for(100ms);

    // Resume the coroutine to get the result
    auto calibrator = task.getResult();
    ASSERT_NE(calibrator, nullptr);

    // Verify calibration results
    EXPECT_NEAR(calibrator->getSlope(), test_slope, 0.2);
    EXPECT_NEAR(calibrator->getIntercept(), test_intercept, 0.2);

    // Clean up
    delete calibrator;
}

// Ensure thread safety with concurrent calibrations
TEST_F(ErrorCalibrationTest, ThreadSafety) {
    // Generate multiple datasets
    const int num_threads = 4;
    std::vector<std::pair<std::vector<double>, std::vector<double>>> datasets;

    for (int i = 0; i < num_threads; ++i) {
        datasets.push_back(generateLinearData<double>(data_size, test_slope + i,
                                                      test_intercept + i));
    }

    // Run calibrations concurrently
    std::vector<std::future<void>> futures;
    std::vector<ErrorCalibration<double>> calibrators(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&calibrators, &datasets, i]() {
                calibrators[i].linearCalibrate(datasets[i].first,
                                               datasets[i].second);
            }));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        future.get();
    }

    // Verify results
    for (int i = 0; i < num_threads; ++i) {
        EXPECT_NEAR(calibrators[i].getSlope(), test_slope + i, 0.2);
        EXPECT_NEAR(calibrators[i].getIntercept(), test_intercept + i, 0.2);
    }
}

// Test memory management
TEST_F(ErrorCalibrationTest, MemoryManagement) {
    auto [x, y] = generateLinearData<double>(10000, test_slope, test_intercept);

    // Create and destroy many calibrators to check for memory leaks
    for (int i = 0; i < 10; ++i) {
        ErrorCalibration<double> calibrator;
        calibrator.linearCalibrate(x, y);

        // Force calculation of residuals to use memory
        auto residuals = calibrator.getResiduals();
        EXPECT_EQ(residuals.size(), x.size());
    }

    // No explicit assertion - this test passes if it doesn't crash or leak
    // memory Can be verified with a memory checker like valgrind
}

// Test plotting residuals
TEST_F(ErrorCalibrationTest, PlotResiduals) {
    auto [x, y] =
        generateLinearData<double>(data_size, test_slope, test_intercept);

    ErrorCalibration<double> calibrator;
    calibrator.linearCalibrate(x, y);

    std::string tempFilename = "/tmp/residuals_test.csv";

    // Should not throw
    EXPECT_NO_THROW(calibrator.plotResiduals(tempFilename));

    // Check if the file exists and has content
    std::ifstream file(tempFilename);
    EXPECT_TRUE(file.good());

    std::string line;
    size_t lineCount = 0;
    while (std::getline(file, line)) {
        lineCount++;
    }

    // Header + one line per data point
    EXPECT_EQ(lineCount, data_size + 1);

    // Clean up
    std::remove(tempFilename.c_str());
}

// Test with edge cases
TEST_F(ErrorCalibrationTest, EdgeCases) {
    // Test with constant input
    std::vector<double> constant_x(10, 5.0);  // All x values are 5.0
    std::vector<double> y = {10.0, 10.1, 9.9,  10.2, 9.8,
                             10.3, 9.7,  10.4, 9.6,  10.5};

    ErrorCalibration<double> calibrator;

    // This should throw because x values are constant (division by zero in
    // slope calculation)
    EXPECT_THROW(calibrator.linearCalibrate(constant_x, y), std::runtime_error);

    // Test with perfectly collinear data
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> perfect_y = {3.0, 5.0, 7.0, 9.0,
                                     11.0};  // Exactly y = 2x + 1

    // This should work fine and give exact results
    EXPECT_NO_THROW(calibrator.linearCalibrate(x, perfect_y));
    EXPECT_DOUBLE_EQ(calibrator.getSlope(), 2.0);
    EXPECT_DOUBLE_EQ(calibrator.getIntercept(), 1.0);
}

// Performance benchmark
TEST_F(ErrorCalibrationTest, PerformanceBenchmark) {
    // Create large datasets of different sizes
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

        LOG_F(INFO, "Linear calibration of %zu points took %lld ms", size,
              duration);
    }
}
