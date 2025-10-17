#include "atom/error/error_recovery.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(error_recovery, m) {
    m.doc() =
        "Error recovery framework with retry policies and fallback strategies";

    // CircuitBreakerState enum
    py::enum_<atom::error::CircuitBreakerState>(m, "CircuitBreakerState",
                                                R"(Circuit breaker states.

Represents the state of a circuit breaker for preventing cascading failures.

Examples:
    >>> from atom.error import CircuitBreakerState
    >>> state = CircuitBreakerState.Closed
)")
        .value("Closed", atom::error::CircuitBreakerState::Closed,
               "Normal operation")
        .value("Open", atom::error::CircuitBreakerState::Open,
               "Circuit is open, failing fast")
        .value("HalfOpen", atom::error::CircuitBreakerState::HalfOpen,
               "Testing if service has recovered")
        .export_values();

    // RetryPolicy base class
    py::class_<atom::error::RetryPolicy,
               std::shared_ptr<atom::error::RetryPolicy>>(
        m, "RetryPolicy",
        R"(Retry policy interface.

Base class for all retry policies. Defines when and how operations should be retried.
)")
        .def("should_retry", &atom::error::RetryPolicy::shouldRetry,
             py::arg("context"),
             R"(Check if operation should be retried.

Args:
    context (ErrorContext): The error context

Returns:
    bool: True if the operation should be retried
)")
        .def("get_retry_delay", &atom::error::RetryPolicy::getRetryDelay,
             py::arg("attempt_number"),
             R"(Get delay before next retry.

Args:
    attempt_number (int): The attempt number (0-based)

Returns:
    timedelta: Delay before next retry
)")
        .def("reset", &atom::error::RetryPolicy::reset,
             R"(Reset policy state.

Resets the retry policy to its initial state.
)")
        .def("clone", &atom::error::RetryPolicy::clone,
             R"(Clone the policy.

Returns:
    RetryPolicy: A clone of this policy
)");

    // FixedIntervalRetryPolicy class
    py::class_<atom::error::FixedIntervalRetryPolicy, atom::error::RetryPolicy,
               std::shared_ptr<atom::error::FixedIntervalRetryPolicy>>(
        m, "FixedIntervalRetryPolicy",
        R"(Fixed interval retry policy.

Retries operations with a fixed delay between attempts.

Examples:
    >>> from atom.error import FixedIntervalRetryPolicy
    >>> from datetime import timedelta
    >>> policy = FixedIntervalRetryPolicy(3, timedelta(seconds=1))
)")
        .def(py::init<int, std::chrono::milliseconds>(), py::arg("max_retries"),
             py::arg("interval"),
             R"(Constructs a FixedIntervalRetryPolicy.

Args:
    max_retries (int): Maximum number of retries
    interval (timedelta): Fixed interval between retries
)")
        .def("should_retry",
             &atom::error::FixedIntervalRetryPolicy::shouldRetry,
             py::arg("context"), "Check if operation should be retried.")
        .def("get_retry_delay",
             &atom::error::FixedIntervalRetryPolicy::getRetryDelay,
             py::arg("attempt_number"), "Get delay before next retry.")
        .def("reset", &atom::error::FixedIntervalRetryPolicy::reset,
             "Reset policy state.")
        .def("clone", &atom::error::FixedIntervalRetryPolicy::clone,
             "Clone the policy.");

    // ExponentialBackoffRetryPolicy class
    py::class_<atom::error::ExponentialBackoffRetryPolicy,
               atom::error::RetryPolicy,
               std::shared_ptr<atom::error::ExponentialBackoffRetryPolicy>>(
        m, "ExponentialBackoffRetryPolicy",
        R"(Exponential backoff retry policy.

Retries operations with exponentially increasing delays between attempts.

Examples:
    >>> from atom.error import ExponentialBackoffRetryPolicy
    >>> from datetime import timedelta
    >>> policy = ExponentialBackoffRetryPolicy(5, timedelta(milliseconds=100), 2.0)
)")
        .def(py::init<int, std::chrono::milliseconds, double,
                      std::chrono::milliseconds>(),
             py::arg("max_retries"), py::arg("base_delay"),
             py::arg("multiplier") = 2.0,
             py::arg("max_delay") = std::chrono::minutes(5),
             R"(Constructs an ExponentialBackoffRetryPolicy.

Args:
    max_retries (int): Maximum number of retries
    base_delay (timedelta): Base delay for first retry
    multiplier (float, optional): Multiplier for exponential backoff (default: 2.0)
    max_delay (timedelta, optional): Maximum delay cap (default: 5 minutes)
)")
        .def("should_retry",
             &atom::error::ExponentialBackoffRetryPolicy::shouldRetry,
             py::arg("context"), "Check if operation should be retried.")
        .def("get_retry_delay",
             &atom::error::ExponentialBackoffRetryPolicy::getRetryDelay,
             py::arg("attempt_number"), "Get delay before next retry.")
        .def("reset", &atom::error::ExponentialBackoffRetryPolicy::reset,
             "Reset policy state.")
        .def("clone", &atom::error::ExponentialBackoffRetryPolicy::clone,
             "Clone the policy.");

    // JitteredRetryPolicy class
    py::class_<atom::error::JitteredRetryPolicy, atom::error::RetryPolicy,
               std::shared_ptr<atom::error::JitteredRetryPolicy>>(
        m, "JitteredRetryPolicy",
        R"(Jittered retry policy.

Adds randomness to retry delays to prevent thundering herd problem.

Examples:
    >>> from atom.error import JitteredRetryPolicy, FixedIntervalRetryPolicy
    >>> from datetime import timedelta
    >>> base_policy = FixedIntervalRetryPolicy(3, timedelta(seconds=1))
    >>> policy = JitteredRetryPolicy(base_policy, 0.1)
)")
        .def(py::init<std::unique_ptr<atom::error::RetryPolicy>, double>(),
             py::arg("base_policy"), py::arg("jitter_factor") = 0.1,
             R"(Constructs a JitteredRetryPolicy.

Args:
    base_policy (RetryPolicy): Base retry policy to add jitter to
    jitter_factor (float, optional): Jitter factor (0.0-1.0, default: 0.1)
)")
        .def("should_retry", &atom::error::JitteredRetryPolicy::shouldRetry,
             py::arg("context"), "Check if operation should be retried.")
        .def("get_retry_delay",
             &atom::error::JitteredRetryPolicy::getRetryDelay,
             py::arg("attempt_number"),
             "Get delay before next retry with jitter.")
        .def("reset", &atom::error::JitteredRetryPolicy::reset,
             "Reset policy state.")
        .def("clone", &atom::error::JitteredRetryPolicy::clone,
             "Clone the policy.");

    // CircuitBreaker class
    py::class_<atom::error::CircuitBreaker,
               std::shared_ptr<atom::error::CircuitBreaker>>(
        m, "CircuitBreaker",
        R"(Circuit breaker for preventing cascading failures.

Implements the circuit breaker pattern to prevent cascading failures
by failing fast when a service is unavailable.

Examples:
    >>> from atom.error import CircuitBreaker
    >>> from datetime import timedelta
    >>> breaker = CircuitBreaker(5, timedelta(seconds=30), 1)
    >>> breaker.record_success()
    >>> breaker.record_failure()
    >>> state = breaker.get_state()
)")
        .def(py::init<int, std::chrono::milliseconds, int>(),
             py::arg("failure_threshold"), py::arg("timeout"),
             py::arg("success_threshold") = 1,
             R"(Constructs a CircuitBreaker.

Args:
    failure_threshold (int): Number of failures before opening circuit
    timeout (timedelta): Timeout before attempting to close circuit
    success_threshold (int, optional): Number of successes needed to close circuit (default: 1)
)")
        .def("record_success", &atom::error::CircuitBreaker::recordSuccess,
             R"(Record successful operation.

Records a successful operation and updates circuit breaker state.
)")
        .def("record_failure", &atom::error::CircuitBreaker::recordFailure,
             R"(Record failed operation.

Records a failed operation and updates circuit breaker state.
)")
        .def("get_state", &atom::error::CircuitBreaker::getState,
             R"(Get current state.

Returns:
    CircuitBreakerState: Current circuit breaker state
)")
        .def("get_statistics", &atom::error::CircuitBreaker::getStatistics,
             R"(Get failure statistics.

Returns:
    dict[str, int]: Dictionary of statistics including failure count, success count
)")
        .def("reset", &atom::error::CircuitBreaker::reset,
             R"(Reset circuit breaker.

Resets the circuit breaker to its initial state.
)");

    // Bulkhead class
    py::class_<atom::error::Bulkhead, std::shared_ptr<atom::error::Bulkhead>>(
        m, "Bulkhead",
        R"(Bulkhead pattern for resource isolation.

Limits the number of concurrent operations to prevent resource exhaustion.

Examples:
    >>> from atom.error import Bulkhead
    >>> bulkhead = Bulkhead(10)  # Allow max 10 concurrent operations
    >>> stats = bulkhead.get_statistics()
)")
        .def(py::init<int>(), py::arg("max_concurrent_operations"),
             R"(Constructs a Bulkhead.

Args:
    max_concurrent_operations (int): Maximum number of concurrent operations
)")
        .def("get_statistics", &atom::error::Bulkhead::getStatistics,
             R"(Get current statistics.

Returns:
    dict[str, int]: Dictionary of statistics including active operations,
                    total operations, and rejected operations
)");

    // RecoveryStrategyFactory class
    py::class_<atom::error::RecoveryStrategyFactory>(
        m, "RecoveryStrategyFactory",
        R"(Factory for creating common recovery strategies.

Provides factory methods for creating retry policies, circuit breakers,
and other recovery strategies.

Examples:
    >>> from atom.error import RecoveryStrategyFactory
    >>> from datetime import timedelta
    >>> policy = RecoveryStrategyFactory.create_fixed_retry(3, timedelta(seconds=1))
    >>> breaker = RecoveryStrategyFactory.create_circuit_breaker(5, timedelta(seconds=30))
)")
        .def_static("create_fixed_retry",
                    &atom::error::RecoveryStrategyFactory::createFixedRetry,
                    py::arg("max_retries"), py::arg("interval"),
                    R"(Create fixed interval retry policy.

Args:
    max_retries (int): Maximum number of retries
    interval (timedelta): Fixed interval between retries

Returns:
    RetryPolicy: Fixed interval retry policy

Examples:
    >>> from datetime import timedelta
    >>> policy = RecoveryStrategyFactory.create_fixed_retry(3, timedelta(seconds=1))
)")
        .def_static(
            "create_exponential_backoff",
            &atom::error::RecoveryStrategyFactory::createExponentialBackoff,
            py::arg("max_retries"), py::arg("base_delay"),
            R"(Create exponential backoff retry policy.

Args:
    max_retries (int): Maximum number of retries
    base_delay (timedelta): Base delay for first retry

Returns:
    RetryPolicy: Exponential backoff retry policy

Examples:
    >>> from datetime import timedelta
    >>> policy = RecoveryStrategyFactory.create_exponential_backoff(5, timedelta(milliseconds=100))
)")
        .def_static("create_jittered_retry",
                    &atom::error::RecoveryStrategyFactory::createJitteredRetry,
                    py::arg("base_policy"), py::arg("jitter_factor") = 0.1,
                    R"(Create jittered retry policy.

Args:
    base_policy (RetryPolicy): Base retry policy
    jitter_factor (float, optional): Jitter factor (default: 0.1)

Returns:
    RetryPolicy: Jittered retry policy
)")
        .def_static("create_circuit_breaker",
                    &atom::error::RecoveryStrategyFactory::createCircuitBreaker,
                    py::arg("failure_threshold"), py::arg("timeout"),
                    R"(Create circuit breaker.

Args:
    failure_threshold (int): Number of failures before opening circuit
    timeout (timedelta): Timeout before attempting to close circuit

Returns:
    CircuitBreaker: Circuit breaker instance

Examples:
    >>> from datetime import timedelta
    >>> breaker = RecoveryStrategyFactory.create_circuit_breaker(5, timedelta(seconds=30))
)");
}
