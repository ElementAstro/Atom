#ifndef ATOM_ALGORITHM_TEST_ERROR_CALIBRATION_HPP
#define ATOM_ALGORITHM_TEST_ERROR_CALIBRATION_HPP

#include "atom/algorithm/error_calibration.hpp"
#include <gtest/gtest.h>

using namespace atom::algorithm;

class ErrorCalibrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        calibrator = std::make_unique<ErrorCalibration<double>>();
    }

    void TearDown() override { calibrator.reset(); }

    std::unique_ptr<ErrorCalibration<double>> calibrator;
};

TEST_F(ErrorCalibrationTest, BasicLinearCalibration) {
    std::vector<double> measured = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> actual = {2.0, 4.0, 6.0, 8.0, 10.0};

    calibrator->linearCalibrate(measured, actual);

    EXPECT_NEAR(calibrator->getSlope(), 2.0, 1e-6);
    EXPECT_NEAR(calibrator->getIntercept(), 0.0, 1e-6);
    EXPECT_NEAR(calibrator->getRSquared().value(), 1.0, 1e-6);
}

TEST_F(ErrorCalibrationTest, EmptyInputs) {
    std::vector<double> empty;
    EXPECT_THROW(calibrator->linearCalibrate(empty, empty),
                 std::invalid_argument);
}

TEST_F(ErrorCalibrationTest, UnequalSizeInputs) {
    std::vector<double> measured = {1.0, 2.0};
    std::vector<double> actual = {2.0};
    EXPECT_THROW(calibrator->linearCalibrate(measured, actual),
                 std::invalid_argument);
}

TEST_F(ErrorCalibrationTest, PolynomialCalibration) {
    std::vector<double> measured = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> actual = {1.0, 4.0, 9.0, 16.0, 25.0};

    calibrator->polynomialCalibrate(measured, actual, 2);

    // Test if the fit is reasonable for quadratic data
    EXPECT_GT(calibrator->getRSquared().value(), 0.9);
}

TEST_F(ErrorCalibrationTest, ExponentialCalibration) {
    std::vector<double> measured = {0.0, 1.0, 2.0, 3.0};
    std::vector<double> actual = {1.0, 2.71828, 7.38906, 20.0855};

    calibrator->exponentialCalibrate(measured, actual);

    // Test if the fit is reasonable for exponential data
    EXPECT_GT(calibrator->getRSquared().value(), 0.9);
}

TEST_F(ErrorCalibrationTest, LogarithmicCalibration) {
    std::vector<double> measured = {1.0, 2.0, 4.0, 8.0};
    std::vector<double> actual = {0.0, 0.693147, 1.38629, 2.07944};

    calibrator->logarithmicCalibrate(measured, actual);

    // Test if the fit is reasonable for logarithmic data
    EXPECT_GT(calibrator->getRSquared().value(), 0.9);
}

TEST_F(ErrorCalibrationTest, PowerLawCalibration) {
    std::vector<double> measured = {1.0, 2.0, 4.0, 8.0};
    std::vector<double> actual = {1.0, 4.0, 16.0, 64.0};

    calibrator->powerLawCalibrate(measured, actual);

    // Test if the fit is reasonable for power law data
    EXPECT_GT(calibrator->getRSquared().value(), 0.9);
}

TEST_F(ErrorCalibrationTest, ResidualCalculation) {
    std::vector<double> measured = {1.0, 2.0, 3.0};
    std::vector<double> actual = {2.0, 4.0, 6.0};

    calibrator->linearCalibrate(measured, actual);
    auto residuals = calibrator->getResiduals();

    EXPECT_EQ(residuals.size(), 3);
    for (const auto& residual : residuals) {
        EXPECT_NEAR(residual, 0.0, 1e-6);
    }
}

TEST_F(ErrorCalibrationTest, OutlierDetection) {
    std::vector<double> measured = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> actual = {2.0, 4.0, 6.0, 8.0,
                                  20.0};  // Last point is outlier

    calibrator->linearCalibrate(measured, actual);
    auto [meanResidual, stdDev, threshold] =
        calibrator->outlierDetection(measured, actual);

    EXPECT_GT(stdDev, 0.0);
}

TEST_F(ErrorCalibrationTest, BootstrapConfidenceInterval) {
    std::vector<double> measured = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> actual = {2.0, 4.0, 6.0, 8.0, 10.0};

    auto [lower, upper] =
        calibrator->bootstrapConfidenceInterval(measured, actual);

    EXPECT_LT(lower, upper);
    EXPECT_NEAR(2.0, (lower + upper) / 2.0, 0.5);  // Expected slope is 2.0
}

TEST_F(ErrorCalibrationTest, CrossValidation) {
    std::vector<double> measured(100);
    std::vector<double> actual(100);
    for (size_t i = 0; i < 100; ++i) {
        measured[i] = i;
        actual[i] = 2.0 * i + 1.0;  // Linear relationship with noise
    }

    EXPECT_NO_THROW(calibrator->crossValidation(measured, actual, 5));
}

TEST_F(ErrorCalibrationTest, ApplyCalibration) {
    std::vector<double> measured = {1.0, 2.0, 3.0};
    std::vector<double> actual = {2.0, 4.0, 6.0};

    calibrator->linearCalibrate(measured, actual);

    EXPECT_NEAR(calibrator->apply(4.0), 8.0, 1e-6);
    EXPECT_NEAR(calibrator->apply(5.0), 10.0, 1e-6);
}

TEST_F(ErrorCalibrationTest, MetricsCalculation) {
    std::vector<double> measured = {1.0, 2.0, 3.0};
    std::vector<double> actual = {2.1, 3.9, 6.2};  // Slight noise

    calibrator->linearCalibrate(measured, actual);

    EXPECT_GT(calibrator->getMse(), 0.0);
    EXPECT_GT(calibrator->getMae(), 0.0);
    EXPECT_LT(calibrator->getMse(), 0.1);  // Small error expected
    EXPECT_LT(calibrator->getMae(), 0.1);  // Small error expected
}

TEST_F(ErrorCalibrationTest, InvalidPolynomialDegree) {
    std::vector<double> measured = {1.0, 2.0, 3.0};
    std::vector<double> actual = {2.0, 4.0, 6.0};

    EXPECT_THROW(calibrator->polynomialCalibrate(measured, actual, 0),
                 std::invalid_argument);
    EXPECT_THROW(calibrator->polynomialCalibrate(measured, actual, -1),
                 std::invalid_argument);
}

TEST_F(ErrorCalibrationTest, NegativeValuesInLogCalibration) {
    std::vector<double> measured = {-1.0, 2.0, 3.0};
    std::vector<double> actual = {2.0, 4.0, 6.0};

    EXPECT_THROW(calibrator->logarithmicCalibrate(measured, actual),
                 std::invalid_argument);
}

#endif  // ATOM_ALGORITHM_TEST_ERROR_CALIBRATION_HPP