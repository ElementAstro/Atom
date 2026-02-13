#ifndef ATOM_ALGORITHM_UTILS_ASYNC_CALIBRATION_HPP
#define ATOM_ALGORITHM_UTILS_ASYNC_CALIBRATION_HPP

#include <concepts>
#include <coroutine>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

// Forward declaration - the full ErrorCalibration is defined in
// error_calibration.hpp
namespace atom::algorithm {
template <std::floating_point T>
class ErrorCalibration;
}

namespace atom::algorithm {

/**
 * @brief Coroutine support for asynchronous calibration
 * @tparam T Floating-point type
 */
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
            spdlog::error(
                "Exception in AsyncCalibrationTask: {}",
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

/**
 * @brief Asynchronous calibration method using coroutines
 * @tparam T Floating-point type
 * @param measured Vector of measured values
 * @param actual Vector of actual values
 * @return AsyncCalibrationTask coroutine handle
 */
template <std::floating_point T>
AsyncCalibrationTask<T> calibrateAsync(const std::vector<T>& measured,
                                       const std::vector<T>& actual) {
    auto calibrator = new ErrorCalibration<T>();

    // Execute calibration in background thread
    std::thread worker([calibrator, measured, actual]() {
        try {
            calibrator->linearCalibrate(measured, actual);
        } catch (const std::exception& e) {
            spdlog::error("Async calibration failed: {}", e.what());
        }
    });
    worker.detach();  // Let the thread run in the background

    // Wait for some ready flag
    co_await std::suspend_always{};

    co_return calibrator;
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_ASYNC_CALIBRATION_HPP
