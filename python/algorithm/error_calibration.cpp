// error_calibration_bindings.cpp
#include "atom/algorithm/error_calibration.hpp"
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "atom/error/exception.hpp"

namespace py = pybind11;

// Helper function to handle the AsyncCalibrationTask
template <typename T>
auto handle_async_calibration(atom::algorithm::AsyncCalibrationTask<T>&& task) {
    // Get the calibrator result
    auto* calibrator = task.getResult();
    if (calibrator == nullptr) {
        throw py::value_error("Async calibration failed");
    }

    // Return the calibrator wrapped as a Python object with ownership transfer
    return py::cast(calibrator, py::return_value_policy::take_ownership);
}

PYBIND11_MODULE(error_calibration, m) {
    m.doc() = R"pbdoc(
        Error Calibration Module
        -----------------------

        This module provides tools for error calibration of measurement data.
        It includes methods for linear, polynomial, exponential, logarithmic,
        and power law calibration, as well as tools for statistical analysis.

        Examples:
            >>> import numpy as np
            >>> from atom.algorithm.error_calibration import ErrorCalibration
            >>>
            >>> # Sample data
            >>> measured = [1.0, 2.0, 3.0, 4.0, 5.0]
            >>> actual = [0.9, 2.1, 2.8, 4.2, 4.9]
            >>>
            >>> # Create calibrator and perform linear calibration
            >>> calibrator = ErrorCalibration()
            >>> calibrator.linear_calibrate(measured, actual)
            >>>
            >>> # Print calibration parameters
            >>> print(f"Slope: {calibrator.get_slope()}")
            >>> print(f"Intercept: {calibrator.get_intercept()}")
            >>> print(f"R-squared: {calibrator.get_r_squared()}")
            >>>
            >>> # Apply calibration to new measurements
            >>> new_measurement = 3.5
            >>> calibrated_value = calibrator.apply(new_measurement)
            >>> print(f"Calibrated value: {calibrated_value}")
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::error::FailToOpenFile& e) {
            PyErr_SetString(PyExc_IOError, e.what());
        } catch (const atom::error::Exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the ErrorCalibration class for double (as the primary class)
    py::class_<atom::algorithm::ErrorCalibration<double>>(m, "ErrorCalibration",
                                                          R"pbdoc(
        Error calibration class for measurement data.

        This class provides methods for calibrating measurements and analyzing errors
        using various calibration techniques, including linear, polynomial, exponential,
        logarithmic, and power law models.
    )pbdoc")
        .def(py::init<>())
        .def("linear_calibrate",
             &atom::algorithm::ErrorCalibration<double>::linearCalibrate,
             py::arg("measured"), py::arg("actual"),
             R"pbdoc(
             Perform linear calibration using the least squares method.

             Args:
                 measured: List of measured values
                 actual: List of actual values

             Raises:
                 ValueError: If input vectors are empty or of unequal size
             )pbdoc")
        .def("polynomial_calibrate",
             &atom::algorithm::ErrorCalibration<double>::polynomialCalibrate,
             py::arg("measured"), py::arg("actual"), py::arg("degree"),
             R"pbdoc(
             Perform polynomial calibration using the least squares method.

             Args:
                 measured: List of measured values
                 actual: List of actual values
                 degree: Degree of the polynomial

             Raises:
                 ValueError: If input vectors are empty, of unequal size, or if degree is invalid
             )pbdoc")
        .def("exponential_calibrate",
             &atom::algorithm::ErrorCalibration<double>::exponentialCalibrate,
             py::arg("measured"), py::arg("actual"),
             R"pbdoc(
             Perform exponential calibration using the least squares method.

             Args:
                 measured: List of measured values
                 actual: List of actual values

             Raises:
                 ValueError: If input vectors are empty, of unequal size, or if actual values are not positive
             )pbdoc")
        .def("logarithmic_calibrate",
             &atom::algorithm::ErrorCalibration<double>::logarithmicCalibrate,
             py::arg("measured"), py::arg("actual"),
             R"pbdoc(
             Perform logarithmic calibration using the least squares method.

             Args:
                 measured: List of measured values
                 actual: List of actual values

             Raises:
                 ValueError: If input vectors are empty, of unequal size, or if measured values are not positive
             )pbdoc")
        .def("power_law_calibrate",
             &atom::algorithm::ErrorCalibration<double>::powerLawCalibrate,
             py::arg("measured"), py::arg("actual"),
             R"pbdoc(
             Perform power law calibration using the least squares method.

             Args:
                 measured: List of measured values
                 actual: List of actual values

             Raises:
                 ValueError: If input vectors are empty, of unequal size, or if values are not positive
             )pbdoc")
        .def("apply", &atom::algorithm::ErrorCalibration<double>::apply,
             py::arg("value"), "Apply calibration to a measured value")
        .def("print_parameters",
             &atom::algorithm::ErrorCalibration<double>::printParameters,
             "Print calibration parameters to the log")
        .def("get_residuals",
             &atom::algorithm::ErrorCalibration<double>::getResiduals,
             "Get residuals from the calibration")
        .def("plot_residuals",
             &atom::algorithm::ErrorCalibration<double>::plotResiduals,
             py::arg("filename"),
             R"pbdoc(
             Save residuals to a CSV file for plotting.

             Args:
                 filename: Path to the output file

             Raises:
                 IOError: If the file cannot be opened
             )pbdoc")
        .def("bootstrap_confidence_interval",
             &atom::algorithm::ErrorCalibration<
                 double>::bootstrapConfidenceInterval,
             py::arg("measured"), py::arg("actual"),
             py::arg("n_iterations") = 1000, py::arg("confidence_level") = 0.95,
             R"pbdoc(
             Calculate bootstrap confidence interval for the slope.

             Args:
                 measured: List of measured values
                 actual: List of actual values
                 n_iterations: Number of bootstrap iterations (default: 1000)
                 confidence_level: Confidence level (default: 0.95)

             Returns:
                 Tuple of lower and upper bounds of the confidence interval

             Raises:
                 ValueError: If input parameters are invalid
             )pbdoc")
        .def("outlier_detection",
             &atom::algorithm::ErrorCalibration<double>::outlierDetection,
             py::arg("measured"), py::arg("actual"), py::arg("threshold") = 2.0,
             R"pbdoc(
             Detect outliers using the residuals of the calibration.

             Args:
                 measured: List of measured values
                 actual: List of actual values
                 threshold: Z-score threshold for outlier detection (default: 2.0)

             Returns:
                 Tuple of mean residual, standard deviation, and threshold

             Raises:
                 RuntimeError: If metrics have not been calculated yet
             )pbdoc")
        .def("cross_validation",
             &atom::algorithm::ErrorCalibration<double>::crossValidation,
             py::arg("measured"), py::arg("actual"), py::arg("k") = 5,
             R"pbdoc(
             Perform k-fold cross-validation of the calibration.

             Args:
                 measured: List of measured values
                 actual: List of actual values
                 k: Number of folds (default: 5)

             Raises:
                 ValueError: If input vectors are invalid
                 RuntimeError: If all cross-validation folds fail
             )pbdoc")
        .def("get_slope", &atom::algorithm::ErrorCalibration<double>::getSlope,
             "Get the calibration slope")
        .def("get_intercept",
             &atom::algorithm::ErrorCalibration<double>::getIntercept,
             "Get the calibration intercept")
        .def(
            "get_r_squared",
            [](const atom::algorithm::ErrorCalibration<double>& self)
                -> py::object {
                auto r_squared = self.getRSquared();
                if (r_squared.has_value()) {
                    return py::cast(r_squared.value());
                } else {
                    return py::none();
                }
            },
            "Get the coefficient of determination (R-squared) if available")
        .def("get_mse", &atom::algorithm::ErrorCalibration<double>::getMse,
             "Get the Mean Squared Error (MSE)")
        .def("get_mae", &atom::algorithm::ErrorCalibration<double>::getMae,
             "Get the Mean Absolute Error (MAE)");

    // Also expose the float precision version
    py::class_<atom::algorithm::ErrorCalibration<float>>(
        m, "ErrorCalibrationFloat", R"pbdoc(
        Error calibration class with single precision (float).

        This class is identical to ErrorCalibration but uses single precision
        floating point calculations, which may be faster but less accurate.
    )pbdoc")
        .def(py::init<>())
        .def("linear_calibrate",
             &atom::algorithm::ErrorCalibration<float>::linearCalibrate,
             py::arg("measured"), py::arg("actual"))
        .def("polynomial_calibrate",
             &atom::algorithm::ErrorCalibration<float>::polynomialCalibrate,
             py::arg("measured"), py::arg("actual"), py::arg("degree"))
        .def("exponential_calibrate",
             &atom::algorithm::ErrorCalibration<float>::exponentialCalibrate,
             py::arg("measured"), py::arg("actual"))
        .def("logarithmic_calibrate",
             &atom::algorithm::ErrorCalibration<float>::logarithmicCalibrate,
             py::arg("measured"), py::arg("actual"))
        .def("power_law_calibrate",
             &atom::algorithm::ErrorCalibration<float>::powerLawCalibrate,
             py::arg("measured"), py::arg("actual"))
        .def("apply", &atom::algorithm::ErrorCalibration<float>::apply,
             py::arg("value"))
        .def("print_parameters",
             &atom::algorithm::ErrorCalibration<float>::printParameters)
        .def("get_residuals",
             &atom::algorithm::ErrorCalibration<float>::getResiduals)
        .def("plot_residuals",
             &atom::algorithm::ErrorCalibration<float>::plotResiduals,
             py::arg("filename"))
        .def("bootstrap_confidence_interval",
             &atom::algorithm::ErrorCalibration<
                 float>::bootstrapConfidenceInterval,
             py::arg("measured"), py::arg("actual"),
             py::arg("n_iterations") = 1000, py::arg("confidence_level") = 0.95)
        .def("outlier_detection",
             &atom::algorithm::ErrorCalibration<float>::outlierDetection,
             py::arg("measured"), py::arg("actual"), py::arg("threshold") = 2.0)
        .def("cross_validation",
             &atom::algorithm::ErrorCalibration<float>::crossValidation,
             py::arg("measured"), py::arg("actual"), py::arg("k") = 5)
        .def("get_slope", &atom::algorithm::ErrorCalibration<float>::getSlope)
        .def("get_intercept",
             &atom::algorithm::ErrorCalibration<float>::getIntercept)
        .def("get_r_squared",
             [](const atom::algorithm::ErrorCalibration<float>& self)
                 -> py::object {
                 auto r_squared = self.getRSquared();
                 if (r_squared.has_value()) {
                     return py::cast(r_squared.value());
                 } else {
                     return py::none();
                 }
             })
        .def("get_mse", &atom::algorithm::ErrorCalibration<float>::getMse)
        .def("get_mae", &atom::algorithm::ErrorCalibration<float>::getMae);

    // Register the async calibration function
    m.def(
        "calibrate_async",
        [](const std::vector<double>& measured,
           const std::vector<double>& actual) {
            auto task =
                atom::algorithm::calibrateAsync<double>(measured, actual);
            return handle_async_calibration(std::move(task));
        },
        py::arg("measured"), py::arg("actual"),
        R"pbdoc(
       Perform asynchronous linear calibration.

       This function starts a calibration in a background thread and returns the calibrator
       once the calibration is complete.

       Args:
           measured: List of measured values
           actual: List of actual values

       Returns:
           ErrorCalibration object with the calibration results

       Raises:
           ValueError: If the calibration fails
       )pbdoc");

    // Add utility functions for common calibration tasks
    m.def(
        "find_best_calibration",
        [](const std::vector<double>& measured,
           const std::vector<double>& actual) {
            // Create instances of each calibration type and find the best one
            std::vector<std::pair<std::string, double>> results;

            try {
                atom::algorithm::ErrorCalibration<double> linear;
                linear.linearCalibrate(measured, actual);
                results.emplace_back("linear", linear.getMse());
            } catch (const std::exception& e) {
                py::print("Linear calibration failed:", e.what());
            }

            try {
                atom::algorithm::ErrorCalibration<double> poly2;
                poly2.polynomialCalibrate(measured, actual, 2);
                results.emplace_back("polynomial_2", poly2.getMse());
            } catch (const std::exception& e) {
                py::print("Polynomial (degree 2) calibration failed:",
                          e.what());
            }

            try {
                atom::algorithm::ErrorCalibration<double> poly3;
                poly3.polynomialCalibrate(measured, actual, 3);
                results.emplace_back("polynomial_3", poly3.getMse());
            } catch (const std::exception& e) {
                py::print("Polynomial (degree 3) calibration failed:",
                          e.what());
            }

            try {
                atom::algorithm::ErrorCalibration<double> exp;
                exp.exponentialCalibrate(measured, actual);
                results.emplace_back("exponential", exp.getMse());
            } catch (const std::exception& e) {
                py::print("Exponential calibration failed:", e.what());
            }

            try {
                atom::algorithm::ErrorCalibration<double> log;
                log.logarithmicCalibrate(measured, actual);
                results.emplace_back("logarithmic", log.getMse());
            } catch (const std::exception& e) {
                py::print("Logarithmic calibration failed:", e.what());
            }

            try {
                atom::algorithm::ErrorCalibration<double> power;
                power.powerLawCalibrate(measured, actual);
                results.emplace_back("power_law", power.getMse());
            } catch (const std::exception& e) {
                py::print("Power law calibration failed:", e.what());
            }

            if (results.empty()) {
                throw py::value_error("All calibration methods failed");
            }

            // Find the best calibration method (lowest MSE)
            auto best_result =
                std::min_element(results.begin(), results.end(),
                                 [](const auto& a, const auto& b) {
                                     return a.second < b.second;
                                 });

            return best_result->first;
        },
        py::arg("measured"), py::arg("actual"),
        R"pbdoc(
       Find the best calibration method for the given data.

       This function tries different calibration methods and returns the name
       of the method with the lowest Mean Squared Error (MSE).

       Args:
           measured: List of measured values
           actual: List of actual values

       Returns:
           String with the name of the best calibration method

       Raises:
           ValueError: If all calibration methods fail
       )pbdoc");

    // Add helper to create calibrated numpy array
    m.def(
        "calibrate_array",
        [](py::array_t<double> measured_array,
           const atom::algorithm::ErrorCalibration<double>& calibrator) {
            auto measured = measured_array.unchecked<1>();
            py::array_t<double> calibrated_array(measured.shape(0));
            auto calibrated = calibrated_array.mutable_unchecked<1>();

            for (py::ssize_t i = 0; i < measured.shape(0); i++) {
                calibrated(i) = calibrator.apply(measured(i));
            }

            return calibrated_array;
        },
        py::arg("measured_array"), py::arg("calibrator"),
        R"pbdoc(
       Apply calibration to a numpy array of measurements.

       Args:
           measured_array: Numpy array of measured values
           calibrator: ErrorCalibration object

       Returns:
           Numpy array of calibrated values
       )pbdoc");

    // Add helper to plot calibration results
    m.def(
        "plot_calibration",
        [](const std::vector<double>& measured,
           const std::vector<double>& actual,
           const atom::algorithm::ErrorCalibration<double>& calibrator) {
            try {
                py::object plt = py::module::import("matplotlib.pyplot");

                // Calculate calibrated values
                std::vector<double> calibrated;
                calibrated.reserve(measured.size());
                for (double m : measured) {
                    calibrated.push_back(calibrator.apply(m));
                }

                // Create scatter plot of measured vs actual
                plt.attr("figure")();
                plt.attr("scatter")(measured, actual,
                                    py::arg("label") = "Original data");
                plt.attr("scatter")(measured, calibrated,
                                    py::arg("label") = "Calibrated data");

                // Plot the ideal line
                double min_val =
                    *std::min_element(measured.begin(), measured.end());
                double max_val =
                    *std::max_element(measured.begin(), measured.end());
                std::vector<double> line_x = {min_val, max_val};
                std::vector<double> line_y = {calibrator.apply(min_val),
                                              calibrator.apply(max_val)};
                plt.attr("plot")(line_x, line_y, "r--",
                                 py::arg("label") = "Calibration line");

                // Add labels and legend
                plt.attr("xlabel")("Measured");
                plt.attr("ylabel")("Actual");
                plt.attr("title")("Calibration Results");
                plt.attr("legend")();
                plt.attr("grid")(true);

                // Show the plot
                plt.attr("show")();

                return true;
            } catch (const py::error_already_set& e) {
                py::print("Error plotting calibration:", e.what());
                py::print("Make sure matplotlib is installed.");
                return false;
            }
        },
        py::arg("measured"), py::arg("actual"), py::arg("calibrator"),
        R"pbdoc(
       Plot calibration results using matplotlib.

       This function creates a scatter plot of measured vs actual values,
       as well as the calibrated values and the calibration line.

       Args:
           measured: List of measured values
           actual: List of actual values
           calibrator: ErrorCalibration object

       Returns:
           True if the plot was created successfully, False otherwise

       Note:
           This function requires matplotlib to be installed.
       )pbdoc");

    // Add a function for residual analysis
    m.def(
        "analyze_residuals",
        [](const atom::algorithm::ErrorCalibration<double>& calibrator,
           const std::vector<double>& measured,
           const std::vector<double>& actual) {
            try {
                py::object plt = py::module::import("matplotlib.pyplot");
                py::object np = py::module::import("numpy");
                py::object stats = py::module::import("scipy.stats");

                // Get residuals
                std::vector<double> residuals = calibrator.getResiduals();

                // Create a figure with 2x2 subplots
                plt.attr("figure")(py::arg("figsize") = py::make_tuple(12, 10));

                // Plot 1: Residuals vs measured
                plt.attr("subplot")(2, 2, 1);
                plt.attr("scatter")(measured, residuals);
                plt.attr("axhline")(0, py::arg("color") = "red",
                                    py::arg("linestyle") = "--");
                plt.attr("xlabel")("Measured values");
                plt.attr("ylabel")("Residuals");
                plt.attr("title")("Residuals vs Measured");
                plt.attr("grid")(true);

                // Plot 2: Histogram of residuals
                plt.attr("subplot")(2, 2, 2);
                plt.attr("hist")(residuals, py::arg("bins") = 20,
                                 py::arg("alpha") = 0.5);
                plt.attr("xlabel")("Residual value");
                plt.attr("ylabel")("Frequency");
                plt.attr("title")("Histogram of Residuals");
                plt.attr("grid")(true);

                // Plot 3: Q-Q plot (requires scipy.stats)
                plt.attr("subplot")(2, 2, 3);
                plt.attr("title")("Q-Q Plot of Residuals");

                // Calculate Q-Q plot data
                auto qq_data =
                    stats.attr("probplot")(residuals, py::arg("dist") = "norm");
                py::tuple points = qq_data.attr("__getitem__")(0);
                py::tuple slope_intercept = qq_data.attr("__getitem__")(1);

                // Extract x and y points
                py::array x_points = points[0];
                py::array y_points = points[1];

                // Calculate the trend line
                double slope = slope_intercept[0].cast<double>();
                double intercept = slope_intercept[1].cast<double>();

                // Plot the points and line
                plt.attr("scatter")(x_points, y_points);

                // Calculate the line values using numpy for proper array
                // operations
                py::object qq_line_y = np.attr("add")(
                    intercept, np.attr("multiply")(slope, x_points));
                plt.attr("plot")(x_points, qq_line_y, "r--");

                plt.attr("xlabel")("Theoretical Quantiles");
                plt.attr("ylabel")("Sample Quantiles");
                plt.attr("grid")(true);

                // Plot 4: Calibration curve
                plt.attr("subplot")(2, 2, 4);

                // Calculate calibrated values
                std::vector<double> calibrated;
                calibrated.reserve(measured.size());
                for (double m : measured) {
                    calibrated.push_back(calibrator.apply(m));
                }

                plt.attr("scatter")(measured, actual,
                                    py::arg("label") = "Original data");
                plt.attr("scatter")(measured, calibrated,
                                    py::arg("label") = "Calibrated data");

                // Plot the ideal line
                double min_val =
                    *std::min_element(measured.begin(), measured.end());
                double max_val =
                    *std::max_element(measured.begin(), measured.end());
                std::vector<double> line_x = {min_val, max_val};
                std::vector<double> line_y = {calibrator.apply(min_val),
                                              calibrator.apply(max_val)};
                plt.attr("plot")(line_x, line_y, "r--",
                                 py::arg("label") = "Calibration line");

                plt.attr("xlabel")("Measured");
                plt.attr("ylabel")("Actual/Calibrated");
                plt.attr("title")("Calibration Curve");
                plt.attr("legend")();
                plt.attr("grid")(true);

                // Add overall title and adjust layout
                plt.attr("tight_layout")();

                // Show the plot
                plt.attr("show")();

                // Calculate basic statistics
                double mean_residual =
                    std::accumulate(residuals.begin(), residuals.end(), 0.0) /
                    residuals.size();
                double ss = std::accumulate(
                    residuals.begin(), residuals.end(), 0.0,
                    [mean_residual](double a, double b) {
                        return a + (b - mean_residual) * (b - mean_residual);
                    });
                double std_dev = std::sqrt(ss / residuals.size());

                // Return statistics dictionary
                py::dict stats_dict;
                stats_dict["mean"] = mean_residual;
                stats_dict["std_dev"] = std_dev;
                stats_dict["mse"] = calibrator.getMse();
                stats_dict["mae"] = calibrator.getMae();
                stats_dict["r_squared"] =
                    calibrator.getRSquared().value_or(0.0);
                stats_dict["slope"] = calibrator.getSlope();
                stats_dict["intercept"] = calibrator.getIntercept();

                return stats_dict;

            } catch (const py::error_already_set& e) {
                py::print("Error analyzing residuals:", e.what());
                py::print("Make sure matplotlib and scipy are installed.");
                return py::dict();
            }
        },
        py::arg("calibrator"), py::arg("measured"), py::arg("actual"),
        R"pbdoc(
       Analyze residuals with comprehensive plots and statistics.

       This function creates a set of diagnostic plots for analyzing residuals:
       1. Residuals vs measured values
       2. Histogram of residuals
       3. Q-Q plot for normality check
       4. Calibration curve

       Args:
           calibrator: ErrorCalibration object
           measured: List of measured values
           actual: List of actual values

       Returns:
           Dictionary with residual statistics (mean, std_dev, mse, mae, r_squared, slope, intercept)

       Note:
           This function requires matplotlib and scipy to be installed.
       )pbdoc");
}
