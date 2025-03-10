#ifndef ATOM_ALGORITHM_SNOWFLAKE_HPP
#define ATOM_ALGORITHM_SNOWFLAKE_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#endif

namespace atom::algorithm {

// Custom exception classes for clearer error handling
class SnowflakeException : public std::runtime_error {
public:
    explicit SnowflakeException(const std::string &message)
        : std::runtime_error(message) {}
};

class InvalidWorkerIdException : public SnowflakeException {
public:
    InvalidWorkerIdException(uint64_t worker_id, uint64_t max)
        : SnowflakeException("Worker ID " + std::to_string(worker_id) +
                             " exceeds maximum of " + std::to_string(max)) {}
};

class InvalidDatacenterIdException : public SnowflakeException {
public:
    InvalidDatacenterIdException(uint64_t datacenter_id, uint64_t max)
        : SnowflakeException("Datacenter ID " + std::to_string(datacenter_id) +
                             " exceeds maximum of " + std::to_string(max)) {}
};

class InvalidTimestampException : public SnowflakeException {
public:
    InvalidTimestampException(uint64_t timestamp)
        : SnowflakeException("Timestamp " + std::to_string(timestamp) +
                             " is invalid or out of range.") {}
};

class SnowflakeNonLock {
public:
    void lock() {}
    void unlock() {}
};

#ifdef ATOM_USE_BOOST
using boost_lock_guard = boost::lock_guard<boost::mutex>;
using mutex_type = boost::mutex;
#else
using std_lock_guard = std::lock_guard<std::mutex>;
using mutex_type = std::mutex;
#endif

template <uint64_t Twepoch, typename Lock = SnowflakeNonLock>
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
    static constexpr uint64_t TWEPOCH = Twepoch;
    static constexpr uint64_t WORKER_ID_BITS = 5;
    static constexpr uint64_t DATACENTER_ID_BITS = 5;
    static constexpr uint64_t MAX_WORKER_ID = (1ULL << WORKER_ID_BITS) - 1;
    static constexpr uint64_t MAX_DATACENTER_ID =
        (1ULL << DATACENTER_ID_BITS) - 1;
    static constexpr uint64_t SEQUENCE_BITS = 12;
    static constexpr uint64_t WORKER_ID_SHIFT = SEQUENCE_BITS;
    static constexpr uint64_t DATACENTER_ID_SHIFT =
        SEQUENCE_BITS + WORKER_ID_BITS;
    static constexpr uint64_t TIMESTAMP_LEFT_SHIFT =
        SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;
    static constexpr uint64_t SEQUENCE_MASK = (1ULL << SEQUENCE_BITS) - 1;

    explicit Snowflake(uint64_t worker_id = 0, uint64_t datacenter_id = 0)
        : workerid_(worker_id), datacenterid_(datacenter_id) {
        initialize();
    }

    Snowflake(const Snowflake &) = delete;
    auto operator=(const Snowflake &) -> Snowflake & = delete;

    void init(uint64_t worker_id, uint64_t datacenter_id) {
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

    // Generate a batch of IDs for better performance
    template <size_t N = 1>
    [[nodiscard]] auto nextid() -> std::array<uint64_t, N> {
        std::array<uint64_t, N> ids;
        uint64_t timestamp = current_millis();

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

        for (size_t i = 0; i < N; ++i) {
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

    // Validate if an ID was generated by this instance
    [[nodiscard]] bool validateId(uint64_t id) const {
        uint64_t decrypted = id ^ secret_key_;
        uint64_t timestamp = (decrypted >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
        uint64_t datacenter_id =
            (decrypted >> DATACENTER_ID_SHIFT) & MAX_DATACENTER_ID;
        uint64_t worker_id = (decrypted >> WORKER_ID_SHIFT) & MAX_WORKER_ID;

        return datacenter_id == datacenterid_ && worker_id == workerid_ &&
               timestamp <= current_millis();
    }

    // Extract timestamp from ID
    [[nodiscard]] uint64_t extractTimestamp(uint64_t id) const {
        return ((id ^ secret_key_) >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
    }

    void parseId(uint64_t encrypted_id, uint64_t &timestamp,
                 uint64_t &datacenter_id, uint64_t &worker_id,
                 uint64_t &sequence) const {
        uint64_t id = encrypted_id ^ secret_key_;

        timestamp = (id >> TIMESTAMP_LEFT_SHIFT) + TWEPOCH;
        datacenter_id = (id >> DATACENTER_ID_SHIFT) & MAX_DATACENTER_ID;
        worker_id = (id >> WORKER_ID_SHIFT) & MAX_WORKER_ID;
        sequence = id & SEQUENCE_MASK;
    }

    // Additional functionality: Reset the Snowflake generator
    void reset() {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        last_timestamp_ = 0;
        sequence_ = 0;
    }

    // Additional functionality: Retrieve current worker ID
    [[nodiscard]] auto getWorkerId() const -> uint64_t { return workerid_; }

    // Additional functionality: Retrieve current datacenter ID
    [[nodiscard]] auto getDatacenterId() const -> uint64_t {
        return datacenterid_;
    }

    // Get statistics about ID generation
    struct Statistics {
        uint64_t total_ids_generated;
        uint64_t sequence_rollovers;
        uint64_t timestamp_wait_count;
    };

    [[nodiscard]] Statistics getStatistics() const {
#ifdef ATOM_USE_BOOST
        boost_lock_guard lock(lock_);
#else
        std_lock_guard lock(lock_);
#endif
        return statistics_;
    }

    // Serialize current state to string
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

    // Deserialize state from string
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
    // Thread-local sequence cache to reduce lock contention
    struct ThreadLocalCache {
        uint64_t last_timestamp;
        uint64_t sequence;
    };

    static thread_local ThreadLocalCache thread_cache_;

    uint64_t workerid_ = 0;
    uint64_t datacenterid_ = 0;
    uint64_t sequence_ = 0;
    mutable mutex_type lock_;
    uint64_t secret_key_;

    std::atomic<uint64_t> last_timestamp_{0};
    std::chrono::steady_clock::time_point start_time_point_ =
        std::chrono::steady_clock::now();
    uint64_t start_millisecond_ = get_system_millis();

#ifdef ATOM_USE_BOOST
    boost::random::mt19937_64 eng_;
    boost::random::uniform_int_distribution<uint64_t> distr_;
#endif

    void initialize() {
#ifdef ATOM_USE_BOOST
        boost::random::random_device rd;
        eng_.seed(rd());
        secret_key_ = distr_(eng_);
#else
        std::random_device rd;
        std::mt19937_64 eng(rd());
        std::uniform_int_distribution<uint64_t> distr;
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

    [[nodiscard]] auto get_system_millis() const -> uint64_t {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }

    // Optimized timestamp generation with caching
    [[nodiscard]] auto current_millis() const -> uint64_t {
        static thread_local uint64_t last_cached_millis = 0;
        static thread_local std::chrono::steady_clock::time_point
            last_time_point;

        auto now = std::chrono::steady_clock::now();
        if (now - last_time_point < std::chrono::milliseconds(1)) {
            return last_cached_millis;
        }

        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - start_time_point_)
                        .count();
        last_cached_millis = start_millisecond_ + static_cast<uint64_t>(diff);
        last_time_point = now;
        return last_cached_millis;
    }

    [[nodiscard]] auto wait_next_millis(uint64_t last) -> uint64_t {
        uint64_t timestamp = current_millis();
        while (timestamp <= last) {
            timestamp = current_millis();
            ++statistics_.timestamp_wait_count;
        }
        return timestamp;
    }
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SNOWFLAKE_HPP
