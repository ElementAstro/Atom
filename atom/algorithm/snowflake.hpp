#ifndef ATOM_ALGORITHM_SNOWFLAKE_HPP
#define ATOM_ALGORITHM_SNOWFLAKE_HPP

#include <immintrin.h>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "atom/algorithm/rust_numeric.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>
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

// High-performance lock-free atomic operations
class AtomicSnowflakeLock {
public:
    void lock() noexcept {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Use CPU pause instruction for better performance
            _mm_pause();
        }
    }

    void unlock() noexcept { flag_.clear(std::memory_order_release); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

// Reader-writer lock for scenarios with frequent reads
class SharedSnowflakeLock {
public:
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    void lock_shared() { mutex_.lock_shared(); }
    void unlock_shared() { mutex_.unlock_shared(); }

private:
    std::shared_mutex mutex_;
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
    constexpr void lock() noexcept {}

    /**
     * @brief Empty unlock method.
     */
    constexpr void unlock() noexcept {}

    /**
     * @brief Empty lock_shared method.
     */
    constexpr void lock_shared() noexcept {}

    /**
     * @brief Empty unlock_shared method.
     */
    constexpr void unlock_shared() noexcept {}
};

// Cache-aligned structure for thread-local data
struct alignas(64) ThreadLocalState {
    u64 last_timestamp;
    u64 sequence;
    u64 padding[6];  // Pad to full cache line
};

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
template <u64 Twepoch, typename Lock = AtomicSnowflakeLock>
class Snowflake {
    static_assert(std::is_same_v<Lock, SnowflakeNonLock> ||
                      std::is_same_v<Lock, AtomicSnowflakeLock> ||
                      std::is_same_v<Lock, SharedSnowflakeLock> ||
#ifdef ATOM_USE_BOOST
                      std::is_same_v<Lock, boost::mutex>,
#else
                      std::is_same_v<Lock, std::mutex>,
#endif
                  "Lock must be a supported lock type");

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
    Snowflake(Snowflake &&) = delete;
    auto operator=(const Snowflake &) -> Snowflake & = delete;
    auto operator=(Snowflake &&) -> Snowflake & = delete;

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
        if constexpr (std::is_same_v<Lock, SnowflakeNonLock>) {
            // No locking needed
        } else {
            std::lock_guard<Lock> lock(lock_);
        }

        if (worker_id > MAX_WORKER_ID) [[unlikely]] {
            throw InvalidWorkerIdException(worker_id, MAX_WORKER_ID);
        }
        if (datacenter_id > MAX_DATACENTER_ID) [[unlikely]] {
            throw InvalidDatacenterIdException(datacenter_id,
                                               MAX_DATACENTER_ID);
        }

        workerid_.store(worker_id, std::memory_order_relaxed);
        datacenterid_.store(datacenter_id, std::memory_order_relaxed);
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

        // Fast path for single ID generation
        if constexpr (N == 1) {
            return generate_single_id();
        }

        // Optimized batch generation
        auto timestamp = get_current_timestamp();

        if constexpr (std::is_same_v<Lock, SnowflakeNonLock>) {
            // Lock-free single-threaded path
            generate_batch_lockfree<N>(ids, timestamp);
        } else {
            // Thread-safe batch generation
            std::lock_guard<Lock> lock(lock_);
            generate_batch_threadsafe<N>(ids, timestamp);
        }

        return ids;
    }

    // Optimized validation with branch prediction hints
    [[nodiscard]] bool validateId(u64 id) const noexcept {
        const u64 decrypted = id ^ secret_key_.load(std::memory_order_relaxed);
        const u64 timestamp = (decrypted >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
        const u64 datacenter_id =
            (decrypted >> DATACENTER_ID_SHIFT) & MAX_DATACENTER_ID;
        const u64 worker_id = (decrypted >> WORKER_ID_SHIFT) & MAX_WORKER_ID;

        return datacenter_id == datacenterid_.load(std::memory_order_relaxed) &&
               worker_id == workerid_.load(std::memory_order_relaxed) &&
               timestamp <= get_current_timestamp();
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
    [[nodiscard]] constexpr u64 extractTimestamp(u64 id) const noexcept {
        return ((id ^ secret_key_.load(std::memory_order_relaxed)) >>
                TIMESTAMP_LEFT_SHIFT) +
               TWEPOCH;
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
                 u64 &worker_id, u64 &sequence) const noexcept {
        const u64 id =
            encrypted_id ^ secret_key_.load(std::memory_order_relaxed);

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
    void reset() noexcept {
        if constexpr (std::is_same_v<Lock, SnowflakeNonLock>) {
            // No locking needed
        } else {
            std::lock_guard<Lock> lock(lock_);
        }

        last_timestamp_.store(0, std::memory_order_relaxed);
        sequence_.store(0, std::memory_order_relaxed);
        statistics_.total_ids_generated.store(0, std::memory_order_relaxed);
        statistics_.sequence_rollovers.store(0, std::memory_order_relaxed);
        statistics_.timestamp_wait_count.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Retrieves the current worker ID.
     *
     * @return The current worker ID.
     */
    [[nodiscard]] auto getWorkerId() const noexcept -> u64 {
        return workerid_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Retrieves the current datacenter ID.
     *
     * @return The current datacenter ID.
     */
    [[nodiscard]] auto getDatacenterId() const noexcept -> u64 {
        return datacenterid_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Structure for collecting statistics about ID generation.
     */
    struct Statistics {
        /**
         * @brief The total number of IDs generated by this instance.
         */
        std::atomic<u64> total_ids_generated{0};

        /**
         * @brief The number of times the sequence number rolled over.
         */
        std::atomic<u64> sequence_rollovers{0};

        /**
         * @brief The number of times the generator had to wait for the next
         * millisecond due to clock synchronization issues.
         */
        std::atomic<u64> timestamp_wait_count{0};
    };

    /**
     * @brief Retrieves statistics about ID generation.
     *
     * @return A Statistics object containing information about ID generation.
     */
    [[nodiscard]] auto getStatistics() const noexcept -> Statistics {
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
        if constexpr (std::is_same_v<Lock, SharedSnowflakeLock>) {
            std::shared_lock<Lock> lock(lock_);
        } else if constexpr (!std::is_same_v<Lock, SnowflakeNonLock>) {
            std::lock_guard<Lock> lock(lock_);
        }

        return std::to_string(workerid_.load(std::memory_order_relaxed)) + ":" +
               std::to_string(datacenterid_.load(std::memory_order_relaxed)) +
               ":" + std::to_string(sequence_.load(std::memory_order_relaxed)) +
               ":" +
               std::to_string(last_timestamp_.load(std::memory_order_relaxed)) +
               ":" +
               std::to_string(secret_key_.load(std::memory_order_relaxed));
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
        if constexpr (std::is_same_v<Lock, SnowflakeNonLock>) {
            // No locking needed
        } else {
            std::lock_guard<Lock> lock(lock_);
        }

        const auto parts = split_string(state, ':');
        if (parts.size() != 5) [[unlikely]] {
            throw SnowflakeException("Invalid serialized state");
        }

        workerid_.store(std::stoull(parts[0]), std::memory_order_relaxed);
        datacenterid_.store(std::stoull(parts[1]), std::memory_order_relaxed);
        sequence_.store(std::stoull(parts[2]), std::memory_order_relaxed);
        last_timestamp_.store(std::stoull(parts[3]), std::memory_order_relaxed);
        secret_key_.store(std::stoull(parts[4]), std::memory_order_relaxed);
    }

private:
    // Cache-aligned atomic members
    alignas(64) std::atomic<u64> workerid_{0};
    alignas(64) std::atomic<u64> datacenterid_{0};
    alignas(64) std::atomic<u64> sequence_{0};
    alignas(64) std::atomic<u64> last_timestamp_{0};
    alignas(64) std::atomic<u64> secret_key_{0};

    mutable Lock lock_;
    mutable Statistics statistics_;

    // High-resolution timestamp with optimized caching
    alignas(64) mutable std::atomic<u64> cached_timestamp_{0};
    alignas(64) mutable std::atomic<
        std::chrono::steady_clock::time_point> cached_time_point_{};

    const std::chrono::steady_clock::time_point start_time_point_ =
        std::chrono::steady_clock::now();
    const u64 start_millisecond_ = get_system_millis();

    // Thread-local state for better cache locality
    static thread_local ThreadLocalState thread_state_;

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
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<u64> distr;
        secret_key_.store(distr(eng), std::memory_order_relaxed);

        if (workerid_.load(std::memory_order_relaxed) > MAX_WORKER_ID)
            [[unlikely]] {
            throw InvalidWorkerIdException(
                workerid_.load(std::memory_order_relaxed), MAX_WORKER_ID);
        }
        if (datacenterid_.load(std::memory_order_relaxed) > MAX_DATACENTER_ID)
            [[unlikely]] {
            throw InvalidDatacenterIdException(
                datacenterid_.load(std::memory_order_relaxed),
                MAX_DATACENTER_ID);
        }
    }

    /**
     * @brief Gets the current system time in milliseconds.
     *
     * @return The current system time in milliseconds since the epoch.
     */
    [[nodiscard]] auto get_system_millis() const noexcept -> u64 {
        return static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    // Optimized timestamp generation with reduced system calls
    [[nodiscard]] auto get_current_timestamp() const noexcept -> u64 {
        const auto now = std::chrono::steady_clock::now();
        const auto cached_time =
            cached_time_point_.load(std::memory_order_relaxed);

        // Check if we can use cached timestamp (within 1ms)
        if (now - cached_time < std::chrono::milliseconds(1)) [[likely]] {
            return cached_timestamp_.load(std::memory_order_relaxed);
        }

        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - start_time_point_)
                              .count();
        const u64 timestamp = start_millisecond_ + static_cast<u64>(diff);

        // Update cache atomically
        cached_timestamp_.store(timestamp, std::memory_order_relaxed);
        cached_time_point_.store(now, std::memory_order_relaxed);

        return timestamp;
    }

    // Optimized single ID generation
    template <usize N>
    [[nodiscard]] auto generate_single_id() -> std::array<u64, N> {
        static_assert(N == 1);

        const u64 timestamp = get_current_timestamp();
        u64 current_sequence;
        u64 last_ts = last_timestamp_.load(std::memory_order_relaxed);

        if (timestamp == last_ts) [[likely]] {
            current_sequence =
                sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
            if ((current_sequence & SEQUENCE_MASK) == 0) [[unlikely]] {
                // Sequence overflow, wait for next millisecond
                const u64 next_ts = wait_next_millis(timestamp);
                last_timestamp_.store(next_ts, std::memory_order_relaxed);
                sequence_.store(0, std::memory_order_relaxed);
                current_sequence = 0;
                statistics_.sequence_rollovers.fetch_add(
                    1, std::memory_order_relaxed);
            }
        } else {
            last_timestamp_.store(timestamp, std::memory_order_relaxed);
            sequence_.store(0, std::memory_order_relaxed);
            current_sequence = 0;
        }

        current_sequence &= SEQUENCE_MASK;
        statistics_.total_ids_generated.fetch_add(1, std::memory_order_relaxed);

        const u64 id =
            ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT) |
            (datacenterid_.load(std::memory_order_relaxed)
             << DATACENTER_ID_SHIFT) |
            (workerid_.load(std::memory_order_relaxed) << WORKER_ID_SHIFT) |
            current_sequence;

        return {id ^ secret_key_.load(std::memory_order_relaxed)};
    }

    // Lock-free batch generation for single-threaded scenarios
    template <usize N>
    void generate_batch_lockfree(std::array<u64, N> &ids, u64 timestamp) {
        u64 current_sequence = sequence_.load(std::memory_order_relaxed);
        u64 last_ts = last_timestamp_.load(std::memory_order_relaxed);

        for (usize i = 0; i < N; ++i) {
            if (timestamp == last_ts) {
                ++current_sequence;
                if ((current_sequence & SEQUENCE_MASK) == 0) [[unlikely]] {
                    timestamp = wait_next_millis(timestamp);
                    last_ts = timestamp;
                    current_sequence = 0;
                    statistics_.sequence_rollovers.fetch_add(
                        1, std::memory_order_relaxed);
                }
            } else {
                last_ts = timestamp;
                current_sequence = 0;
            }

            const u64 masked_sequence = current_sequence & SEQUENCE_MASK;
            const u64 id =
                ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT) |
                (datacenterid_.load(std::memory_order_relaxed)
                 << DATACENTER_ID_SHIFT) |
                (workerid_.load(std::memory_order_relaxed) << WORKER_ID_SHIFT) |
                masked_sequence;

            ids[i] = id ^ secret_key_.load(std::memory_order_relaxed);
        }

        sequence_.store(current_sequence, std::memory_order_relaxed);
        last_timestamp_.store(last_ts, std::memory_order_relaxed);
        statistics_.total_ids_generated.fetch_add(N, std::memory_order_relaxed);
    }

    // Thread-safe batch generation
    template <usize N>
    void generate_batch_threadsafe(std::array<u64, N> &ids, u64 timestamp) {
        u64 current_sequence = sequence_.load(std::memory_order_relaxed);
        u64 last_ts = last_timestamp_.load(std::memory_order_relaxed);

        if (timestamp < last_ts) [[unlikely]] {
            throw InvalidTimestampException(timestamp);
        }

        for (usize i = 0; i < N; ++i) {
            if (timestamp == last_ts) {
                ++current_sequence;
                if ((current_sequence & SEQUENCE_MASK) == 0) [[unlikely]] {
                    timestamp = wait_next_millis(timestamp);
                    last_ts = timestamp;
                    current_sequence = 0;
                    statistics_.sequence_rollovers.fetch_add(
                        1, std::memory_order_relaxed);
                }
            } else {
                last_ts = timestamp;
                current_sequence = 0;
            }

            const u64 masked_sequence = current_sequence & SEQUENCE_MASK;
            const u64 id =
                ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT) |
                (datacenterid_.load(std::memory_order_relaxed)
                 << DATACENTER_ID_SHIFT) |
                (workerid_.load(std::memory_order_relaxed) << WORKER_ID_SHIFT) |
                masked_sequence;

            ids[i] = id ^ secret_key_.load(std::memory_order_relaxed);
        }

        sequence_.store(current_sequence, std::memory_order_relaxed);
        last_timestamp_.store(last_ts, std::memory_order_relaxed);
        statistics_.total_ids_generated.fetch_add(N, std::memory_order_relaxed);
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
    [[nodiscard]] auto wait_next_millis(u64 last) const -> u64 {
        u64 timestamp = get_current_timestamp();
        while (timestamp <= last) {
            // Use CPU pause for better performance in spin-wait
            _mm_pause();
            timestamp = get_current_timestamp();
            statistics_.timestamp_wait_count.fetch_add(
                1, std::memory_order_relaxed);
        }
        return timestamp;
    }

    // Optimized string splitting
    [[nodiscard]] static auto split_string(const std::string &str,
                                           char delimiter)
        -> std::vector<std::string> {
        std::vector<std::string> parts;
        parts.reserve(8);  // Reserve space for typical use case

        std::string::size_type start = 0;
        std::string::size_type end = str.find(delimiter);

        while (end != std::string::npos) {
            parts.emplace_back(str.substr(start, end - start));
            start = end + 1;
            end = str.find(delimiter, start);
        }

        parts.emplace_back(str.substr(start));
        return parts;
    }
};

// Thread-local storage initialization
template <u64 Twepoch, typename Lock>
thread_local ThreadLocalState  // Removed typename Snowflake<Twepoch, Lock>::
    Snowflake<Twepoch, Lock>::thread_state_{};

// Convenience aliases for common configurations
using FastSnowflake = Snowflake<1609459200000ULL, AtomicSnowflakeLock>;
using SharedSnowflake = Snowflake<1609459200000ULL, SharedSnowflakeLock>;
using SingleThreadSnowflake = Snowflake<1609459200000ULL, SnowflakeNonLock>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SNOWFLAKE_HPP
