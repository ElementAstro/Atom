#ifndef ATOM_EXTRA_BOOST_MATH_HPP
#define ATOM_EXTRA_BOOST_MATH_HPP

#include <boost/math/distributions.hpp>
#include <boost/math/quadrature/trapezoidal.hpp>
#include <boost/math/special_functions.hpp>
#include <boost/math/statistics/univariate_statistics.hpp>

#include <boost/numeric/odeint.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/vector.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <execution>
#include <functional>
#include <numeric>
#include <random>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>
#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace atom::extra::boost {

/**
 * @brief Concept to check if a type is numeric
 * @tparam T The type to check
 */
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

/**
 * @brief Concept to check if a type is floating point
 * @tparam T The type to check
 */
template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

/**
 * @brief Enhanced mathematical constants with high precision
 */
template <FloatingPoint T>
struct MathConstants {
    static constexpr T PI =
        static_cast<T>(3.141592653589793238462643383279502884L);
    static constexpr T E =
        static_cast<T>(2.718281828459045235360287471352662498L);
    static constexpr T SQRT_2 =
        static_cast<T>(1.414213562373095048801688724209698079L);
    static constexpr T SQRT_PI =
        static_cast<T>(1.772453850905516027298167483341145182L);
    static constexpr T LN_2 =
        static_cast<T>(0.693147180559945309417232121458176568L);
    static constexpr T LN_10 =
        static_cast<T>(2.302585092994045684017991454684364208L);
    static constexpr T GOLDEN_RATIO =
        static_cast<T>(1.618033988749894848204586834365638118L);
    static constexpr T EULER_GAMMA =
        static_cast<T>(0.577215664901532860606512090082402431L);
};

/**
 * @brief SIMD-optimized vector operations
 */
template <FloatingPoint T>
class VectorizedMath {
public:
    /**
     * @brief SIMD-optimized vector addition
     * @param a First vector
     * @param b Second vector
     * @param result Output vector
     * @param size Vector size
     */
    static void vectorAdd(const T* a, const T* b, T* result,
                          size_t size) noexcept {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>) {
            vectorAddAVX(a, b, result, size);
        } else if constexpr (std::is_same_v<T, double>) {
            vectorAddAVXDouble(a, b, result, size);
        } else {
            vectorAddScalar(a, b, result, size);
        }
#else
        vectorAddScalar(a, b, result, size);
#endif
    }

    /**
     * @brief SIMD-optimized dot product
     * @param a First vector
     * @param b Second vector
     * @param size Vector size
     * @return Dot product result
     */
    static T dotProduct(const T* a, const T* b, size_t size) noexcept {
#ifdef __AVX2__
        if constexpr (std::is_same_v<T, float>) {
            return dotProductAVX(a, b, size);
        } else if constexpr (std::is_same_v<T, double>) {
            return dotProductAVXDouble(a, b, size);
        } else {
            return dotProductScalar(a, b, size);
        }
#else
        return dotProductScalar(a, b, size);
#endif
    }

private:
#ifdef __AVX2__
    static void vectorAddAVX(const float* a, const float* b, float* result,
                             size_t size) noexcept {
        size_t simd_size = size - (size % 8);
        for (size_t i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(&a[i]);
            __m256 vb = _mm256_loadu_ps(&b[i]);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(&result[i], vr);
        }
        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static void vectorAddAVXDouble(const double* a, const double* b,
                                   double* result, size_t size) noexcept {
        size_t simd_size = size - (size % 4);
        for (size_t i = 0; i < simd_size; i += 4) {
            __m256d va = _mm256_loadu_pd(&a[i]);
            __m256d vb = _mm256_loadu_pd(&b[i]);
            __m256d vr = _mm256_add_pd(va, vb);
            _mm256_storeu_pd(&result[i], vr);
        }
        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static float dotProductAVX(const float* a, const float* b,
                               size_t size) noexcept {
        __m256 sum = _mm256_setzero_ps();
        size_t simd_size = size - (size % 8);

        for (size_t i = 0; i < simd_size; i += 8) {
            __m256 va = _mm256_loadu_ps(&a[i]);
            __m256 vb = _mm256_loadu_ps(&b[i]);
            sum = _mm256_fmadd_ps(va, vb, sum);
        }

        // Horizontal sum
        alignas(32) float temp[8];
        _mm256_storeu_ps(temp, sum);
        float result = temp[0] + temp[1] + temp[2] + temp[3] + temp[4] +
                       temp[5] + temp[6] + temp[7];

        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }

    static double dotProductAVXDouble(const double* a, const double* b,
                                      size_t size) noexcept {
        __m256d sum = _mm256_setzero_pd();
        size_t simd_size = size - (size % 4);

        for (size_t i = 0; i < simd_size; i += 4) {
            __m256d va = _mm256_loadu_pd(&a[i]);
            __m256d vb = _mm256_loadu_pd(&b[i]);
            sum = _mm256_fmadd_pd(va, vb, sum);
        }

        // Horizontal sum
        alignas(32) double temp[4];
        _mm256_storeu_pd(temp, sum);
        double result = temp[0] + temp[1] + temp[2] + temp[3];

        // Handle remaining elements
        for (size_t i = simd_size; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

    static void vectorAddScalar(const T* a, const T* b, T* result,
                                size_t size) noexcept {
        for (size_t i = 0; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static T dotProductScalar(const T* a, const T* b, size_t size) noexcept {
        T result = T{0};
        for (size_t i = 0; i < size; ++i) {
            result += a[i] * b[i];
        }
        return result;
    }
};

/**
 * @brief Wrapper class for special mathematical functions
 * @tparam T The numeric type
 */
template <Numeric T>
class SpecialFunctions {
public:
    /**
     * @brief Computes the beta function
     * @param alpha The alpha parameter
     * @param beta The beta parameter
     * @return The value of the beta function
     */
    [[nodiscard]] static constexpr T beta(T alpha, T beta) noexcept {
        return ::boost::math::beta(alpha, beta);
    }

    /**
     * @brief Computes the gamma function
     * @param value The input value
     * @return The value of the gamma function
     */
    [[nodiscard]] static constexpr T gamma(T value) noexcept {
        return ::boost::math::tgamma(value);
    }

    /**
     * @brief Computes the digamma function
     * @param value The input value
     * @return The value of the digamma function
     */
    [[nodiscard]] static constexpr T digamma(T value) noexcept {
        return ::boost::math::digamma(value);
    }

    /**
     * @brief Computes the error function
     * @param value The input value
     * @return The value of the error function
     */
    [[nodiscard]] static constexpr T erf(T value) noexcept {
        return ::boost::math::erf(value);
    }

    /**
     * @brief Computes the Bessel function of the first kind
     * @param order The order of the Bessel function
     * @param value The input value
     * @return The value of the Bessel function
     */
    [[nodiscard]] static constexpr T besselJ(int order, T value) noexcept {
        return ::boost::math::cyl_bessel_j(order, value);
    }

    /**
     * @brief Computes the Legendre polynomial
     * @param order The order of the polynomial
     * @param value The input value
     * @return The value of the Legendre polynomial
     */
    [[nodiscard]] static constexpr T legendreP(int order, T value) noexcept {
        return ::boost::math::legendre_p(order, value);
    }
};

/**
 * @brief Enhanced wrapper class for statistical functions with parallel
 * processing
 * @tparam T The numeric type
 */
template <Numeric T>
class Statistics {
private:
    static std::atomic<uint64_t> computation_count_;
    static thread_local std::unordered_map<std::string, T> cache_;

public:
    /**
     * @brief Computes the mean of a dataset with optional parallel processing
     * @param data The input dataset
     * @param use_parallel Whether to use parallel execution for large datasets
     * @return The mean of the dataset
     */
    [[nodiscard]] static T mean(const std::vector<T>& data,
                                bool use_parallel = true) {
        ++computation_count_;

        if (data.empty())
            return T{0};

        if (use_parallel && data.size() > 10000) {
            return std::reduce(std::execution::par_unseq, data.begin(),
                               data.end(), T{0}) /
                   static_cast<T>(data.size());
        } else {
            return ::boost::math::statistics::mean(data);
        }
    }

    /**
     * @brief Computes the variance of a dataset with enhanced precision
     * @param data The input dataset
     * @param use_parallel Whether to use parallel execution
     * @return The variance of the dataset
     */
    [[nodiscard]] static T variance(const std::vector<T>& data,
                                    bool use_parallel = true) {
        ++computation_count_;

        if (data.size() < 2)
            return T{0};

        if (use_parallel && data.size() > 10000) {
            T data_mean = mean(data, use_parallel);
            T sum_sq_diff = std::transform_reduce(
                std::execution::par_unseq, data.begin(), data.end(), T{0},
                std::plus<T>{},
                [data_mean](T x) { return (x - data_mean) * (x - data_mean); });
            return sum_sq_diff / static_cast<T>(data.size() - 1);
        } else {
            return ::boost::math::statistics::variance(data);
        }
    }

    /**
     * @brief Computes the standard deviation
     * @param data The input dataset
     * @param use_parallel Whether to use parallel execution
     * @return The standard deviation
     */
    [[nodiscard]] static T standardDeviation(const std::vector<T>& data,
                                             bool use_parallel = true) {
        return std::sqrt(variance(data, use_parallel));
    }

    /**
     * @brief Computes the skewness of a dataset
     * @param data The input dataset
     * @return The skewness of the dataset
     */
    [[nodiscard]] static T skewness(const std::vector<T>& data) {
        ++computation_count_;
        return ::boost::math::statistics::skewness(data);
    }

    /**
     * @brief Computes the kurtosis of a dataset
     * @param data The input dataset
     * @return The kurtosis of the dataset
     */
    [[nodiscard]] static T kurtosis(const std::vector<T>& data) {
        ++computation_count_;
        return ::boost::math::statistics::kurtosis(data);
    }

    /**
     * @brief Computes percentiles of a dataset
     * @param data The input dataset
     * @param percentiles Vector of percentiles to compute (0-100)
     * @return Vector of percentile values
     */
    [[nodiscard]] static std::vector<T> percentiles(
        std::vector<T> data, const std::vector<T>& percentiles) {
        ++computation_count_;

        if (data.empty())
            return {};

        std::sort(std::execution::par_unseq, data.begin(), data.end());

        std::vector<T> result;
        result.reserve(percentiles.size());

        for (T p : percentiles) {
            if (p < 0 || p > 100) {
                throw std::invalid_argument(
                    "Percentile must be between 0 and 100");
            }

            T index = (p / 100.0) * (data.size() - 1);
            size_t lower = static_cast<size_t>(std::floor(index));
            size_t upper = static_cast<size_t>(std::ceil(index));

            if (lower == upper) {
                result.push_back(data[lower]);
            } else {
                T weight = index - lower;
                result.push_back(data[lower] * (1 - weight) +
                                 data[upper] * weight);
            }
        }

        return result;
    }

    /**
     * @brief Computes the median of a dataset
     * @param data The input dataset
     * @return The median value
     */
    [[nodiscard]] static T median(std::vector<T> data) {
        auto result = percentiles(data, {50.0});
        return result.empty() ? T{0} : result[0];
    }

    /**
     * @brief Computes the correlation coefficient between two datasets
     * @param x First dataset
     * @param y Second dataset
     * @return Pearson correlation coefficient
     */
    [[nodiscard]] static T correlation(const std::vector<T>& x,
                                       const std::vector<T>& y) {
        ++computation_count_;

        if (x.size() != y.size() || x.empty()) {
            throw std::invalid_argument(
                "Datasets must have the same non-zero size");
        }

        T mean_x = mean(x);
        T mean_y = mean(y);

        T numerator = T{0};
        T sum_sq_x = T{0};
        T sum_sq_y = T{0};

        for (size_t i = 0; i < x.size(); ++i) {
            T diff_x = x[i] - mean_x;
            T diff_y = y[i] - mean_y;
            numerator += diff_x * diff_y;
            sum_sq_x += diff_x * diff_x;
            sum_sq_y += diff_y * diff_y;
        }

        T denominator = std::sqrt(sum_sq_x * sum_sq_y);
        return (denominator > T{0}) ? numerator / denominator : T{0};
    }

    /**
     * @brief Computes linear regression coefficients
     * @param x Independent variable
     * @param y Dependent variable
     * @return Pair of (slope, intercept)
     */
    [[nodiscard]] static std::pair<T, T> linearRegression(
        const std::vector<T>& x, const std::vector<T>& y) {
        ++computation_count_;

        if (x.size() != y.size() || x.empty()) {
            throw std::invalid_argument(
                "Datasets must have the same non-zero size");
        }

        T mean_x = mean(x);
        T mean_y = mean(y);

        T numerator = T{0};
        T denominator = T{0};

        for (size_t i = 0; i < x.size(); ++i) {
            T diff_x = x[i] - mean_x;
            numerator += diff_x * (y[i] - mean_y);
            denominator += diff_x * diff_x;
        }

        T slope = (denominator > T{0}) ? numerator / denominator : T{0};
        T intercept = mean_y - slope * mean_x;

        return {slope, intercept};
    }

    /**
     * @brief Gets computation statistics
     * @return Number of computations performed
     */
    [[nodiscard]] static uint64_t getComputationCount() {
        return computation_count_.load();
    }

    /**
     * @brief Resets computation statistics
     */
    static void resetStatistics() {
        computation_count_.store(0);
        cache_.clear();
    }
};

/**
 * @brief Machine Learning utilities with vectorized operations
 * @tparam T The numeric type
 */
template <FloatingPoint T>
class MachineLearning {
public:
    /**
     * @brief Sigmoid activation function with vectorization
     * @param x Input value or vector
     * @return Sigmoid output
     */
    [[nodiscard]] static T sigmoid(T x) noexcept {
        return T{1} / (T{1} + std::exp(-x));
    }

    /**
     * @brief Vectorized sigmoid function
     * @param input Input vector
     * @param output Output vector
     * @param size Vector size
     */
    static void sigmoidVector(const T* input, T* output, size_t size) noexcept {
        for (size_t i = 0; i < size; ++i) {
            output[i] = sigmoid(input[i]);
        }
    }

    /**
     * @brief ReLU activation function
     * @param x Input value
     * @return ReLU output
     */
    [[nodiscard]] static constexpr T relu(T x) noexcept {
        return std::max(T{0}, x);
    }

    /**
     * @brief Vectorized ReLU function
     * @param input Input vector
     * @param output Output vector
     * @param size Vector size
     */
    static void reluVector(const T* input, T* output, size_t size) noexcept {
        for (size_t i = 0; i < size; ++i) {
            output[i] = relu(input[i]);
        }
    }

    /**
     * @brief Softmax activation function
     * @param input Input vector
     * @param output Output vector
     * @param size Vector size
     */
    static void softmax(const T* input, T* output, size_t size) noexcept {
        // Find maximum for numerical stability
        T max_val = *std::max_element(input, input + size);

        // Compute exponentials and sum
        T sum = T{0};
        for (size_t i = 0; i < size; ++i) {
            output[i] = std::exp(input[i] - max_val);
            sum += output[i];
        }

        // Normalize
        for (size_t i = 0; i < size; ++i) {
            output[i] /= sum;
        }
    }

    /**
     * @brief K-means clustering (simplified implementation)
     * @param data Input data points (flattened)
     * @param dimensions Number of dimensions per point
     * @param k Number of clusters
     * @param max_iterations Maximum iterations
     * @return Cluster centers
     */
    [[nodiscard]] static std::vector<T> kmeans(const std::vector<T>& data,
                                               size_t dimensions, size_t k,
                                               size_t max_iterations = 100) {
        if (data.size() % dimensions != 0) {
            throw std::invalid_argument(
                "Data size must be divisible by dimensions");
        }

        size_t num_points = data.size() / dimensions;
        if (num_points < k) {
            throw std::invalid_argument("Number of points must be >= k");
        }

        // Initialize centroids randomly
        std::vector<T> centroids(k * dimensions);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, num_points - 1);

        for (size_t i = 0; i < k; ++i) {
            size_t random_point = dist(gen);
            for (size_t d = 0; d < dimensions; ++d) {
                centroids[i * dimensions + d] =
                    data[random_point * dimensions + d];
            }
        }

        std::vector<size_t> assignments(num_points);

        for (size_t iter = 0; iter < max_iterations; ++iter) {
            // Assign points to nearest centroids
            bool changed = false;
            for (size_t p = 0; p < num_points; ++p) {
                T min_distance = std::numeric_limits<T>::max();
                size_t best_cluster = 0;

                for (size_t c = 0; c < k; ++c) {
                    T distance = T{0};
                    for (size_t d = 0; d < dimensions; ++d) {
                        T diff = data[p * dimensions + d] -
                                 centroids[c * dimensions + d];
                        distance += diff * diff;
                    }

                    if (distance < min_distance) {
                        min_distance = distance;
                        best_cluster = c;
                    }
                }

                if (assignments[p] != best_cluster) {
                    assignments[p] = best_cluster;
                    changed = true;
                }
            }

            if (!changed)
                break;

            // Update centroids
            std::vector<T> new_centroids(k * dimensions, T{0});
            std::vector<size_t> cluster_counts(k, 0);

            for (size_t p = 0; p < num_points; ++p) {
                size_t cluster = assignments[p];
                cluster_counts[cluster]++;
                for (size_t d = 0; d < dimensions; ++d) {
                    new_centroids[cluster * dimensions + d] +=
                        data[p * dimensions + d];
                }
            }

            for (size_t c = 0; c < k; ++c) {
                if (cluster_counts[c] > 0) {
                    for (size_t d = 0; d < dimensions; ++d) {
                        new_centroids[c * dimensions + d] /=
                            static_cast<T>(cluster_counts[c]);
                    }
                }
            }

            centroids = std::move(new_centroids);
        }

        return centroids;
    }

    /**
     * @brief Principal Component Analysis (simplified)
     * @param data Input data matrix (row-major)
     * @param rows Number of rows
     * @param cols Number of columns
     * @param num_components Number of principal components to compute
     * @return Principal components (simplified implementation)
     */
    [[nodiscard]] static std::vector<T> pca(const std::vector<T>& data,
                                            size_t rows, size_t cols,
                                            size_t num_components) {
        if (data.size() != rows * cols) {
            throw std::invalid_argument("Data size mismatch");
        }

        // Center the data (subtract mean from each column)
        std::vector<T> centered_data = data;
        std::vector<T> column_means(cols, T{0});

        // Compute column means
        for (size_t c = 0; c < cols; ++c) {
            for (size_t r = 0; r < rows; ++r) {
                column_means[c] += data[r * cols + c];
            }
            column_means[c] /= static_cast<T>(rows);
        }

        // Center the data
        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < cols; ++c) {
                centered_data[r * cols + c] -= column_means[c];
            }
        }

        // For simplicity, return the first num_components columns of centered
        // data In a full implementation, this would involve eigenvalue
        // decomposition
        std::vector<T> components;
        components.reserve(rows * num_components);

        for (size_t r = 0; r < rows; ++r) {
            for (size_t c = 0; c < std::min(num_components, cols); ++c) {
                components.push_back(centered_data[r * cols + c]);
            }
        }

        return components;
    }
};

/**
 * @brief Wrapper class for probability distributions
 * @tparam T The numeric type
 */
template <Numeric T>
class Distributions {
public:
    /**
     * @brief Wrapper class for normal distribution
     */
    class NormalDistribution {
    private:
        ::boost::math::normal_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a normal distribution with given mean and standard
         * deviation
         * @param mean The mean of the distribution
         * @param stddev The standard deviation of the distribution
         */
        constexpr NormalDistribution(T mean, T stddev) noexcept
            : distribution_(mean, stddev) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }

        /**
         * @brief Computes the quantile (inverse CDF) at a given probability
         * @param probability The input probability
         * @return The quantile value
         */
        [[nodiscard]] constexpr T quantile(T probability) const noexcept {
            return ::boost::math::quantile(distribution_, probability);
        }
    };

    /**
     * @brief Wrapper class for Student's t-distribution
     */
    class StudentTDistribution {
    private:
        ::boost::math::students_t_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a Student's t-distribution with given degrees of
         * freedom
         * @param degreesOfFreedom The degrees of freedom
         */
        explicit constexpr StudentTDistribution(T degreesOfFreedom) noexcept
            : distribution_(degreesOfFreedom) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }

        /**
         * @brief Computes the quantile (inverse CDF) at a given probability
         * @param probability The input probability
         * @return The quantile value
         */
        [[nodiscard]] constexpr T quantile(T probability) const noexcept {
            return ::boost::math::quantile(distribution_, probability);
        }
    };

    /**
     * @brief Wrapper class for Poisson distribution
     */
    class PoissonDistribution {
    private:
        ::boost::math::poisson_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs a Poisson distribution with given mean
         * @param mean The mean of the distribution
         */
        explicit constexpr PoissonDistribution(T mean) noexcept
            : distribution_(mean) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }
    };

    /**
     * @brief Wrapper class for exponential distribution
     */
    class ExponentialDistribution {
    private:
        ::boost::math::exponential_distribution<T> distribution_;

    public:
        /**
         * @brief Constructs an exponential distribution with given rate
         * parameter
         * @param lambda The rate parameter
         */
        explicit constexpr ExponentialDistribution(T lambda) noexcept
            : distribution_(lambda) {}

        /**
         * @brief Computes the probability density function (PDF) at a given
         * value
         * @param value The input value
         * @return The PDF value
         */
        [[nodiscard]] constexpr T pdf(T value) const noexcept {
            return ::boost::math::pdf(distribution_, value);
        }

        /**
         * @brief Computes the cumulative distribution function (CDF) at a given
         * value
         * @param value The input value
         * @return The CDF value
         */
        [[nodiscard]] constexpr T cdf(T value) const noexcept {
            return ::boost::math::cdf(distribution_, value);
        }
    };
};

/**
 * @brief Wrapper class for numerical integration methods
 * @tparam T The numeric type
 */
template <Numeric T>
class NumericalIntegration {
public:
    /**
     * @brief Computes the integral of a function using the trapezoidal rule
     * @param func The function to integrate
     * @param start The start of the integration interval
     * @param end The end of the integration interval
     * @return The computed integral
     */
    [[nodiscard]] static T trapezoidal(const std::function<T(T)>& func, T start,
                                       T end) {
        return ::boost::math::quadrature::trapezoidal(func, start, end);
    }
};

/**
 * @brief Computes the factorial of a number using constexpr optimization
 * @tparam T The numeric type
 * @param number The input number
 * @return The factorial of the number
 */
template <Numeric T>
[[nodiscard]] constexpr T factorial(T number) noexcept {
    if constexpr (std::is_integral_v<T>) {
        if ((number <= 1)) [[likely]] {
            return 1;
        }
        return number * factorial(number - 1);
    } else {
        return std::tgamma(number + 1);
    }
}

/**
 * @brief Transforms a range of data using a given function
 * @tparam Range The type of the input range
 * @tparam Func The type of the transformation function
 * @param range The input range
 * @param func The transformation function
 * @return A transformed view of the input range
 */
template <std::ranges::input_range Range, typename Func>
[[nodiscard]] auto transformRange(Range&& range, Func func) {
    return std::ranges::transform_view(std::forward<Range>(range), func);
}

/**
 * @brief Wrapper class for optimization methods
 * @tparam T The numeric type
 */
template <Numeric T>
class Optimization {
public:
    /**
     * @brief Performs one-dimensional golden section search to find the minimum
     * of a function
     * @param func The function to minimize
     * @param start The start of the search interval
     * @param end The end of the search interval
     * @param tolerance The tolerance for convergence
     * @return The point where the function attains its minimum
     */
    [[nodiscard]] static T goldenSectionSearch(const std::function<T(T)>& func,
                                               T start, T end, T tolerance) {
        static constexpr T goldenRatio = static_cast<T>(0.618033988749895);
        T pointC = end - goldenRatio * (end - start);
        T pointD = start + goldenRatio * (end - start);

        while (std::abs(pointC - pointD) > tolerance) {
            if ((func(pointC) < func(pointD))) [[likely]] {
                end = pointD;
            } else {
                start = pointC;
            }
            pointC = end - goldenRatio * (end - start);
            pointD = start + goldenRatio * (end - start);
        }

        return (start + end) / 2;
    }

    /**
     * @brief Performs Newton-Raphson method to find the root of a function
     * @param func The function whose root is to be found
     * @param derivativeFunc The derivative of the function
     * @param initialGuess The initial guess for the root
     * @param tolerance The tolerance for convergence
     * @param maxIterations The maximum number of iterations
     * @return The root of the function
     * @throws std::runtime_error If the derivative is zero or maximum
     * iterations are reached
     */
    [[nodiscard]] static T newtonRaphson(
        const std::function<T(T)>& func,
        const std::function<T(T)>& derivativeFunc, T initialGuess, T tolerance,
        int maxIterations) {
        T currentGuess = initialGuess;
        for (int i = 0; i < maxIterations; ++i) {
            T funcValue = func(currentGuess);
            if ((std::abs(funcValue) < tolerance)) [[unlikely]] {
                return currentGuess;
            }
            T derivativeValue = derivativeFunc(currentGuess);
            if ((derivativeValue == 0)) [[unlikely]] {
                throw std::runtime_error(
                    "Derivative is zero. Cannot continue.");
            }
            currentGuess = currentGuess - funcValue / derivativeValue;
        }
        throw std::runtime_error("Max iterations reached without convergence.");
    }
};

/**
 * @brief Wrapper class for linear algebra operations
 * @tparam T The numeric type
 */
template <Numeric T>
class LinearAlgebra {
public:
    using Matrix = ::boost::numeric::ublas::matrix<T>;
    using Vector = ::boost::numeric::ublas::vector<T>;

    /**
     * @brief Solves a linear system of equations Ax = b
     * @param matrix The matrix A
     * @param vector The vector b
     * @return The solution vector x
     */
    [[nodiscard]] static Vector solveLinearSystem(const Matrix& matrix,
                                                  const Vector& vector) {
        ::boost::numeric::ublas::permutation_matrix<std::size_t>
            permutationMatrix(matrix.size1());
        Matrix matrixCopy = matrix;
        ::boost::numeric::ublas::lu_factorize(matrixCopy, permutationMatrix);
        Vector solution = vector;
        ::boost::numeric::ublas::lu_substitute(matrixCopy, permutationMatrix,
                                               solution);
        return solution;
    }

    /**
     * @brief Computes the determinant of a matrix
     * @param matrix The input matrix
     * @return The determinant of the matrix
     */
    [[nodiscard]] static T determinant(const Matrix& matrix) {
        Matrix matrixCopy = matrix;
        ::boost::numeric::ublas::permutation_matrix<std::size_t>
            permutationMatrix(matrix.size1());
        ::boost::numeric::ublas::lu_factorize(matrixCopy, permutationMatrix);
        T determinantValue = T{1};
        for (std::size_t i = 0; i < matrix.size1(); ++i) {
            determinantValue *= matrixCopy(i, i);
        }
        return determinantValue *
               (permutationMatrix.size() % 2 == 1 ? T{-1} : T{1});
    }

    /**
     * @brief Multiplies two matrices
     * @param matrix1 The first matrix
     * @param matrix2 The second matrix
     * @return The product of the two matrices
     */
    [[nodiscard]] static Matrix multiply(const Matrix& matrix1,
                                         const Matrix& matrix2) {
        return ::boost::numeric::ublas::prod(matrix1, matrix2);
    }

    /**
     * @brief Computes the transpose of a matrix
     * @param matrix The input matrix
     * @return The transpose of the matrix
     */
    [[nodiscard]] static Matrix transpose(const Matrix& matrix) {
        return ::boost::numeric::ublas::trans(matrix);
    }
};

/**
 * @brief Wrapper class for solving ordinary differential equations (ODEs)
 * @tparam T The numeric type
 */
template <Numeric T>
class ODESolver {
public:
    using State = std::vector<T>;
    using SystemFunction = std::function<void(const State&, State&, T)>;

    /**
     * @brief Solves an ODE using the 4th order Runge-Kutta method
     * @param system The system function defining the ODE
     * @param initialState The initial state of the system
     * @param startTime The start time
     * @param endTime The end time
     * @param stepSize The step size
     * @return A vector of states representing the solution
     */
    [[nodiscard]] static std::vector<State> rungeKutta4(
        const SystemFunction& system, State initialState, T startTime,
        T endTime, T stepSize) {
        std::vector<State> solution;
        solution.reserve(static_cast<size_t>((endTime - startTime) / stepSize) +
                         1);
        ::boost::numeric::odeint::runge_kutta4<State> stepper;
        ::boost::numeric::odeint::integrate_const(
            stepper, system, initialState, startTime, endTime, stepSize,
            [&solution](const State& state, T) { solution.push_back(state); });
        return solution;
    }
};

/**
 * @brief Wrapper class for financial mathematics functions
 * @tparam T The numeric type
 */
template <Numeric T>
class FinancialMath {
public:
    /**
     * @brief Computes the price of a European call option using the
     * Black-Scholes formula
     * @param stockPrice The current stock price
     * @param strikePrice The strike price of the option
     * @param riskFreeRate The risk-free interest rate
     * @param volatility The volatility of the stock
     * @param timeToMaturity The time to maturity of the option
     * @return The price of the European call option
     */
    [[nodiscard]] static T blackScholesCall(T stockPrice, T strikePrice,
                                            T riskFreeRate, T volatility,
                                            T timeToMaturity) noexcept {
        T sqrtTime = std::sqrt(timeToMaturity);
        T d1 = (std::log(stockPrice / strikePrice) +
                (riskFreeRate + T{0.5} * volatility * volatility) *
                    timeToMaturity) /
               (volatility * sqrtTime);
        T d2 = d1 - volatility * sqrtTime;

        static const ::boost::math::normal_distribution<T> normal;
        return stockPrice * ::boost::math::cdf(normal, d1) -
               strikePrice * std::exp(-riskFreeRate * timeToMaturity) *
                   ::boost::math::cdf(normal, d2);
    }

    /**
     * @brief Computes the modified duration of a bond
     * @param yield The yield to maturity
     * @param couponRate The coupon rate of the bond
     * @param faceValue The face value of the bond
     * @param periods The number of periods
     * @return The modified duration of the bond
     */
    [[nodiscard]] static T modifiedDuration(T yield, T couponRate, T faceValue,
                                            int periods) noexcept {
        T periodYield = yield / periods;
        T couponPayment = couponRate * faceValue / periods;
        T presentValue = T{0};
        T weightedPresentValue = T{0};

        for (int i = 1; i <= periods; ++i) {
            T discountFactor = std::pow(T{1} + periodYield, -i);
            presentValue += couponPayment * discountFactor;
            weightedPresentValue += i * couponPayment * discountFactor;
        }

        T finalDiscountFactor = std::pow(T{1} + periodYield, -periods);
        presentValue += faceValue * finalDiscountFactor;
        weightedPresentValue += periods * faceValue * finalDiscountFactor;

        return (weightedPresentValue / presentValue) / (T{1} + periodYield);
    }

    /**
     * @brief Computes the bond price
     * @param yield The yield to maturity
     * @param couponRate The coupon rate of the bond
     * @param faceValue The face value of the bond
     * @param periods The number of periods
     * @return The bond price
     */
    [[nodiscard]] static T bondPrice(T yield, T couponRate, T faceValue,
                                     int periods) noexcept {
        T periodYield = yield / periods;
        T couponPayment = couponRate * faceValue / periods;
        T presentValue = T{0};

        for (int i = 1; i <= periods; ++i) {
            presentValue += couponPayment * std::pow(T{1} + periodYield, -i);
        }
        presentValue += faceValue * std::pow(T{1} + periodYield, -periods);
        return presentValue;
    }

    /**
     * @brief Computes the implied volatility of an option
     * @param marketPrice The market price of the option
     * @param stockPrice The current stock price
     * @param strikePrice The strike price of the option
     * @param riskFreeRate The risk-free interest rate
     * @param timeToMaturity The time to maturity of the option
     * @return The implied volatility
     */
    [[nodiscard]] static T impliedVolatility(T marketPrice, T stockPrice,
                                             T strikePrice, T riskFreeRate,
                                             T timeToMaturity) {
        auto objectiveFunction = [=](T volatility) {
            return blackScholesCall(stockPrice, strikePrice, riskFreeRate,
                                    volatility, timeToMaturity) -
                   marketPrice;
        };
        auto derivative = [=](T volatility) {
            T sqrtTime = std::sqrt(timeToMaturity);
            T d1 = (std::log(stockPrice / strikePrice) +
                    (riskFreeRate + T{0.5} * volatility * volatility) *
                        timeToMaturity) /
                   (volatility * sqrtTime);
            static const ::boost::math::normal_distribution<T> normal;
            return stockPrice * ::boost::math::pdf(normal, d1) * sqrtTime;
        };
        return Optimization<T>::newtonRaphson(objectiveFunction, derivative,
                                              T{0.2}, T{1e-6}, 100);
    }
};

// Static member definitions
template <Numeric T>
inline std::atomic<uint64_t> Statistics<T>::computation_count_{0};

template <Numeric T>
inline thread_local std::unordered_map<std::string, T> Statistics<T>::cache_{};

}  // namespace atom::extra::boost

#endif
