#ifndef ATOM_ALGORITHM_SNOWFLAKE_HPP
#define ATOM_ALGORITHM_SNOWFLAKE_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "atom/algorithm/rust_numeric.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#endif

namespace atom::algorithm {

/**
 * @brief Custom exception class for Snowflake-related errors.
 *
 * This class inherits from std::runtime_error and provides a base for more
 * specific Snowflake exceptions.
 */
class SnowflakeException : public std::runtime_error {
public:
    /**
     * @brief Constructs a SnowflakeException with a specified error message.
     *
     * @param message The error message associated with the exception.
     */
    explicit SnowflakeException(const std::string &message)
        : std::runtime_error(message) {}
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

/**
 * @brief A class for generating unique IDs using the Snowflake algorithm.
 *
 * The Snowflake algorithm generates 64-bit unique IDs that are time-based and
 * incorporate worker and datacenter identifiers to ensure uniqueness across
 * multiple instances and systems.
 *
 * @tparam Twepoch The custom epoch (in milliseconds) to subtract from the
 * current timestamp. This allows for a smaller timestamp value in the ID.
 * @tparam Lock The lock type to use for thread safety. Defaults to
 * SnowflakeNonLock for no locking.
 */
template <u64 Twepoch, typename Lock = SnowflakeNonLock>
class Snowflake {
    static_assert(std::is_same_v<Lock, SnowflakeNonLock> ||
#ifdef ATOM_USE_BOOST
                      std::is_same_v<Lock, boost::mutex>,
#else
                      std::is_same_v<Lock, std::mutex>,
#endif
                  "Lock must be SnowflakeNonLock, std::mutex or boost::mutex");

public:
    using lock_type = Lock;

    /**
     * @brief The custom epoch (in milliseconds) used as the starting point for
     * timestamp generation.
     */
    static constexpr u64 TWEPOCH = Twepoch;

    /**
     * @brief The number of bits used to represent the worker ID.
     */
    static constexpr u64 WORKER_ID_BITS = 5;

    /**
     * @brief The number of bits used to represent the datacenter ID.
     */
    static constexpr u64 DATACENTER_ID_BITS = 5;

    /**
     * @brief The maximum value that can be assigned to a worker ID.
     */
    static constexpr u64 MAX_WORKER_ID = (1ULL << WORKER_ID_BITS) - 1;

    /**
     * @brief The maximum value that can be assigned to a datacenter ID.
     */
    static constexpr u64 MAX_DATACENTER_ID = (1ULL << DATACENTER_ID_BITS) - 1;

    /**
     * @brief The number of bits used to represent the sequence number.
     */
    static constexpr u64 SEQUENCE_BITS = 12;

    /**
     * @brief The number of bits to shift the worker ID to the left.
     */
    static constexpr u64 WORKER_ID_SHIFT = SEQUENCE_BITS;

    /**
     * @brief The number of bits to shift the datacenter ID to the left.
     */
    static constexpr u64 DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;

    /**
     * @brief The number of bits to shift the timestamp to the left.
     */
    static constexpr u64 TIMESTAMP_LEFT_SHIFT =
        SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;

    /**
     * @brief A mask used to extract the sequence number from an ID.
     */
    static constexpr u64 SEQUENCE_MASK = (1ULL << SEQUENCE_BITS) - 1;

    /**
     * @brief Constructs a Snowflake ID generator with specified worker and
     * datacenter IDs.
     *
     * @param worker_id The ID of the worker generating the IDs. Must be less
     * than or equal to MAX_WORKER_ID.
     * @param datacenter_id The ID of the datacenter where the worker is
     * located. Must be less than or equal to MAX_DATACENTER_ID.
     * @throws InvalidWorkerIdException If the worker_id is greater than
     * MAX_WORKER_ID.
     * @throws InvalidDatacenterIdException If the datacenter_id is greater than
     * MAX_DATACENTER_ID.
     */
    explicit Snowflake(u64 worker_id = 0, u64 datacenter_id = 0)
        : workerid_(worker_id), datacenterid_(datacenter_id) {
        initialize();
    }

    Snowflake(const Snowflake &) = delete;
    auto operator=(const Snowflake &) -> Snowflake & = delete;

    /**
     * @brief Initializes the Snowflake ID generator with new worker and
     * datacenter IDs.
     *
     * This method allows changing the worker and datacenter IDs after the
     * Snowflake object has been constructed.
     *
     * @param worker_id The new ID of the worker generating the IDs. Must be
     * less than or equal to MAX_WORKER_ID.
     * @param datacenter_id The new ID of the datacenter where the worker is
     * located. Must be less than or equal to MAX_DATACENTER_ID.
     * @throws InvalidWorkerIdException If the worker_id is greater than
     * MAX_WORKER_ID.
     * @throws InvalidDatacenterIdException If the datacenter_id is greater than
     * MAX_DATACENTER_ID.
     */
    void init(u64 worker_id, u64 datacenter_id) {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        if (worker_id > MAX_WORKER_ID) {
            throw InvalidWorkerIdException(worker_id, MAX_WORKER_ID);
        }
        if (datacenter_id > MAX_DATACENTER_ID) {
            throw InvalidDatacenterIdException(datacenter_id,
                                               MAX_DATACENTER_ID);
        }
        workerid_ = worker_id;
        datacenterid_ = datacenter_id;
    }

    /**
     * @brief Generates a batch of unique IDs.
     *
     * This method generates an array of unique IDs based on the Snowflake
     * algorithm. It is optimized for generating multiple IDs at once to
     * improve performance.
     *
     * @tparam N The number of IDs to generate. Defaults to 1.
     * @return An array containing the generated unique IDs.
     * @throws InvalidTimestampException If the system clock is adjusted
     * backwards or if there is an issue with timestamp generation.
     */
    template <usize N = 1>
    [[nodiscard]] auto nextid() -> std::array<u64, N> {
        std::array<u64, N> ids;
        u64 timestamp = current_millis();

#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        if (timestamp < last_timestamp_) {
            throw InvalidTimestampException(timestamp);
        }

        if (last_timestamp_ == timestamp) {
            sequence_ = (sequence_ + 1) & SEQUENCE_MASK;
            if (sequence_ == 0) {
                timestamp = wait_next_millis(last_timestamp_);
                if (timestamp < last_timestamp_) {
                    throw InvalidTimestampException(timestamp);
                }
            }
        } else {
            sequence_ = 0;
        }

        last_timestamp_ = timestamp;

        for (usize i = 0; i < N; ++i) {
            if (timestamp < last_timestamp_) {
                throw InvalidTimestampException(timestamp);
            }

            if (last_timestamp_ == timestamp) {
                sequence_ = (sequence_ + 1) & SEQUENCE_MASK;
                if (sequence_ == 0) {
                    timestamp = wait_next_millis(last_timestamp_);
                    if (timestamp < last_timestamp_) {
                        throw InvalidTimestampException(timestamp);
                    }
                }
            } else {
                sequence_ = 0;
            }

            last_timestamp_ = timestamp;

            ids[i] = ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT) |
                     (datacenterid_ << DATACENTER_ID_SHIFT) |
                     (workerid_ << WORKER_ID_SHIFT) | sequence_;
            ids[i] ^= secret_key_;
        }

        return ids;
    }

    /**
     * @brief Validates if an ID was generated by this Snowflake instance.
     *
     * This method checks if a given ID was generated by this specific
     * Snowflake instance by verifying the datacenter ID, worker ID, and
     * timestamp.
     *
     * @param id The ID to validate.
     * @return True if the ID was generated by this instance, false otherwise.
     */
    [[nodiscard]] bool validateId(u64 id) const {
        u64 decrypted = id ^ secret_key_;
        u64 timestamp = (decrypted >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
        u64 datacenter_id =
            (decrypted >> DATACENTER_ID_SHIFT) & MAX_DATACENTER_ID;
        u64 worker_id = (decrypted >> WORKER_ID_SHIFT) & MAX_WORKER_ID;

        return datacenter_id == datacenterid_ && worker_id == workerid_ &&
               timestamp <= current_millis();
    }

    /**
     * @brief Extracts the timestamp from a Snowflake ID.
     *
     * This method extracts the timestamp component from a given Snowflake ID.
     *
     * @param id The Snowflake ID.
     * @return The timestamp (in milliseconds since the epoch) extracted from
     * the ID.
     */
    [[nodiscard]] u64 extractTimestamp(u64 id) const {
        return ((id ^ secret_key_) >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
    }

    /**
     * @brief Parses a Snowflake ID into its constituent parts.
     *
     * This method decomposes a Snowflake ID into its timestamp, datacenter ID,
     * worker ID, and sequence number components.
     *
     * @param encrypted_id The Snowflake ID to parse.
     * @param timestamp A reference to store the extracted timestamp.
     * @param datacenter_id A reference to store the extracted datacenter ID.
     * @param worker_id A reference to store the extracted worker ID.
     * @param sequence A reference to store the extracted sequence number.
     */
    void parseId(u64 encrypted_id, u64 &timestamp, u64 &datacenter_id,
                 u64 &worker_id, u64 &sequence) const {
        u64 id = encrypted_id ^ secret_key_;

        timestamp = (id >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
        datacenter_id = (id >> DATACENTER_ID_SHIFT) & MAX_DATACENTER_ID;
        worker_id = (id >> WORKER_ID_SHIFT) & MAX_WORKER_ID;
        sequence = id & SEQUENCE_MASK;
    }

    /**
     * @brief Resets the Snowflake ID generator to its initial state.
     *
     * This method resets the internal state of the Snowflake ID generator,
     * effectively starting the sequence from 0 and resetting the last
     * timestamp.
     */
    void reset() {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        last_timestamp_ = 0;
        sequence_ = 0;
    }

    /**
     * @brief Retrieves the current worker ID.
     *
     * @return The current worker ID.
     */
    [[nodiscard]] auto getWorkerId() const -> u64 { return workerid_; }

    /**
     * @brief Retrieves the current datacenter ID.
     *
     * @return The current datacenter ID.
     */
    [[nodiscard]] auto getDatacenterId() const -> u64 { return datacenterid_; }

    /**
     * @brief Structure for collecting statistics about ID generation.
     */
    struct Statistics {
        /**
         * @brief The total number of IDs generated by this instance.
         */
        u64 total_ids_generated;

        /**
         * @brief The number of times the sequence number rolled over.
         */
        u64 sequence_rollovers;

        /**
         * @brief The number of times the generator had to wait for the next
         * millisecond due to clock synchronization issues.
         */
        u64 timestamp_wait_count;
    };

    /**
     * @brief Retrieves statistics about ID generation.
     *
     * @return A Statistics object containing information about ID generation.
     */
    [[nodiscard]] Statistics getStatistics() const {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        return statistics_;
    }

    /**
     * @brief Serializes the current state of the Snowflake generator to a
     * string.
     *
     * This method serializes the internal state of the Snowflake generator,
     * including the worker ID, datacenter ID, sequence number, last timestamp,
     * and secret key, into a string format.
     *
     * @return A string representing the serialized state of the Snowflake
     * generator.
     */
    [[nodiscard]] std::string serialize() const {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        return std::to_string(workerid_) + ":" + std::to_string(datacenterid_) +
               ":" + std::to_string(sequence_) + ":" +
               std::to_string(last_timestamp_.load()) + ":" +
               std::to_string(secret_key_);
    }

    /**
     * @brief Deserializes the state of the Snowflake generator from a string.
     *
     * This method deserializes the internal state of the Snowflake generator
     * from a string, restoring the worker ID, datacenter ID, sequence number,
     * last timestamp, and secret key.
     *
     * @param state A string representing the serialized state of the Snowflake
     * generator.
     * @throws SnowflakeException If the provided state string is invalid.
     */
    void deserialize(const std::string &state) {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        std::vector<std::string> parts;
        std::stringstream ss(state);
        std::string part;

        while (std::getline(ss, part, ':')) {
            parts.push_back(part);
        }

        if (parts.size() != 5) {
            throw SnowflakeException("Invalid serialized state");
        }

        workerid_ = std::stoull(parts[0]);
        datacenterid_ = std::stoull(parts[1]);
        sequence_ = std::stoull(parts[2]);
        last_timestamp_.store(std::stoull(parts[3]));
        secret_key_ = std::stoull(parts[4]);
    }

private:
    Statistics statistics_{};

    /**
     * @brief Thread-local cache for sequence and timestamp to reduce lock
     * contention.
     */
    struct ThreadLocalCache {
        /**
         * @brief The last timestamp used by this thread.
         */
        u64 last_timestamp;

        /**
         * @brief The sequence number for the last timestamp used by this
         * thread.
         */
        u64 sequence;
    };

    /**
     * @brief Thread-local instance of the ThreadLocalCache.
     */
    static thread_local ThreadLocalCache thread_cache_;

    /**
     * @brief The ID of the worker generating the IDs.
     */
    u64 workerid_ = 0;

    /**
     * @brief The ID of the datacenter where the worker is located.
     */
    u64 datacenterid_ = 0;

    /**
     * @brief The current sequence number.
     */
    u64 sequence_ = 0;

    /**
     * @brief The lock used to synchronize access to the Snowflake generator.
     */
    mutable mutex_type lock_;

    /**
     * @brief A secret key used to encrypt the generated IDs.
     */
    u64 secret_key_;

    /**
     * @brief The last generated timestamp.
     */
    std::atomic<u64> last_timestamp_{0};

    /**
     * @brief The time point when the Snowflake generator was started.
     */
    std::chrono::steady_clock::time_point start_time_point_ =
        std::chrono::steady_clock::now();

    /**
     * @brief The system time in milliseconds when the Snowflake generator was
     * started.
     */
    u64 start_millisecond_ = get_system_millis();

#ifdef ATOM_USE_BOOST
    boost::random::mt19937_64 eng_;
    boost::random::uniform_int_distribution<u64> distr_;
#endif

    /**
     * @brief Initializes the Snowflake ID generator.
     *
     * This method initializes the Snowflake ID generator by setting the worker
     * ID, datacenter ID, and generating a secret key.
     *
     * @throws InvalidWorkerIdException If the worker_id is greater than
     * MAX_WORKER_ID.
     * @throws InvalidDatacenterIdException If the datacenter_id is greater than
     * MAX_DATACENTER_ID.
     */
    void initialize() {
#ifdef ATOM_USE_BOOST
        boost::random::random_device rd;
        eng_.seed(rd());
        secret_key_ = distr_(eng_);
#else
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<u64> distr;
        secret_key_ = distr(eng);
#endif

        if (workerid_ > MAX_WORKER_ID) {
            throw InvalidWorkerIdException(workerid_, MAX_WORKER_ID);
        }
        if (datacenterid_ > MAX_DATACENTER_ID) {
            throw InvalidDatacenterIdException(datacenterid_,
                                               MAX_DATACENTER_ID);
        }
    }

    /**
     * @brief Gets the current system time in milliseconds.
     *
     * @return The current system time in milliseconds since the epoch.
     */
    [[nodiscard]] auto get_system_millis() const -> u64 {
        return static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    /**
     * @brief Generates the current timestamp in milliseconds.
     *
     * This method generates the current timestamp in milliseconds, taking into
     * account the start time of the Snowflake generator.
     *
     * @return The current timestamp in milliseconds.
     */
    [[nodiscard]] auto current_millis() const -> u64 {
        static thread_local u64 last_cached_millis = 0;
        static thread_local std::chrono::steady_clock::time_point
            last_time_point;

        auto now = std::chrono::steady_clock::now();
        if (now - last_time_point < std::chrono::milliseconds(1)) {
            return last_cached_millis;
        }

        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - start_time_point_)
                        .count();
        last_cached_millis = start_millisecond_ + static_cast<u64>(diff);
        last_time_point = now;
        return last_cached_millis;
    }

    /**
     * @brief Waits until the next millisecond to avoid generating duplicate
     * IDs.
     *
     * This method waits until the current timestamp is greater than the last
     * generated timestamp, ensuring that IDs are generated with increasing
     * timestamps.
     *
     * @param last The last generated timestamp.
     * @return The next valid timestamp.
     */
    [[nodiscard]] auto wait_next_millis(u64 last) -> u64 {
        u64 timestamp = current_millis();
        while (timestamp <= last) {
            timestamp = current_millis();
            ++statistics_.timestamp_wait_count;
        }
        return timestamp;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SNOWFLAKE_HPP
