/*
 * statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Python bindings for statistics from atom/algorithm/math/statistics.hpp
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/algorithm/math/statistics.hpp"

namespace py = pybind11;
using namespace atom::algorithm;

PYBIND11_MODULE(statistics, m) {
    m.doc() = R"pbdoc(
        Statistics Module
        -----------------

        This module provides statistical functions and utilities:
        - Descriptive statistics (mean, median, mode, variance, etc.)
        - Correlation and covariance
        - Percentiles and quartiles
        - Z-score normalization

        Example:
            >>> from atom.algorithm.math import statistics
            >>> data = [1.0, 2.0, 3.0, 4.0, 5.0]
            >>> print(f"Mean: {statistics.mean(data)}")
            >>> print(f"Std Dev: {statistics.standard_deviation(data)}")
    )pbdoc";

    // Statistics<double> bindings
    using Stats = Statistics<double>;

    // Mean
    m.def(
        "mean",
        [](const std::vector<double>& data) { return Stats::mean(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the arithmetic mean of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Arithmetic mean

            Example:
                >>> mean([1.0, 2.0, 3.0, 4.0, 5.0])
                3.0
        )pbdoc");

    // Median
    m.def(
        "median",
        [](std::vector<double> data) { return Stats::median(std::move(data)); },
        py::arg("data"),
        R"pbdoc(
            Calculate the median of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Median value

            Example:
                >>> median([1.0, 2.0, 3.0, 4.0, 5.0])
                3.0
        )pbdoc");

    // Mode
    m.def(
        "mode",
        [](const std::vector<double>& data) { return Stats::mode(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the mode(s) of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                List of mode values (can be multiple)

            Example:
                >>> mode([1.0, 2.0, 2.0, 3.0, 3.0, 3.0])
                [3.0]
        )pbdoc");

    // Variance
    m.def(
        "variance",
        [](const std::vector<double>& data, bool sample_correction) {
            return Stats::variance(data, sample_correction);
        },
        py::arg("data"), py::arg("sample_correction") = true,
        R"pbdoc(
            Calculate the variance of a dataset.

            Args:
                data: Input data as a list of numbers
                sample_correction: Use sample correction (n-1 denominator) if True

            Returns:
                Variance value

            Example:
                >>> variance([1.0, 2.0, 3.0, 4.0, 5.0])
                2.5
        )pbdoc");

    // Standard deviation
    m.def(
        "standard_deviation",
        [](const std::vector<double>& data, bool sample_correction) {
            return Stats::standardDeviation(data, sample_correction);
        },
        py::arg("data"), py::arg("sample_correction") = true,
        R"pbdoc(
            Calculate the standard deviation of a dataset.

            Args:
                data: Input data as a list of numbers
                sample_correction: Use sample correction if True

            Returns:
                Standard deviation value

            Example:
                >>> standard_deviation([1.0, 2.0, 3.0, 4.0, 5.0])
                1.5811388300841898
        )pbdoc");

    // Skewness
    m.def(
        "skewness",
        [](const std::vector<double>& data) { return Stats::skewness(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the skewness of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Skewness value (0 for symmetric, positive for right-skewed,
                negative for left-skewed)

            Example:
                >>> skewness([1.0, 2.0, 3.0, 4.0, 5.0])
                0.0
        )pbdoc");

    // Kurtosis
    m.def(
        "kurtosis",
        [](const std::vector<double>& data) { return Stats::kurtosis(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the kurtosis of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Kurtosis value (3 for normal distribution)

            Example:
                >>> kurtosis([1.0, 2.0, 3.0, 4.0, 5.0])
                1.7
        )pbdoc");

    // Covariance
    m.def(
        "covariance",
        [](const std::vector<double>& x, const std::vector<double>& y) {
            return Stats::covariance(x, y);
        },
        py::arg("x"), py::arg("y"),
        R"pbdoc(
            Calculate the covariance between two datasets.

            Args:
                x: First dataset
                y: Second dataset (must have same length as x)

            Returns:
                Covariance value

            Example:
                >>> x = [1.0, 2.0, 3.0, 4.0, 5.0]
                >>> y = [2.0, 4.0, 6.0, 8.0, 10.0]
                >>> covariance(x, y)
                5.0
        )pbdoc");

    // Correlation
    m.def(
        "correlation",
        [](const std::vector<double>& x, const std::vector<double>& y) {
            return Stats::correlation(x, y);
        },
        py::arg("x"), py::arg("y"),
        R"pbdoc(
            Calculate the Pearson correlation coefficient between two datasets.

            Args:
                x: First dataset
                y: Second dataset (must have same length as x)

            Returns:
                Correlation coefficient (-1 to 1)

            Example:
                >>> x = [1.0, 2.0, 3.0, 4.0, 5.0]
                >>> y = [2.0, 4.0, 6.0, 8.0, 10.0]
                >>> correlation(x, y)
                1.0
        )pbdoc");

    // Percentile
    m.def(
        "percentile",
        [](std::vector<double> data, double p) {
            return Stats::percentile(std::move(data), p);
        },
        py::arg("data"), py::arg("p"),
        R"pbdoc(
            Calculate the p-th percentile of a dataset.

            Args:
                data: Input data as a list of numbers
                p: Percentile to compute (0-100)

            Returns:
                Percentile value

            Example:
                >>> percentile([1.0, 2.0, 3.0, 4.0, 5.0], 50.0)
                3.0
        )pbdoc");

    // Min
    m.def(
        "min", [](const std::vector<double>& data) { return Stats::min(data); },
        py::arg("data"),
        R"pbdoc(
            Find the minimum value in a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Minimum value
        )pbdoc");

    // Max
    m.def(
        "max", [](const std::vector<double>& data) { return Stats::max(data); },
        py::arg("data"),
        R"pbdoc(
            Find the maximum value in a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Maximum value
        )pbdoc");

    // Range
    m.def(
        "range",
        [](const std::vector<double>& data) { return Stats::range(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the range (max - min) of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Range value
        )pbdoc");

    // Z-score normalization
    m.def(
        "z_score_normalize",
        [](const std::vector<double>& data) {
            return Stats::zScoreNormalize(data);
        },
        py::arg("data"),
        R"pbdoc(
            Normalize data using z-score (standardization).

            Args:
                data: Input data as a list of numbers

            Returns:
                List of z-scores (mean=0, std=1)

            Example:
                >>> z_score_normalize([1.0, 2.0, 3.0, 4.0, 5.0])
                [-1.2649..., -0.6324..., 0.0, 0.6324..., 1.2649...]
        )pbdoc");

    // Sum
    m.def(
        "sum", [](const std::vector<double>& data) { return Stats::sum(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the sum of all values in a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Sum of all values
        )pbdoc");

    // Product
    m.def(
        "product",
        [](const std::vector<double>& data) { return Stats::product(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the product of all values in a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                Product of all values
        )pbdoc");

    // Geometric mean
    m.def(
        "geometric_mean",
        [](const std::vector<double>& data) {
            return Stats::geometricMean(data);
        },
        py::arg("data"),
        R"pbdoc(
            Calculate the geometric mean of a dataset.

            Args:
                data: Input data as a list of positive numbers

            Returns:
                Geometric mean

            Note:
                All values must be positive.
        )pbdoc");

    // Harmonic mean
    m.def(
        "harmonic_mean",
        [](const std::vector<double>& data) {
            return Stats::harmonicMean(data);
        },
        py::arg("data"),
        R"pbdoc(
            Calculate the harmonic mean of a dataset.

            Args:
                data: Input data as a list of positive numbers

            Returns:
                Harmonic mean

            Note:
                All values must be positive and non-zero.
        )pbdoc");

    // Root mean square
    m.def(
        "rms", [](const std::vector<double>& data) { return Stats::rms(data); },
        py::arg("data"),
        R"pbdoc(
            Calculate the root mean square (RMS) of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                RMS value
        )pbdoc");

    // Interquartile range
    m.def(
        "iqr",
        [](std::vector<double> data) { return Stats::iqr(std::move(data)); },
        py::arg("data"),
        R"pbdoc(
            Calculate the interquartile range (IQR) of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                IQR value (Q3 - Q1)
        )pbdoc");

    // Coefficient of variation
    m.def(
        "coefficient_of_variation",
        [](const std::vector<double>& data) {
            return Stats::coefficientOfVariation(data);
        },
        py::arg("data"),
        R"pbdoc(
            Calculate the coefficient of variation (CV) of a dataset.

            Args:
                data: Input data as a list of numbers

            Returns:
                CV value (std_dev / mean)

            Note:
                Useful for comparing variability between datasets with
                different units or means.
        )pbdoc");
}
