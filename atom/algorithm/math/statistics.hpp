#ifndef ATOM_ALGORITHM_MATH_STATISTICS_HPP
#define ATOM_ALGORITHM_MATH_STATISTICS_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <numeric>
#include <span>
#include <unordered_map>
#include <vector>

#include "../rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Statistical functions and utilities
 *
 * This class provides common statistical operations including:
 * - Descriptive statistics (mean, median, mode, variance, etc.)
 * - Correlation and covariance
 * - Probability distributions
 * - Hypothesis testing utilities
 */
template <std::floating_point T>
class Statistics {
public:
    /**
     * @brief Calculate the arithmetic mean of a dataset
     * @param data Input data
     * @return Arithmetic mean
     */
    [[nodiscard]] static auto mean(std::span<const T> data) -> T {
        if (data.empty()) {
            return T{0};
        }
        return std::accumulate(data.begin(), data.end(), T{0}) /
               static_cast<T>(data.size());
    }

    /**
     * @brief Calculate the median of a dataset
     * @param data Input data (will be partially reordered)
     * @return Median value
     */
    [[nodiscard]] static auto median(std::vector<T> data) -> T {
        if (data.empty()) {
            return T{0};
        }

        usize n = data.size();
        auto mid = data.begin() + static_cast<std::ptrdiff_t>(n / 2);

        // Use nth_element for O(n) average complexity instead of O(n log n)
        // sort
        std::nth_element(data.begin(), mid, data.end());

        if (n % 2 == 0) {
            // For even size, need to find max of left partition
            auto max_left = std::max_element(data.begin(), mid);
            return (*max_left + *mid) / T{2};
        } else {
            return *mid;
        }
    }

    /**
     * @brief Calculate the mode(s) of a dataset
     * @param data Input data
     * @return Vector of mode values (can be multiple)
     */
    [[nodiscard]] static auto mode(std::span<const T> data) -> std::vector<T> {
        if (data.empty()) {
            return {};
        }

        std::unordered_map<T, usize> frequency;
        for (T value : data) {
            frequency[value]++;
        }

        usize max_freq = 0;
        for (const auto& [value, freq] : frequency) {
            max_freq = std::max(max_freq, freq);
        }

        std::vector<T> modes;
        for (const auto& [value, freq] : frequency) {
            if (freq == max_freq) {
                modes.push_back(value);
            }
        }

        return modes;
    }

    /**
     * @brief Calculate the sample variance
     * @param data Input data
     * @param sample_correction Whether to use sample correction (n-1
     * denominator)
     * @return Sample variance
     */
    [[nodiscard]] static auto variance(std::span<const T> data,
                                       bool sample_correction = true) -> T {
        if (data.size() <= 1) {
            return T{0};
        }

        T mean_val = mean(data);
        T sum_sq_diff = std::transform_reduce(
            data.begin(), data.end(), T{0}, std::plus<T>{},
            [mean_val](T x) { return (x - mean_val) * (x - mean_val); });

        usize denominator = sample_correction ? data.size() - 1 : data.size();
        return sum_sq_diff / static_cast<T>(denominator);
    }

    /**
     * @brief Calculate the standard deviation
     * @param data Input data
     * @param sample_correction Whether to use sample correction
     * @return Standard deviation
     */
    [[nodiscard]] static auto standardDeviation(std::span<const T> data,
                                                bool sample_correction = true)
        -> T {
        return std::sqrt(variance(data, sample_correction));
    }

    /**
     * @brief Calculate the skewness of a dataset
     * @param data Input data
     * @return Skewness value
     */
    [[nodiscard]] static auto skewness(std::span<const T> data) -> T {
        if (data.size() < 3) {
            return T{0};
        }

        T mean_val = mean(data);
        T std_dev = standardDeviation(data);

        if (std_dev == T{0}) {
            return T{0};
        }

        T sum_cubed = std::transform_reduce(
            data.begin(), data.end(), T{0}, std::plus<T>{},
            [mean_val, std_dev](T x) {
                T normalized = (x - mean_val) / std_dev;
                return normalized * normalized * normalized;
            });

        return sum_cubed / static_cast<T>(data.size());
    }

    /**
     * @brief Calculate the kurtosis of a dataset
     * @param data Input data
     * @return Kurtosis value
     */
    [[nodiscard]] static auto kurtosis(std::span<const T> data) -> T {
        if (data.size() < 4) {
            return T{0};
        }

        T mean_val = mean(data);
        T std_dev = standardDeviation(data);

        if (std_dev == T{0}) {
            return T{0};
        }

        T sum_fourth =
            std::transform_reduce(data.begin(), data.end(), T{0},
                                  std::plus<T>{}, [mean_val, std_dev](T x) {
                                      T normalized = (x - mean_val) / std_dev;
                                      T squared = normalized * normalized;
                                      return squared * squared;
                                  });

        return (sum_fourth / static_cast<T>(data.size())) -
               T{3};  // Excess kurtosis
    }

    /**
     * @brief Calculate Pearson correlation coefficient between two datasets
     * @param x First dataset
     * @param y Second dataset
     * @return Correlation coefficient (-1 to 1)
     */
    [[nodiscard]] static auto correlation(std::span<const T> x,
                                          std::span<const T> y) -> T {
        if (x.size() != y.size() || x.empty()) {
            return T{0};
        }

        T mean_x = mean(x);
        T mean_y = mean(y);

        T numerator = T{0};
        T sum_sq_x = T{0};
        T sum_sq_y = T{0};

        for (usize i = 0; i < x.size(); ++i) {
            T diff_x = x[i] - mean_x;
            T diff_y = y[i] - mean_y;

            numerator += diff_x * diff_y;
            sum_sq_x += diff_x * diff_x;
            sum_sq_y += diff_y * diff_y;
        }

        T denominator = std::sqrt(sum_sq_x * sum_sq_y);
        return (denominator == T{0}) ? T{0} : numerator / denominator;
    }

    /**
     * @brief Calculate covariance between two datasets
     * @param x First dataset
     * @param y Second dataset
     * @param sample_correction Whether to use sample correction
     * @return Covariance
     */
    [[nodiscard]] static auto covariance(std::span<const T> x,
                                         std::span<const T> y,
                                         bool sample_correction = true) -> T {
        if (x.size() != y.size() || x.empty()) {
            return T{0};
        }

        T mean_x = mean(x);
        T mean_y = mean(y);

        T sum_products = T{0};
        for (usize i = 0; i < x.size(); ++i) {
            sum_products += (x[i] - mean_x) * (y[i] - mean_y);
        }

        usize denominator = sample_correction ? x.size() - 1 : x.size();
        return sum_products / static_cast<T>(denominator);
    }

    /**
     * @brief Calculate percentile of a dataset
     * @param data Input data (will be modified for sorting)
     * @param percentile Percentile to calculate (0-100)
     * @return Percentile value
     */
    [[nodiscard]] static auto percentile(std::vector<T> data, T percentile)
        -> T {
        if (data.empty() || percentile < T{0} || percentile > T{100}) {
            return T{0};
        }

        std::sort(data.begin(), data.end());

        if (percentile == T{0}) {
            return data.front();
        }
        if (percentile == T{100}) {
            return data.back();
        }

        T index = (percentile / T{100}) * static_cast<T>(data.size() - 1);
        usize lower_index = static_cast<usize>(std::floor(index));
        usize upper_index = static_cast<usize>(std::ceil(index));

        if (lower_index == upper_index) {
            return data[lower_index];
        }

        T weight = index - static_cast<T>(lower_index);
        return data[lower_index] * (T{1} - weight) + data[upper_index] * weight;
    }

    /**
     * @brief Calculate the interquartile range (IQR)
     * @param data Input data
     * @return IQR value (Q3 - Q1)
     */
    [[nodiscard]] static auto interquartileRange(std::vector<T> data) -> T {
        T q1 = percentile(data, T{25});
        T q3 = percentile(data, T{75});
        return q3 - q1;
    }

    /**
     * @brief Detect outliers using the IQR method
     * @param data Input data
     * @param multiplier IQR multiplier for outlier detection (default: 1.5)
     * @return Vector of outlier values
     */
    [[nodiscard]] static auto detectOutliers(std::vector<T> data,
                                             T multiplier = T{1.5})
        -> std::vector<T> {
        if (data.size() < 4) {
            return {};
        }

        T q1 = percentile(data, T{25});
        T q3 = percentile(data, T{75});
        T iqr = q3 - q1;

        T lower_bound = q1 - multiplier * iqr;
        T upper_bound = q3 + multiplier * iqr;

        std::vector<T> outliers;
        for (T value : data) {
            if (value < lower_bound || value > upper_bound) {
                outliers.push_back(value);
            }
        }

        return outliers;
    }

    /**
     * @brief Calculate z-scores for a dataset
     * @param data Input data
     * @return Vector of z-scores
     */
    [[nodiscard]] static auto zScores(std::span<const T> data)
        -> std::vector<T> {
        if (data.empty()) {
            return {};
        }

        T mean_val = mean(data);
        T std_dev = standardDeviation(data);

        if (std_dev == T{0}) {
            return std::vector<T>(data.size(), T{0});
        }

        std::vector<T> z_scores;
        z_scores.reserve(data.size());

        for (T value : data) {
            z_scores.push_back((value - mean_val) / std_dev);
        }

        return z_scores;
    }
};

// Type aliases for common use cases
using StatisticsF = Statistics<f32>;
using StatisticsD = Statistics<f64>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_STATISTICS_HPP
