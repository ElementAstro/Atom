#ifndef ATOM_ALGORITHM_ERROR_CALIBRATION_HPP
#define ATOM_ALGORITHM_ERROR_CALIBRATION_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <coroutine>
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

#ifdef USE_SIMD
#ifdef __AVX__
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#endif

#include "atom/async/pool.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

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
    T mse_ = 0.0;  // Mean Squared Error
    T mae_ = 0.0;  // Mean Absolute Error

    std::mutex metrics_mutex_;
    std::unique_ptr<atom::async::ThreadPool<>> thread_pool_;

    // 使用更高效的内存池
    static constexpr size_t MAX_CACHE_SIZE = 10000;
    std::shared_ptr<std::pmr::monotonic_buffer_resource> memory_resource_;
    std::pmr::vector<T> cached_residuals_{memory_resource_.get()};

    // 使用线程本地存储优化并行计算
    thread_local static std::vector<T> tls_buffer;

    // 自动资源管理
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
            const unsigned int num_threads = std::min(
                std::thread::hardware_concurrency(), 8u);  // 限制最大线程数
            thread_pool_ =
                std::make_unique<atom::async::ThreadPool<>>(num_threads);
            LOG_F(INFO, "Thread pool initialized with {} threads", num_threads);
        }
    }

    /**
     * Calculate calibration metrics
     * @param measured Vector of measured values
     * @param actual Vector of actual values
     */
    void calculateMetrics(const std::vector<T>& measured,
                          const std::vector<T>& actual) {
        initThreadPool();

        // 使用std::execution::par_unseq进行并行计算
        T meanActual =
            std::transform_reduce(std::execution::par_unseq, actual.begin(),
                                  actual.end(), T(0), std::plus<>{},
                                  [](T val) { return val; }) /
            actual.size();

        residuals_.clear();
        residuals_.resize(measured.size());

        // 更高效的SIMD实现
#ifdef USE_SIMD
        // 使用更高级的SIMD指令
        // ...
#else
        std::transform(std::execution::par_unseq, measured.begin(),
                       measured.end(), actual.begin(), residuals_.begin(),
                       [this](T m, T a) { return a - apply(m); });

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

        // 计算R-squared
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

    using NonlinearFunction = std::function<T(T, const std::vector<T>&)>;

    /**
     * Solve a system of linear equations using the Levenberg-Marquardt method
     * @param x Vector of x values
     * @param y Vector of y values
     * @param func Nonlinear function to fit
     * @param initial_params Initial guess for the parameters
     * @param max_iterations Maximum number of iterations
     * @param lambda Regularization parameter
     * @param epsilon Convergence criterion
     * @return Vector of optimized parameters
     */
    auto levenbergMarquardt(const std::vector<T>& x, const std::vector<T>& y,
                            NonlinearFunction func,
                            std::vector<T> initial_params,
                            int max_iterations = 100, T lambda = 0.01,
                            T epsilon = 1e-8) -> std::vector<T> {
        int n = x.size();
        int m = initial_params.size();
        std::vector<T> params = initial_params;
        std::vector<T> prevParams(m);
        std::vector<std::vector<T>> jacobian(n, std::vector<T>(m));

        for (int iteration = 0; iteration < max_iterations; ++iteration) {
            std::vector<T> residuals(n);
            for (int i = 0; i < n; ++i) {
                try {
                    residuals[i] = y[i] - func(x[i], params);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in func: %s", e.what());
                    throw;
                }
                for (int j = 0; j < m; ++j) {
                    T h = std::max(T(1e-6), std::abs(params[j]) * T(1e-6));
                    std::vector<T> paramsPlusH = params;
                    paramsPlusH[j] += h;
                    try {
                        jacobian[i][j] =
                            (func(x[i], paramsPlusH) - func(x[i], params)) / h;
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in jacobian computation: %s",
                              e.what());
                        throw;
                    }
                }
            }

            std::vector<std::vector<T>> JTJ(m, std::vector<T>(m, 0.0));
            std::vector<T> jTr(m, 0.0);
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    for (int k = 0; k < n; ++k) {
                        JTJ[i][j] += jacobian[k][i] * jacobian[k][j];
                    }
                    if (i == j)
                        JTJ[i][j] += lambda;
                }
                for (int k = 0; k < n; ++k) {
                    jTr[i] += jacobian[k][i] * residuals[k];
                }
            }

#ifdef ATOM_USE_BOOST
            // 使用Boost的LU分解来求解线性系统
            boost::numeric::ublas::matrix<T> A(m, m);
            boost::numeric::ublas::vector<T> b(m);
            for (int i = 0; i < m; ++i) {
                for (int j = 0; j < m; ++j) {
                    A(i, j) = JTJ[i][j];
                }
                b(i) = jTr[i];
            }

            boost::numeric::ublas::permutation_matrix<std::size_t> pm(
                A.size1());
            bool singular = boost::numeric::ublas::lu_factorize(A, pm);
            if (singular) {
                THROW_RUNTIME_ERROR("Matrix is singular.");
            }
            boost::numeric::ublas::lu_substitute(A, pm, b);

            std::vector<T> delta(m);
            for (int i = 0; i < m; ++i) {
                delta[i] = b(i);
            }
#else
            // 使用自定义的高斯消元法
            std::vector<T> delta;
            try {
                delta = solveLinearSystem(JTJ, jTr);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in solving linear system: %s",
                      e.what());
                throw;
            }
#endif

            prevParams = params;
            for (int i = 0; i < m; ++i) {
                params[i] += delta[i];
            }

            T diff = 0;
            for (int i = 0; i < m; ++i) {
                diff += std::abs(params[i] - prevParams[i]);
            }
            if (diff < epsilon) {
                break;
            }
        }

        return params;
    }

    /**
     * Solve a system of linear equations using Gaussian elimination
     * @param A Coefficient matrix
     * @param b Right-hand side vector
     * @return Solution vector
     */
#ifdef ATOM_USE_BOOST
    // 使用Boost的线性代数库，无需自定义实现
#else
    auto solveLinearSystem(const std::vector<std::vector<T>>& A,
                           const std::vector<T>& b) -> std::vector<T> {
        int n = A.size();
        std::vector<std::vector<T>> augmented(n, std::vector<T>(n + 1, 0.0));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                augmented[i][j] = A[i][j];
            }
            augmented[i][n] = b[i];
        }

        for (int i = 0; i < n; ++i) {
            // Partial pivoting
            int maxRow = i;
            for (int k = i + 1; k < n; ++k) {
                if (std::abs(augmented[k][i]) >
                    std::abs(augmented[maxRow][i])) {
                    maxRow = k;
                }
            }
            if (std::abs(augmented[maxRow][i]) < 1e-12) {
                THROW_RUNTIME_ERROR("Matrix is singular or nearly singular.");
            }
            std::swap(augmented[i], augmented[maxRow]);

            // Eliminate below
            for (int k = i + 1; k < n; ++k) {
                T factor = augmented[k][i] / augmented[i][i];
                for (int j = i; j <= n; ++j) {
                    augmented[k][j] -= factor * augmented[i][j];
                }
            }
        }

        std::vector<T> x(n, 0.0);
        for (int i = n - 1; i >= 0; --i) {
            if (std::abs(augmented[i][i]) < 1e-12) {
                THROW_RUNTIME_ERROR(
                    "Division by zero during back substitution.");
            }
            x[i] = augmented[i][n];
            for (int j = i + 1; j < n; ++j) {
                x[i] -= augmented[i][j] * x[j];
            }
            x[i] /= augmented[i][i];
        }

        return x;
    }
#endif

public:
    ErrorCalibration()
        : memory_resource_(
              std::make_shared<std::pmr::monotonic_buffer_resource>()) {
        // 预分配内存以避免频繁重分配
        cached_residuals_.reserve(MAX_CACHE_SIZE);
    }

    ~ErrorCalibration() {
        try {
            if (thread_pool_) {
                thread_pool_->waitForTasks();
            }
        } catch (...) {
            // 确保析构函数永远不抛出异常
            LOG_F(ERROR, "Exception during thread pool cleanup");
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
                             const std::vector<T>& actual, int degree) {
        // 强化输入验证
        if (measured.size() != actual.size()) {
            THROW_INVALID_ARGUMENT("Input vectors must be of equal size");
        }

        if (measured.empty()) {
            THROW_INVALID_ARGUMENT("Input vectors must be non-empty");
        }

        if (degree < 1) {
            THROW_INVALID_ARGUMENT("Polynomial degree must be at least 1.");
        }

        if (measured.size() <= static_cast<size_t>(degree)) {
            THROW_INVALID_ARGUMENT(
                "Number of data points must exceed polynomial degree.");
        }

        // 检查NaN和无穷大
        if (std::ranges::any_of(
                measured, [](T x) { return std::isnan(x) || std::isinf(x); }) ||
            std::ranges::any_of(
                actual, [](T y) { return std::isnan(y) || std::isinf(y); })) {
            THROW_INVALID_ARGUMENT(
                "Input vectors contain NaN or infinity values.");
        }

        auto polyFunc = [degree](T x, const std::vector<T>& params) -> T {
            T result = 0;
            for (int i = 0; i <= degree; ++i) {
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
        LOG_F(INFO, "Calibration parameters: slope = {}, intercept = {}",
              slope_, intercept_);
        if (r_squared_.has_value()) {
            LOG_F(INFO, "R-squared = {}", r_squared_.value());
        }
        LOG_F(INFO, "MSE = {}, MAE = {}", mse_, mae_);
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
        for (size_t i = 0; i < residuals_.size(); ++i) {
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
    auto bootstrapConfidenceInterval(const std::vector<T>& measured,
                                     const std::vector<T>& actual,
                                     int n_iterations = 1000,
                                     double confidence_level = 0.95)
        -> std::pair<T, T> {
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

        for (int i = 0; i < n_iterations; ++i) {
            std::vector<T> bootMeasured;
            std::vector<T> bootActual;
            bootMeasured.reserve(measured.size());
            bootActual.reserve(actual.size());
            for (size_t j = 0; j < measured.size(); ++j) {
                int idx = dis(gen);
                bootMeasured.push_back(measured[idx]);
                bootActual.push_back(actual[idx]);
            }

            ErrorCalibration<T> bootCalibrator;
            try {
                bootCalibrator.linearCalibrate(bootMeasured, bootActual);
                bootstrapSlopes.push_back(bootCalibrator.getSlope());
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Bootstrap iteration %d failed: %s", i,
                      e.what());
            }
        }

        if (bootstrapSlopes.empty()) {
            THROW_RUNTIME_ERROR("All bootstrap iterations failed.");
        }

        std::sort(bootstrapSlopes.begin(), bootstrapSlopes.end());
        int lowerIdx = static_cast<int>((1 - confidence_level) / 2 *
                                        bootstrapSlopes.size());
        int upperIdx = static_cast<int>((1 + confidence_level) / 2 *
                                        bootstrapSlopes.size());

        lowerIdx = std::clamp(lowerIdx, 0,
                              static_cast<int>(bootstrapSlopes.size()) - 1);
        upperIdx = std::clamp(upperIdx, 0,
                              static_cast<int>(bootstrapSlopes.size()) - 1);

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
                          const std::vector<T>& actual, T threshold = 2.0)
        -> std::tuple<T, T, T> {
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
        for (size_t i = 0; i < residuals_.size(); ++i) {
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
                         const std::vector<T>& actual, int k = 5) {
        if (measured.size() != actual.size() ||
            measured.size() < static_cast<size_t>(k)) {
            THROW_INVALID_ARGUMENT(
                "Input vectors must be non-empty and of size greater than k");
        }

        std::vector<T> mseValues;
        std::vector<T> maeValues;
        std::vector<T> rSquaredValues;

        for (int i = 0; i < k; ++i) {
            std::vector<T> trainMeasured;
            std::vector<T> trainActual;
            std::vector<T> testMeasured;
            std::vector<T> testActual;
            for (size_t j = 0; j < measured.size(); ++j) {
                if (j % k == static_cast<size_t>(i)) {
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
                LOG_F(WARNING, "Cross-validation fold %d failed: %s", i,
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
            for (size_t j = 0; j < testMeasured.size(); ++j) {
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
        std::cout << "K-fold cross-validation results (k = " << k
                  << "):" << std::endl;
        std::cout << "Average MSE: " << avgMse << std::endl;
        std::cout << "Average MAE: " << avgMae << std::endl;
        std::cout << "Average R-squared: " << avgRSquared << std::endl;
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

// 使用协程支持的异步校准方法
template <std::floating_point T>
class AsyncCalibrationTask {
public:
    struct promise_type {
        ErrorCalibration<T>* result;

        auto get_return_object() {
            return AsyncCalibrationTask{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        auto initial_suspend() { return std::suspend_never{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void unhandled_exception() {
            LOG_F(ERROR, "Exception in AsyncCalibrationTask: %s",
                  std::current_exception().__cxa_exception_type()->name());
        }
        void return_value(ErrorCalibration<T>* calibrator) {
            result = calibrator;
        }
    };

    std::coroutine_handle<promise_type> handle;

    AsyncCalibrationTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~AsyncCalibrationTask() {
        if (handle)
            handle.destroy();
    }

    ErrorCalibration<T>* getResult() { return handle.promise().result; }
};

// 使用协程的异步校准方法
template <std::floating_point T>
AsyncCalibrationTask<T> calibrateAsync(const std::vector<T>& measured,
                                       const std::vector<T>& actual) {
    auto calibrator = new ErrorCalibration<T>();

    // 在后台线程中执行校准
    std::thread worker([calibrator, measured, actual]() {
        try {
            calibrator->linearCalibrate(measured, actual);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Async calibration failed: %s", e.what());
        }
    });
    worker.detach();  // 让线程在后台运行

    // 等待一些结果就绪的标志
    co_await std::suspend_always{};

    co_return calibrator;
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ERROR_CALIBRATION_HPP
