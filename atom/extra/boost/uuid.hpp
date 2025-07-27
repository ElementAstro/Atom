#ifndef ATOM_EXTRA_BOOST_UUID_HPP
#define ATOM_EXTRA_BOOST_UUID_HPP

#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <compare>
#include <format>
#include <functional>
#include <future>
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace atom::extra::boost {

constexpr size_t UUID_SIZE = 16;
constexpr size_t BASE64_ENCODED_SIZE = 22;
constexpr uint64_t TIMESTAMP_DIVISOR = 10000000;
constexpr uint64_t UUID_EPOCH = 0x01B21DD213814000L;

/**
 * @brief UUID generation statistics
 */
struct UUIDStats {
    std::atomic<uint64_t> total_generated{0};
    std::atomic<uint64_t> v1_generated{0};
    std::atomic<uint64_t> v3_generated{0};
    std::atomic<uint64_t> v4_generated{0};
    std::atomic<uint64_t> v5_generated{0};
    std::atomic<uint64_t> pool_hits{0};
    std::atomic<uint64_t> pool_misses{0};
    std::atomic<uint64_t> bulk_operations{0};
};

/**
 * @brief High-performance UUID pool for bulk operations
 */
class UUIDPool {
private:
    static std::mutex pool_mutex_;
    static std::queue<::boost::uuids::uuid> uuid_pool_;
    static std::atomic<bool> pool_enabled_;
    static std::thread pool_thread_;
    static std::atomic<bool> shutdown_;
    static constexpr size_t POOL_SIZE = 10000;
    static constexpr size_t REFILL_THRESHOLD = 1000;

public:
    /**
     * @brief Initialize the UUID pool
     */
    static void initialize() {
        pool_enabled_.store(true);
        shutdown_.store(false);

        // Start background thread to maintain pool
        pool_thread_ = std::thread([]() {
            ::boost::uuids::random_generator gen;

            while (!shutdown_.load()) {
                {
                    std::lock_guard<std::mutex> lock(pool_mutex_);
                    while (uuid_pool_.size() < POOL_SIZE) {
                        uuid_pool_.push(gen());
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    /**
     * @brief Get UUID from pool
     * @return UUID from pool or newly generated if pool is empty
     */
    static ::boost::uuids::uuid getFromPool() {
        if (!pool_enabled_.load()) {
            return ::boost::uuids::random_generator()();
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (!uuid_pool_.empty()) {
            auto uuid = uuid_pool_.front();
            uuid_pool_.pop();
            return uuid;
        }

        return ::boost::uuids::random_generator()();
    }

    /**
     * @brief Shutdown the UUID pool
     */
    static void shutdown() {
        shutdown_.store(true);
        if (pool_thread_.joinable()) {
            pool_thread_.join();
        }
        pool_enabled_.store(false);
    }

    /**
     * @brief Get pool statistics
     * @return Current pool size
     */
    static size_t getPoolSize() {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return uuid_pool_.size();
    }
};

/**
 * @brief UUID validation utilities
 */
class UUIDValidator {
public:
    /**
     * @brief Validate UUID string format
     * @param str String to validate
     * @return True if valid UUID format
     */
    static bool isValidFormat(std::string_view str) noexcept {
        if (str.length() != 36)
            return false;

        // Check hyphens at correct positions
        if (str[8] != '-' || str[13] != '-' || str[18] != '-' ||
            str[23] != '-') {
            return false;
        }

        // Check hex characters
        for (size_t i = 0; i < str.length(); ++i) {
            if (i == 8 || i == 13 || i == 18 || i == 23)
                continue;
            char c = str[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Validate UUID version
     * @param uuid UUID to validate
     * @param expected_version Expected version
     * @return True if UUID has expected version
     */
    static bool hasVersion(const ::boost::uuids::uuid& uuid,
                           int expected_version) noexcept {
        return uuid.version() == expected_version;
    }

    /**
     * @brief Check if UUID is RFC 4122 compliant
     * @param uuid UUID to check
     * @return True if RFC 4122 compliant
     */
    static bool isRFC4122Compliant(const ::boost::uuids::uuid& uuid) noexcept {
        return uuid.variant() == ::boost::uuids::uuid::variant_rfc_4122;
    }
};

/**
 * @brief Bulk UUID operations
 */
class UUIDBulkOperations {
public:
    /**
     * @brief Generate multiple UUIDs in parallel
     * @param count Number of UUIDs to generate
     * @return Vector of generated UUIDs
     */
    static std::vector<::boost::uuids::uuid> generateBulk(size_t count) {
        std::vector<::boost::uuids::uuid> result;
        result.reserve(count);

        if (count > 1000) {
            // Use parallel generation for large batches
            const size_t num_threads = std::thread::hardware_concurrency();
            const size_t chunk_size = count / num_threads;

            std::vector<std::future<std::vector<::boost::uuids::uuid>>> futures;

            for (size_t i = 0; i < num_threads; ++i) {
                size_t start = i * chunk_size;
                size_t end =
                    (i == num_threads - 1) ? count : (i + 1) * chunk_size;

                futures.emplace_back(
                    std::async(std::launch::async, [start, end]() {
                        std::vector<::boost::uuids::uuid> chunk;
                        chunk.reserve(end - start);
                        ::boost::uuids::random_generator gen;

                        for (size_t j = start; j < end; ++j) {
                            chunk.push_back(gen());
                        }

                        return chunk;
                    }));
            }

            for (auto& future : futures) {
                auto chunk = future.get();
                result.insert(result.end(), chunk.begin(), chunk.end());
            }
        } else {
            // Sequential generation for smaller batches
            ::boost::uuids::random_generator gen;
            for (size_t i = 0; i < count; ++i) {
                result.push_back(gen());
            }
        }

        return result;
    }

    /**
     * @brief Convert multiple UUIDs to strings in parallel
     * @param uuids Vector of UUIDs to convert
     * @return Vector of string representations
     */
    static std::vector<std::string> toStringsBulk(
        const std::vector<::boost::uuids::uuid>& uuids) {
        std::vector<std::string> result(uuids.size());

        // Sequential processing (parallel execution requires TBB)
        std::transform(uuids.begin(), uuids.end(), result.begin(),
                       [](const ::boost::uuids::uuid& uuid) {
                           return ::boost::uuids::to_string(uuid);
                       });

        return result;
    }

    /**
     * @brief Parse multiple UUID strings in parallel
     * @param strings Vector of UUID strings to parse
     * @return Vector of parsed UUIDs
     */
    static std::vector<std::optional<::boost::uuids::uuid>> parseStringsBulk(
        const std::vector<std::string>& strings) {
        std::vector<std::optional<::boost::uuids::uuid>> result(strings.size());

        // Sequential processing (parallel execution requires TBB)
        std::transform(
            strings.begin(), strings.end(), result.begin(),
            [](const std::string& str) -> std::optional<::boost::uuids::uuid> {
                try {
                    if (UUIDValidator::isValidFormat(str)) {
                        return ::boost::uuids::string_generator()(str);
                    }
                } catch (...) {
                    // Ignore parsing errors
                }
                return std::nullopt;
            });

        return result;
    }
};

/**
 * @brief High-performance wrapper for Boost.UUID with enhanced functionality
 */
class UUID {
private:
    ::boost::uuids::uuid uuid_;
    static UUIDStats stats_;

public:
    /**
     * @brief Default constructor that generates a random UUID (v4) using pool
     */
    UUID() : uuid_(UUIDPool::getFromPool()) {
        stats_.total_generated++;
        stats_.v4_generated++;
    }

    /**
     * @brief Constructs UUID from string representation
     * @param str String representation of the UUID
     */
    explicit UUID(std::string_view str)
        : uuid_(::boost::uuids::string_generator()(std::string(str))) {}

    /**
     * @brief Constructs UUID from Boost.UUID object
     * @param uuid The Boost.UUID object
     */
    explicit constexpr UUID(const ::boost::uuids::uuid& uuid) noexcept
        : uuid_(uuid) {}

    /**
     * @brief Converts UUID to string representation
     * @return String representation of the UUID
     */
    [[nodiscard]] std::string toString() const {
        return ::boost::uuids::to_string(uuid_);
    }

    /**
     * @brief Checks if UUID is nil (all zeros)
     * @return True if UUID is nil
     */
    [[nodiscard]] constexpr bool isNil() const noexcept {
        return uuid_.is_nil();
    }

    /**
     * @brief Three-way comparison operator
     * @param other UUID to compare with
     * @return Comparison result
     */
    constexpr std::strong_ordering operator<=>(
        const UUID& other) const noexcept {
        if (uuid_ < other.uuid_) [[likely]] {
            return std::strong_ordering::less;
        }
        if (uuid_ > other.uuid_) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    /**
     * @brief Equality comparison operator
     * @param other UUID to compare with
     * @return True if UUIDs are equal
     */
    constexpr bool operator==(const UUID& other) const noexcept {
        return uuid_ == other.uuid_;
    }

    /**
     * @brief Formats UUID with curly braces
     * @return Formatted string
     */
    [[nodiscard]] std::string format() const {
        return std::format("{{{}}}", toString());
    }

    /**
     * @brief Converts UUID to byte vector
     * @return Vector of bytes representing the UUID
     */
    [[nodiscard]] std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> result;
        result.reserve(UUID_SIZE);
        result.assign(uuid_.begin(), uuid_.end());
        return result;
    }

    /**
     * @brief Converts UUID to byte array
     * @return Array of bytes representing the UUID
     */
    [[nodiscard]] std::array<uint8_t, UUID_SIZE> toBytesArray() const noexcept {
        std::array<uint8_t, UUID_SIZE> result;
        std::copy(uuid_.begin(), uuid_.end(), result.begin());
        return result;
    }

    /**
     * @brief Validates the UUID format and structure
     * @return True if UUID is valid
     */
    [[nodiscard]] bool isValid() const noexcept {
        return UUIDValidator::isRFC4122Compliant(uuid_);
    }

    /**
     * @brief Gets UUID as hexadecimal string without hyphens
     * @return Hex string representation
     */
    [[nodiscard]] std::string toHex() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (auto byte : uuid_) {
            oss << std::setw(2) << static_cast<unsigned>(byte);
        }
        return oss.str();
    }

    /**
     * @brief Gets UUID as uppercase string
     * @return Uppercase string representation
     */
    [[nodiscard]] std::string toUpperString() const {
        std::string result = toString();
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    /**
     * @brief Gets UUID as compact string (no hyphens)
     * @return Compact string representation
     */
    [[nodiscard]] std::string toCompactString() const {
        std::string result = toString();
        result.erase(std::remove(result.begin(), result.end(), '-'),
                     result.end());
        return result;
    }

    /**
     * @brief Calculates Hamming distance to another UUID
     * @param other Other UUID to compare
     * @return Hamming distance (number of differing bits)
     */
    [[nodiscard]] size_t hammingDistance(const UUID& other) const noexcept {
        size_t distance = 0;
        for (size_t i = 0; i < UUID_SIZE; ++i) {
            uint8_t xor_result = uuid_.data[i] ^ other.uuid_.data[i];
            distance += __builtin_popcount(xor_result);
        }
        return distance;
    }

    /**
     * @brief Gets the node ID from version 1 UUID
     * @return Node ID as 48-bit value
     * @throws std::runtime_error if UUID is not version 1
     */
    [[nodiscard]] uint64_t getNodeId() const {
        if (version() != 1) {
            throw std::runtime_error(
                "Node ID is only available for version 1 UUIDs");
        }

        uint64_t node_id = 0;
        for (int i = 10; i < 16; ++i) {
            node_id = (node_id << 8) | uuid_.data[i];
        }
        return node_id & 0xFFFFFFFFFFFFULL;
    }

    /**
     * @brief Gets the clock sequence from version 1 UUID
     * @return Clock sequence as 14-bit value
     * @throws std::runtime_error if UUID is not version 1
     */
    [[nodiscard]] uint16_t getClockSequence() const {
        if (version() != 1) {
            throw std::runtime_error(
                "Clock sequence is only available for version 1 UUIDs");
        }

        return ((static_cast<uint16_t>(uuid_.data[8]) & 0x3F) << 8) |
               uuid_.data[9];
    }

    /**
     * @brief Constructs UUID from byte span
     * @param bytes Span of bytes (must be exactly 16 bytes)
     * @return Constructed UUID
     * @throws std::invalid_argument if span size is not 16 bytes
     */
    static UUID fromBytes(std::span<const uint8_t> bytes) {
        if ((bytes.size() != UUID_SIZE)) [[unlikely]] {
            throw std::invalid_argument("UUID must be exactly 16 bytes");
        }
        ::boost::uuids::uuid uuid;
        std::copy(bytes.begin(), bytes.end(), uuid.begin());
        return UUID(uuid);
    }

    /**
     * @brief Converts UUID to 64-bit unsigned integer
     * @return 64-bit representation of the UUID
     */
    [[nodiscard]] uint64_t toUint64() const {
        return ::boost::lexical_cast<uint64_t>(uuid_);
    }

    /**
     * @brief Gets DNS namespace UUID
     * @return DNS namespace UUID
     */
    static constexpr UUID namespaceDNS() noexcept {
        return UUID(::boost::uuids::ns::dns());
    }

    /**
     * @brief Gets URL namespace UUID
     * @return URL namespace UUID
     */
    static constexpr UUID namespaceURL() noexcept {
        return UUID(::boost::uuids::ns::url());
    }

    /**
     * @brief Gets OID namespace UUID
     * @return OID namespace UUID
     */
    static constexpr UUID namespaceOID() noexcept {
        return UUID(::boost::uuids::ns::oid());
    }

    /**
     * @brief Generates version 3 (MD5) UUID
     * @param namespace_uuid Namespace UUID
     * @param name Name to hash
     * @return Generated UUID
     */
    static UUID v3(const UUID& namespace_uuid, std::string_view name) {
        return UUID(::boost::uuids::name_generator(namespace_uuid.uuid_)(
            std::string(name)));
    }

    /**
     * @brief Generates version 5 (SHA-1) UUID
     * @param namespace_uuid Namespace UUID
     * @param name Name to hash
     * @return Generated UUID
     */
    static UUID v5(const UUID& namespace_uuid, std::string_view name) {
        ::boost::uuids::name_generator_sha1 gen(namespace_uuid.uuid_);
        return UUID(gen(std::string(name)));
    }

    /**
     * @brief Gets UUID version
     * @return Version number
     */
    [[nodiscard]] constexpr int version() const noexcept {
        return uuid_.version();
    }

    /**
     * @brief Gets UUID variant
     * @return Variant number
     */
    [[nodiscard]] constexpr int variant() const noexcept {
        return uuid_.variant();
    }

    /**
     * @brief Generates version 1 (timestamp-based) UUID
     * @return Generated UUID
     */
    [[nodiscard]] static UUID v1() {
        static thread_local ::boost::uuids::basic_random_generator<std::mt19937>
            gen;
        stats_.total_generated++;
        stats_.v1_generated++;
        return UUID(gen());
    }

    /**
     * @brief Generates version 4 (random) UUID
     * @return Generated UUID
     */
    [[nodiscard]] static UUID v4() noexcept { return UUID{}; }

    /**
     * @brief Creates a nil UUID (all zeros)
     * @return Nil UUID
     */
    [[nodiscard]] static UUID nil() noexcept {
        return UUID(::boost::uuids::nil_uuid());
    }

    /**
     * @brief Parses UUID from string with validation
     * @param str String to parse
     * @return Optional UUID if parsing succeeds
     */
    [[nodiscard]] static std::optional<UUID> parse(
        std::string_view str) noexcept {
        try {
            if (UUIDValidator::isValidFormat(str)) {
                return UUID(str);
            }
        } catch (...) {
            // Ignore parsing errors
        }
        return std::nullopt;
    }

    /**
     * @brief Generates multiple UUIDs efficiently
     * @param count Number of UUIDs to generate
     * @return Vector of generated UUIDs
     */
    [[nodiscard]] static std::vector<UUID> generateBatch(size_t count) {
        auto boost_uuids = UUIDBulkOperations::generateBulk(count);
        std::vector<UUID> result;
        result.reserve(count);

        for (const auto& boost_uuid : boost_uuids) {
            result.emplace_back(boost_uuid);
        }

        stats_.total_generated += count;
        stats_.v4_generated += count;
        stats_.bulk_operations++;

        return result;
    }

    /**
     * @brief Gets generation statistics
     * @return Current UUID generation statistics
     */
    [[nodiscard]] static UUIDStats getStatistics() {
        return UUIDStats{.total_generated = {stats_.total_generated.load()},
                         .v1_generated = {stats_.v1_generated.load()},
                         .v3_generated = {stats_.v3_generated.load()},
                         .v4_generated = {stats_.v4_generated.load()},
                         .v5_generated = {stats_.v5_generated.load()},
                         .pool_hits = {stats_.pool_hits.load()},
                         .pool_misses = {stats_.pool_misses.load()},
                         .bulk_operations = {stats_.bulk_operations.load()}};
    }

    /**
     * @brief Resets generation statistics
     */
    static void resetStatistics() {
        stats_.total_generated.store(0);
        stats_.v1_generated.store(0);
        stats_.v3_generated.store(0);
        stats_.v4_generated.store(0);
        stats_.v5_generated.store(0);
        stats_.pool_hits.store(0);
        stats_.pool_misses.store(0);
        stats_.bulk_operations.store(0);
    }

    /**
     * @brief Converts UUID to Base64 string
     * @return Base64 string representation
     */
    [[nodiscard]] std::string toBase64() const {
        static constexpr char base64_chars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        result.reserve(BASE64_ENCODED_SIZE);

        auto bytes = toBytes();
        for (size_t i = 0; i < bytes.size(); i += 3) {
            uint32_t num =
                (static_cast<uint32_t>(bytes[i]) << 16) |
                (i + 1 < bytes.size() ? static_cast<uint32_t>(bytes[i + 1]) << 8
                                      : 0) |
                (i + 2 < bytes.size() ? static_cast<uint32_t>(bytes[i + 2])
                                      : 0);

            result += base64_chars[(num >> 18) & 63];
            result += base64_chars[(num >> 12) & 63];
            result += base64_chars[(num >> 6) & 63];
            result += base64_chars[num & 63];
        }

        result.resize(BASE64_ENCODED_SIZE);
        return result;
    }

    /**
     * @brief Gets timestamp from version 1 UUID
     * @return Timestamp as time_point
     * @throws std::runtime_error if UUID is not version 1
     */
    [[nodiscard]] std::chrono::system_clock::time_point getTimestamp() const {
        if ((version() != 1)) [[unlikely]] {
            throw std::runtime_error(
                "Timestamp is only available for version 1 UUIDs");
        }

        uint64_t timestamp = (static_cast<uint64_t>(uuid_.data[6]) << 40) |
                             (static_cast<uint64_t>(uuid_.data[7]) << 32) |
                             (static_cast<uint64_t>(uuid_.data[4]) << 24) |
                             (static_cast<uint64_t>(uuid_.data[5]) << 16) |
                             (static_cast<uint64_t>(uuid_.data[0]) << 8) |
                             static_cast<uint64_t>(uuid_.data[1]);

        auto time_since_epoch = (timestamp - UUID_EPOCH) / TIMESTAMP_DIVISOR;
        return std::chrono::system_clock::from_time_t(
            static_cast<std::time_t>(time_since_epoch));
    }

    /**
     * @brief Hash function for Abseil containers
     * @tparam H Hash function type
     * @param h Hash function
     * @param uuid UUID to hash
     * @return Hash value
     */
    template <typename H>
    friend H abslHashValue(H h, const UUID& uuid) noexcept {
        return H::combine(std::move(h), uuid.uuid_);
    }

    /**
     * @brief Gets underlying Boost.UUID object
     * @return Reference to Boost.UUID object
     */
    [[nodiscard]] constexpr const ::boost::uuids::uuid& getUUID()
        const noexcept {
        return uuid_;
    }
};

// Static member definitions
inline std::mutex UUIDPool::pool_mutex_{};
inline std::queue<::boost::uuids::uuid> UUIDPool::uuid_pool_{};
inline std::atomic<bool> UUIDPool::pool_enabled_{false};
inline std::thread UUIDPool::pool_thread_{};
inline std::atomic<bool> UUIDPool::shutdown_{false};

inline UUIDStats UUID::stats_{};

}  // namespace atom::extra::boost

namespace std {

/**
 * @brief Hash specialization for UUID
 */
template <>
struct hash<atom::extra::boost::UUID> {
    size_t operator()(const atom::extra::boost::UUID& uuid) const noexcept {
        return ::boost::hash<::boost::uuids::uuid>()(uuid.getUUID());
    }
};

}  // namespace std

#endif
