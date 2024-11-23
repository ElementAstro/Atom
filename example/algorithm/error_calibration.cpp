#include "error_calibration.hpp"
#include <iostream>
#include <vector>

int main() {
    // Example data
    std::vector<double> measured = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> actual = {1.1, 1.9, 3.2, 3.8, 5.1};

    // Create an instance of AdvancedErrorCalibration
    atom::algorithm::AdvancedErrorCalibration<double> calibrator;

    // Perform linear calibration
    calibrator.linearCalibrate(measured, actual);
    calibrator.printParameters();

    // Perform polynomial calibration
    calibrator.polynomialCalibrate(measured, actual, 2);
    calibrator.printParameters();

    // Perform exponential calibration
    calibrator.exponentialCalibrate(measured, actual);
    calibrator.printParameters();

    // Apply the calibration to a new value
    double newValue = 6.0;
    double calibratedValue = calibrator.apply(newValue);
    std::cout << "Calibrated value for " << newValue << ": " << calibratedValue
              << std::endl;

    // Get residuals
    std::vector<double> residuals = calibrator.getResiduals();
    std::cout << "Residuals: ";
    for (double res : residuals) {
        std::cout << res << " ";
    }
    std::cout << std::endl;

    // Plot residuals to a file
    calibrator.plotResiduals("residuals.csv");

    // Bootstrap confidence interval for the slope
    auto [lowerBound, upperBound] =
        calibrator.bootstrapConfidenceInterval(measured, actual);
    std::cout << "Bootstrap confidence interval for the slope: [" << lowerBound
              << ", " << upperBound << "]" << std::endl;

    // Detect outliers
    auto [meanResidual, stdDev, threshold] =
        calibrator.outlierDetection(measured, actual);
    std::cout << "Outlier detection - Mean residual: " << meanResidual
              << ", Standard deviation: " << stdDev
              << ", Threshold: " << threshold << std::endl;

    // Perform cross-validation
    calibrator.crossValidation(measured, actual, 5);

    return 0;
}