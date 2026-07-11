#ifndef ATOM_ALGORITHM_UTILS_ERROR_CALIBRATION_HPP
#define ATOM_ALGORITHM_UTILS_ERROR_CALIBRATION_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <execution>
#include <fstream>
#include <functional>
#include <memory_resource>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/algorithm/core/simd_utils.hpp"  // ATOM_SIMD_* macros + SIMD headers
#include "atom/async/pool.hpp"
#include "atom/error/exception.hpp"
#include "levenberg_marquardt.hpp"
#include "linear_solver.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/random.hpp>
#endif

namespace atom::algorithm {

template <std::floating_point T>
class ErrorCalibration {
private:
    T slope_ = 1.0;
    T intercept_ = 0.0;
    std::optional<T> r_squared_;
    std::vector<T> residuals_;
    T mse_ = 0.0;                 // Mean Squared Error
    T mae_ = 0.0;                 // Mean Absolute Error
    std::vector<T> poly_coeffs_;  // Polynomial coefficients for non-linear fits
    i32 calibration_type_ =
        0;  // 0=linear, 1=polynomial, 2=exponential, 3=logarithmic, 4=power

    std::mutex metrics_mutex_;
    std::unique_ptr<atom::async::ThreadPool> thread_pool_;

    // More efficient memory pool
    static constexpr usize MAX_CACHE_SIZE = 10000;
    std::shared_ptr<std::pmr::monotonic_buffer_resource> memory_resource_;
    std::pmr::vector<T> cached_residuals_{memory_resource_.get()};

    // Thread-local storage for parallel computation optimization
    thread_local static std::vector<T> tls_buffer;

    // Automatic resource management
    struct ResourceGuard {
        std::function<void()> cleanup;
        ~ResourceGuard() {
            if (cleanup)
                cleanup();
        }
    };

    /**
     * Initialize thread pool if not already initialized
     */
    void initThreadPool() {
        if (!thread_pool_) {
            const u32 num_threads =
                std::min(std::thread::hardware_concurrency(), 8u);
            // Create Options with proper initialization
            atom::async::ThreadPool::Options options;
            options.initialThreadCount = num_threads;
            thread_pool_ = std::make_unique<atom::async::ThreadPool>(options);

            spdlog::info("Thread pool initialized with {} threads",
                         num_threads);
        }
    }

    /**
     * Apply polynomial transformation
     */
    [[nodiscard]] auto applyPolynomial(T value) const -> T {
        if (poly_coeffs_.empty()) {
            return apply(value);
        }
        T result = 0;
        for (usize i = 0; i < poly_coeffs_.size(); ++i) {
            result += poly_coeffs_[i] * std::pow(value, static_cast<T>(i));
        }
        return result;
    }

    /**
     * Calculate calibration metrics
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void calculateMetrics(const std::vector<T>& measured,
                          const std::vector<T>& actual) {
        initThreadPool();

        // Using std::execution::par_unseq for parallel computation
        T meanActual =
            std::transform_reduce(std::execution::par_unseq, actual.begin(),
                                  actual.end(), T(0), std::plus<>{},
                                  [](T val) { return val; }) /
            actual.size();

        residuals_.clear();
        residuals_.resize(measured.size());

        // More efficient SIMD implementation
#ifdef USE_SIMD
        // Using more advanced SIMD instructions
        // ...
#else
        // Use polynomial apply if coefficients are available, otherwise linear
        if (!poly_coeffs_.empty()) {
            std::transform(std::execution::par_unseq, measured.begin(),
                           measured.end(), actual.begin(), residuals_.begin(),
                           [this](T m, T a) { return a - applyPolynomial(m); });
        } else {
            std::transform(std::execution::par_unseq, measured.begin(),
                           measured.end(), actual.begin(), residuals_.begin(),
                           [this](T m, T a) { return a - apply(m); });
        }

        mse_ = std::transform_reduce(
                   std::execution::par_unseq, residuals_.begin(),
                   residuals_.end(), T(0), std::plus<>{},
                   [](T residual) { return residual * residual; }) /
               residuals_.size();

        mae_ = std::transform_reduce(
                   std::execution::par_unseq, residuals_.begin(),
                   residuals_.end(), T(0), std::plus<>{},
                   [](T residual) { return std::abs(residual); }) /
               residuals_.size();
#endif

        // Calculate R-squared
        T ssTotal = std::transform_reduce(
            std::execution::par_unseq, actual.begin(), actual.end(), T(0),
            std::plus<>{},
            [meanActual](T val) { return std::pow(val - meanActual, 2); });

        T ssResidual = std::transform_reduce(
            std::execution::par_unseq, residuals_.begin(), residuals_.end(),
            T(0), std::plus<>{},
            [](T residual) { return residual * residual; });

        if (ssTotal > 0) {
            r_squared_ = 1 - (ssResidual / ssTotal);
        } else {
            r_squared_ = std::nullopt;
        }
    }

    using NonlinearFunction = detail::NonlinearFunction<T>;

    /**
     * Delegate to detail::levenbergMarquardt free function
     */
    auto levenbergMarquardt(const std::vector<T>& x, const std::vector<T>& y,
                            NonlinearFunction func,
                            std::vector<T> initial_params,
                            i32 max_iterations = 100, T lambda = 0.01,
                            T epsilon = 1e-8) -> std::vector<T> {
        return detail::levenbergMarquardt<T>(x, y, std::move(func),
                                            std::move(initial_params),
                                            max_iterations, lambda, epsilon);
    }

public:
    ErrorCalibration()
        : memory_resource_(
              std::make_shared<std::pmr::monotonic_buffer_resource>()) {
        // Pre-allocate memory to avoid frequent reallocation
        cached_residuals_.reserve(MAX_CACHE_SIZE);
    }

    ~ErrorCalibration() {
        try {
            if (thread_pool_) {
                thread_pool_->waitForTasks();
            }
        } catch (...) {
            // Ensure destructor never throws exceptions
            spdlog::error("Exception during thread pool cleanup");
        }
    }

    /**
     * Linear calibration using the least squares method
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void linearCalibrate(const std::vector<T>& measured,
                         const std::vector<T>& actual) {
        if (measured.size() != actual.size() || measured.empty()) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of equal size");
        }

        T sumX = std::accumulate(measured.begin(), measured.end(), T(0));
        T sumY = std::accumulate(actual.begin(), actual.end(), T(0));
        T sumXy = std::inner_product(measured.begin(), measured.end(),
                                     actual.begin(), T(0));
        T sumXx = std::inner_product(measured.begin(), measured.end(),
                                     measured.begin(), T(0));

        T n = static_cast<T>(measured.size());
        if (n * sumXx - sumX * sumX == 0) {
            THROW_RUNTIME_ERROR("Division by zero in slope calculation.");
        }
        slope_ = (n * sumXy - sumX * sumY) / (n * sumXx - sumX * sumX);
        intercept_ = (sumY - slope_ * sumX) / n;

        calculateMetrics(measured, actual);
    }

    /**
     * Polynomial calibration using the least squares method
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     * @param degree Degree of the polynomial
     */
    void polynomialCalibrate(const std::vector<T>& measured,
                             const std::vector<T>& actual, i32 degree) {
        // Enhanced input validation
        if (measured.size() != actual.size()) {
            THROW_INVALID_ARGUMENT("Input vectors must be of equal size");
        }

        if (measured.empty()) {
            THROW_INVALID_ARGUMENT("Input vectors must be non-empty");
        }

        if (degree < 1) {
            THROW_INVALID_ARGUMENT("Polynomial degree must be at least 1.");
        }

        if (measured.size() <= static_cast<usize>(degree)) {
            THROW_INVALID_ARGUMENT(
                "Number of data points must exceed polynomial degree.");
        }

        // Check for NaN and infinity values
        if (std::ranges::any_of(
                measured, [](T x) { return std::isnan(x) || std::isinf(x); }) ||
            std::ranges::any_of(
                actual, [](T y) { return std::isnan(y) || std::isinf(y); })) {
            THROW_INVALID_ARGUMENT(
                "Input vectors contain NaN or infinity values.");
        }

        auto polyFunc = [degree](T x, const std::vector<T>& params) -> T {
            T result = 0;
            for (i32 i = 0; i <= degree; ++i) {
                result += params[i] * std::pow(x, i);
            }
            return result;
        };

        std::vector<T> initialParams(degree + 1, 1.0);
        try {
            auto params =
                levenbergMarquardt(measured, actual, polyFunc, initialParams);

            if (params.size() < 2) {
                THROW_RUNTIME_ERROR(
                    "Insufficient parameters returned from calibration.");
            }

            // Store all polynomial coefficients for accurate metrics
            // calculation
            poly_coeffs_ = params;
            calibration_type_ = 1;   // polynomial
            slope_ = params[1];      // First-order coefficient as slope
            intercept_ = params[0];  // Constant term as intercept

            calculateMetrics(measured, actual);
        } catch (const std::exception& e) {
            THROW_RUNTIME_ERROR(std::string("Polynomial calibration failed: ") +
                                e.what());
        }
    }

    /**
     * Exponential calibration using the least squares method
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void exponentialCalibrate(const std::vector<T>& measured,
                              const std::vector<T>& actual) {
        if (measured.size() != actual.size() || measured.empty()) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of equal size");
        }
        if (std::any_of(actual.begin(), actual.end(),
                        [](T val) { return val <= 0; })) {
            THROW_INVALID_ARGUMENT(
                "Actual values must be positive for exponential calibration.");
        }

        auto expFunc = [](T x, const std::vector<T>& params) -> T {
            return params[0] * std::exp(params[1] * x);
        };

        std::vector<T> initialParams = {1.0, 0.1};
        auto params =
            levenbergMarquardt(measured, actual, expFunc, initialParams);

        if (params.size() < 2) {
            THROW_RUNTIME_ERROR(
                "Insufficient parameters returned from calibration.");
        }

        slope_ = params[1];
        intercept_ = params[0];

        calculateMetrics(measured, actual);
    }

    /**
     * Logarithmic calibration using the least squares method
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void logarithmicCalibrate(const std::vector<T>& measured,
                              const std::vector<T>& actual) {
        if (measured.size() != actual.size() || measured.empty()) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of equal size");
        }
        if (std::any_of(measured.begin(), measured.end(),
                        [](T val) { return val <= 0; })) {
            THROW_INVALID_ARGUMENT(
                "Measured values must be positive for logarithmic "
                "calibration.");
        }

        auto logFunc = [](T x, const std::vector<T>& params) -> T {
            return params[0] + params[1] * std::log(x);
        };

        std::vector<T> initialParams = {0.0, 1.0};
        auto params =
            levenbergMarquardt(measured, actual, logFunc, initialParams);

        if (params.size() < 2) {
            THROW_RUNTIME_ERROR(
                "Insufficient parameters returned from calibration.");
        }

        slope_ = params[1];
        intercept_ = params[0];

        calculateMetrics(measured, actual);
    }

    /**
     * Power law calibration using the least squares method
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void powerLawCalibrate(const std::vector<T>& measured,
                           const std::vector<T>& actual) {
        if (measured.size() != actual.size() || measured.empty()) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of equal size");
        }
        if (std::any_of(measured.begin(), measured.end(),
                        [](T val) { return val <= 0; }) ||
            std::any_of(actual.begin(), actual.end(),
                        [](T val) { return val <= 0; })) {
            THROW_INVALID_ARGUMENT(
                "Values must be positive for power law calibration.");
        }

        auto powerFunc = [](T x, const std::vector<T>& params) -> T {
            return params[0] * std::pow(x, params[1]);
        };

        std::vector<T> initialParams = {1.0, 1.0};
        auto params =
            levenbergMarquardt(measured, actual, powerFunc, initialParams);

        if (params.size() < 2) {
            THROW_RUNTIME_ERROR(
                "Insufficient parameters returned from calibration.");
        }

        slope_ = params[1];
        intercept_ = params[0];

        calculateMetrics(measured, actual);
    }

    [[nodiscard]] auto apply(T value) const -> T {
        return slope_ * value + intercept_;
    }

    void printParameters() const {
        spdlog::info("Calibration parameters: slope = {}, intercept = {}",
                     slope_, intercept_);
        if (r_squared_.has_value()) {
            spdlog::info("R-squared = {}", r_squared_.value());
        }
        spdlog::info("MSE = {}, MAE = {}", mse_, mae_);
    }

    [[nodiscard]] auto getResiduals() const -> std::vector<T> {
        return residuals_;
    }

    void plotResiduals(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Failed to open file: " + filename);
        }

        file << "Index,Residual\n";
        for (usize i = 0; i < residuals_.size(); ++i) {
            file << i << "," << residuals_[i] << "\n";
        }
    }

    /**
     * Bootstrap confidence interval for the slope
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     * @param n_iterations Number of bootstrap iterations
     * @param confidence_level Confidence level for the interval
     * @return Pair of lower and upper bounds of the confidence interval
     */
    auto bootstrapConfidenceInterval(
        const std::vector<T>& measured, const std::vector<T>& actual,
        i32 n_iterations = 1000,
        f64 confidence_level = 0.95) -> std::pair<T, T> {
        if (n_iterations <= 0) {
            THROW_INVALID_ARGUMENT("Number of iterations must be positive.");
        }
        if (confidence_level <= 0 || confidence_level >= 1) {
            THROW_INVALID_ARGUMENT("Confidence level must be between 0 and 1.");
        }

        std::vector<T> bootstrapSlopes;
        bootstrapSlopes.reserve(n_iterations);
#ifdef ATOM_USE_BOOST
        boost::random::random_device rd;
        boost::random::mt19937 gen(rd());
        boost::random::uniform_int_distribution<> dis(0, measured.size() - 1);
#else
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, measured.size() - 1);
#endif

        for (i32 i = 0; i < n_iterations; ++i) {
            std::vector<T> bootMeasured;
            std::vector<T> bootActual;
            bootMeasured.reserve(measured.size());
            bootActual.reserve(actual.size());
            for (usize j = 0; j < measured.size(); ++j) {
                i32 idx = dis(gen);
                bootMeasured.push_back(measured[idx]);
                bootActual.push_back(actual[idx]);
            }

            ErrorCalibration<T> bootCalibrator;
            try {
                bootCalibrator.linearCalibrate(bootMeasured, bootActual);
                bootstrapSlopes.push_back(bootCalibrator.getSlope());
            } catch (const std::exception& e) {
                spdlog::warn("Bootstrap iteration {} failed: {}", i, e.what());
            }
        }

        if (bootstrapSlopes.empty()) {
            THROW_RUNTIME_ERROR("All bootstrap iterations failed.");
        }

        std::sort(bootstrapSlopes.begin(), bootstrapSlopes.end());
        i32 lowerIdx = static_cast<i32>((1 - confidence_level) / 2 *
                                        bootstrapSlopes.size());
        i32 upperIdx = static_cast<i32>((1 + confidence_level) / 2 *
                                        bootstrapSlopes.size());

        lowerIdx = std::clamp(lowerIdx, 0,
                              static_cast<i32>(bootstrapSlopes.size()) - 1);
        upperIdx = std::clamp(upperIdx, 0,
                              static_cast<i32>(bootstrapSlopes.size()) - 1);

        return {bootstrapSlopes[lowerIdx], bootstrapSlopes[upperIdx]};
    }

    /**
     * Detect outliers using the residuals of the calibration
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     * @param threshold Threshold for outlier detection
     * @return Tuple of mean residual, standard deviation, and threshold
     */
    auto outlierDetection(const std::vector<T>& measured,
                          const std::vector<T>& actual,
                          T threshold = 2.0) -> std::tuple<T, T, T> {
        if (residuals_.empty()) {
            calculateMetrics(measured, actual);
        }

        T meanResidual =
            std::accumulate(residuals_.begin(), residuals_.end(), T(0)) /
            residuals_.size();
        T std_dev = std::sqrt(
            std::accumulate(residuals_.begin(), residuals_.end(), T(0),
                            [meanResidual](T acc, T val) {
                                return acc + std::pow(val - meanResidual, 2);
                            }) /
            residuals_.size());

#if ATOM_ENABLE_DEBUG
        std::cout << "Detected outliers:" << std::endl;
        for (usize i = 0; i < residuals_.size(); ++i) {
            if (std::abs(residuals_[i] - meanResidual) > threshold * std_dev) {
                std::cout << "Index: " << i << ", Measured: " << measured[i]
                          << ", Actual: " << actual[i]
                          << ", Residual: " << residuals_[i] << std::endl;
            }
        }
#endif
        return {meanResidual, std_dev, threshold};
    }

    void crossValidation(const std::vector<T>& measured,
                         const std::vector<T>& actual, i32 k = 5) {
        if (measured.size() != actual.size() ||
            measured.size() < static_cast<usize>(k)) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of size greater than k");
        }

        std::vector<T> mseValues;
        std::vector<T> maeValues;
        std::vector<T> rSquaredValues;

        for (i32 i = 0; i < k; ++i) {
            std::vector<T> trainMeasured;
            std::vector<T> trainActual;
            std::vector<T> testMeasured;
            std::vector<T> testActual;
            for (usize j = 0; j < measured.size(); ++j) {
                if (j % k == static_cast<usize>(i)) {
                    testMeasured.push_back(measured[j]);
                    testActual.push_back(actual[j]);
                } else {
                    trainMeasured.push_back(measured[j]);
                    trainActual.push_back(actual[j]);
                }
            }

            ErrorCalibration<T> cvCalibrator;
            try {
                cvCalibrator.linearCalibrate(trainMeasured, trainActual);
            } catch (const std::exception& e) {
                spdlog::warn("Cross-validation fold {} failed: {}", i,
                             e.what());
                continue;
            }

            T foldMse = 0;
            T foldMae = 0;
            T foldSsTotal = 0;
            T foldSsResidual = 0;
            T meanTestActual =
                std::accumulate(testActual.begin(), testActual.end(), T(0)) /
                testActual.size();
            for (usize j = 0; j < testMeasured.size(); ++j) {
                T predicted = cvCalibrator.apply(testMeasured[j]);
                T error = testActual[j] - predicted;
                foldMse += error * error;
                foldMae += std::abs(error);
                foldSsTotal += std::pow(testActual[j] - meanTestActual, 2);
                foldSsResidual += std::pow(error, 2);
            }

            mseValues.push_back(foldMse / testMeasured.size());
            maeValues.push_back(foldMae / testMeasured.size());
            if (foldSsTotal != 0) {
                rSquaredValues.push_back(1 - (foldSsResidual / foldSsTotal));
            }
        }

        if (mseValues.empty()) {
            THROW_RUNTIME_ERROR("All cross-validation folds failed.");
        }

        T avgRSquared = 0;
        if (!rSquaredValues.empty()) {
            avgRSquared = std::accumulate(rSquaredValues.begin(),
                                          rSquaredValues.end(), T(0)) /
                          rSquaredValues.size();
        }

#if ATOM_ENABLE_DEBUG
        T avgMse = std::accumulate(mseValues.begin(), mseValues.end(), T(0)) /
                   mseValues.size();
        T avgMae = std::accumulate(maeValues.begin(), maeValues.end(), T(0)) /
                   maeValues.size();
        spdlog::debug("K-fold cross-validation results (k = {})", k);
        spdlog::debug("Average MSE: {}", avgMse);
        spdlog::debug("Average MAE: {}", avgMae);
        spdlog::debug("Average R-squared: {}", avgRSquared);
#endif
    }

    [[nodiscard]] auto getSlope() const -> T { return slope_; }
    [[nodiscard]] auto getIntercept() const -> T { return intercept_; }
    [[nodiscard]] auto getRSquared() const -> std::optional<T> {
        return r_squared_;
    }
    [[nodiscard]] auto getMse() const -> T { return mse_; }
    [[nodiscard]] auto getMae() const -> T { return mae_; }
};

}  // namespace atom::algorithm

// Include async calibration support (coroutines)
#include "async_calibration.hpp"

#endif  // ATOM_ALGORITHM_UTILS_ERROR_CALIBRATION_HPP
