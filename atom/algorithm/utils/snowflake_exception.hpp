#ifndef ATOM_ALGORITHM_UTILS_SNOWFLAKE_EXCEPTION_HPP
#define ATOM_ALGORITHM_UTILS_SNOWFLAKE_EXCEPTION_HPP

#include <mutex>
#include <string>

#include "atom/algorithm/algorithm_exception.hpp"  // UtilsException
#include "atom/algorithm/core/rust_numeric.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Custom exception class for Snowflake-related errors.
 *
 * Inherits from UtilsException in the unified algorithm exception hierarchy.
 * Provides a simple string constructor for backward compatibility.
 */
class SnowflakeException : public UtilsException {
public:
    explicit SnowflakeException(const std::string &message)
        : UtilsException("", 0, "", message) {}
    using UtilsException::UtilsException;
};

/**
 * @brief Exception class for invalid worker ID errors.
 *
 * This exception is thrown when the configured worker ID exceeds the maximum
 * allowed value.
 */
class InvalidWorkerIdException : public SnowflakeException {
public:
    /**
     * @brief Constructs an InvalidWorkerIdException with details about the
     * invalid worker ID.
     *
     * @param worker_id The invalid worker ID.
     * @param max The maximum allowed worker ID.
     */
    InvalidWorkerIdException(u64 worker_id, u64 max)
        : SnowflakeException("Worker ID " + std::to_string(worker_id) +
                             " exceeds maximum of " + std::to_string(max)) {}
};

/**
 * @brief Exception class for invalid datacenter ID errors.
 *
 * This exception is thrown when the configured datacenter ID exceeds the
 * maximum allowed value.
 */
class InvalidDatacenterIdException : public SnowflakeException {
public:
    /**
     * @brief Constructs an InvalidDatacenterIdException with details about the
     * invalid datacenter ID.
     *
     * @param datacenter_id The invalid datacenter ID.
     * @param max The maximum allowed datacenter ID.
     */
    InvalidDatacenterIdException(u64 datacenter_id, u64 max)
        : SnowflakeException("Datacenter ID " + std::to_string(datacenter_id) +
                             " exceeds maximum of " + std::to_string(max)) {}
};

/**
 * @brief Exception class for invalid timestamp errors.
 *
 * This exception is thrown when a generated timestamp is invalid or out of
 * range, typically indicating clock synchronization issues.
 */
class InvalidTimestampException : public SnowflakeException {
public:
    /**
     * @brief Constructs an InvalidTimestampException with details about the
     * invalid timestamp.
     *
     * @param timestamp The invalid timestamp.
     */
    InvalidTimestampException(u64 timestamp)
        : SnowflakeException("Timestamp " + std::to_string(timestamp) +
                             " is invalid or out of range.") {}
};

/**
 * @brief A no-op lock class for scenarios where locking is not required.
 *
 * This class provides empty lock and unlock methods, effectively disabling
 * locking. It is used as a template parameter to allow the Snowflake class to
 * operate without synchronization overhead.
 */
class SnowflakeNonLock {
public:
    /**
     * @brief Empty lock method.
     */
    void lock() {}

    /**
     * @brief Empty unlock method.
     */
    void unlock() {}
};

#ifdef ATOM_USE_BOOST
using boost_lock_guard = boost::lock_guard<boost::mutex>;
using mutex_type = boost::mutex;
#else
using std_lock_guard = std::lock_guard<std::mutex>;
using mutex_type = std::mutex;
#endif

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_UTILS_SNOWFLAKE_EXCEPTION_HPP
