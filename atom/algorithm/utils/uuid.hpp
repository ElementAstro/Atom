#ifndef ATOM_ALGORITHM_UTILS_UUID_HPP
#define ATOM_ALGORITHM_UTILS_UUID_HPP

#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

#include "../rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief UUID (Universally Unique Identifier) generator and utilities
 *
 * This class provides functionality to generate and manipulate UUIDs according
 * to RFC 4122. It supports multiple UUID versions:
 * - Version 1: Time-based UUID
 * - Version 4: Random UUID (most common)
 * - Version 5: Name-based UUID using SHA-1
 */
class UUID {
public:
    /**
     * @brief UUID data storage (128 bits)
     */
    using Data = std::array<u8, 16>;

    /**
     * @brief UUID version enumeration
     */
    enum class Version : u8 {
        TIME_BASED = 1,  ///< Time-based UUID
        RANDOM = 4,      ///< Random UUID
        NAME_SHA1 = 5    ///< Name-based UUID using SHA-1
    };

    /**
     * @brief Default constructor - creates a null UUID
     */
    UUID() : data_{} {}

    /**
     * @brief Construct UUID from raw data
     * @param data 16-byte array containing UUID data
     */
    explicit UUID(const Data& data) : data_(data) {}

    /**
     * @brief Construct UUID from string representation
     * @param uuid_str String in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     */
    explicit UUID(std::string_view uuid_str) {
        if (!fromString(uuid_str)) {
            data_.fill(0);
        }
    }

    /**
     * @brief Generate a random UUID (version 4)
     * @return New random UUID
     */
    [[nodiscard]] static auto generateRandom() -> UUID {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        static thread_local std::uniform_int_distribution<u64> dis;

        UUID uuid;

        // Generate 128 bits of random data
        u64 high = dis(gen);
        u64 low = dis(gen);

        std::memcpy(uuid.data_.data(), &high, 8);
        std::memcpy(uuid.data_.data() + 8, &low, 8);

        // Set version (4) and variant bits
        uuid.data_[6] = (uuid.data_[6] & 0x0F) | 0x40;  // Version 4
        uuid.data_[8] = (uuid.data_[8] & 0x3F) | 0x80;  // Variant 10

        return uuid;
    }

    /**
     * @brief Generate a time-based UUID (version 1)
     * @param node_id 6-byte node identifier (MAC address or random)
     * @return New time-based UUID
     */
    [[nodiscard]] static auto generateTimeBased(
        const std::array<u8, 6>& node_id) -> UUID {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<u16> clock_seq_dis(
            0, 0x3FFF);
        static thread_local u16 clock_seq = clock_seq_dis(gen);

        UUID uuid;

        // Get current time in 100-nanosecond intervals since UUID epoch
        // (1582-10-15)
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto nanos =
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
                .count();

        // UUID epoch is 1582-10-15 00:00:00 UTC
        // Difference from Unix epoch (1970-01-01) is 122192928000000000 * 100ns
        constexpr u64 UUID_EPOCH_OFFSET = 122192928000000000ULL;
        u64 timestamp = (nanos / 100) + UUID_EPOCH_OFFSET;

        // Time low (32 bits)
        uuid.data_[0] = static_cast<u8>(timestamp & 0xFF);
        uuid.data_[1] = static_cast<u8>((timestamp >> 8) & 0xFF);
        uuid.data_[2] = static_cast<u8>((timestamp >> 16) & 0xFF);
        uuid.data_[3] = static_cast<u8>((timestamp >> 24) & 0xFF);

        // Time mid (16 bits)
        uuid.data_[4] = static_cast<u8>((timestamp >> 32) & 0xFF);
        uuid.data_[5] = static_cast<u8>((timestamp >> 40) & 0xFF);

        // Time high and version (16 bits)
        // Version 1 goes in upper nibble of byte 6, time_hi_and_version uses 12
        // bits
        u16 time_hi = static_cast<u16>((timestamp >> 48) & 0x0FFF);
        uuid.data_[6] = static_cast<u8>(((time_hi >> 8) & 0x0F) |
                                        0x10);  // Version 1 in upper nibble
        uuid.data_[7] =
            static_cast<u8>(time_hi & 0xFF);  // Lower 8 bits of time_hi

        // Clock sequence and variant
        uuid.data_[8] = static_cast<u8>((clock_seq >> 8) | 0x80);  // Variant 10
        uuid.data_[9] = static_cast<u8>(clock_seq & 0xFF);

        // Node ID
        std::memcpy(uuid.data_.data() + 10, node_id.data(), 6);

        return uuid;
    }

    /**
     * @brief Generate a nil (all zeros) UUID
     * @return Nil UUID
     */
    [[nodiscard]] static auto generateNil() -> UUID { return UUID{}; }

    /**
     * @brief Convert UUID to string representation
     * @return String in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     */
    [[nodiscard]] auto toString() const -> std::string {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        for (usize i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                oss << '-';
            }
            oss << std::setw(2) << static_cast<u32>(data_[i]);
        }

        return oss.str();
    }

    /**
     * @brief Parse UUID from string representation
     * @param uuid_str String in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
     * @return true if parsing succeeded, false otherwise
     */
    [[nodiscard]] auto fromString(std::string_view uuid_str) -> bool {
        if (uuid_str.length() != 36) {
            data_.fill(0);  // Set to nil on failure
            return false;
        }

        // Check hyphen positions
        if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' ||
            uuid_str[23] != '-') {
            data_.fill(0);  // Set to nil on failure
            return false;
        }

        // Parse hex digits
        std::string hex_str;
        hex_str.reserve(32);

        for (char c : uuid_str) {
            if (c != '-') {
                if (!std::isxdigit(c)) {
                    data_.fill(0);  // Set to nil on failure
                    return false;
                }
                hex_str += c;
            }
        }

        // Must have exactly 32 hex digits
        if (hex_str.length() != 32) {
            data_.fill(0);  // Set to nil on failure
            return false;
        }

        // Convert hex string to bytes
        for (usize i = 0; i < 16; ++i) {
            std::string byte_str = hex_str.substr(i * 2, 2);
            data_[i] = static_cast<u8>(std::stoul(byte_str, nullptr, 16));
        }

        return true;
    }

    /**
     * @brief Get UUID version
     * @return UUID version
     */
    [[nodiscard]] auto getVersion() const -> Version {
        return static_cast<Version>((data_[6] & 0xF0) >> 4);
    }

    /**
     * @brief Check if UUID is nil (all zeros)
     * @return true if UUID is nil, false otherwise
     */
    [[nodiscard]] auto isNil() const -> bool {
        return std::all_of(data_.begin(), data_.end(),
                           [](u8 b) { return b == 0; });
    }

    /**
     * @brief Get raw UUID data
     * @return Reference to internal data array
     */
    [[nodiscard]] auto getData() const -> const Data& { return data_; }

    /**
     * @brief Equality comparison
     */
    [[nodiscard]] auto operator==(const UUID& other) const -> bool {
        return data_ == other.data_;
    }

    /**
     * @brief Inequality comparison
     */
    [[nodiscard]] auto operator!=(const UUID& other) const -> bool {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordering
     */
    [[nodiscard]] auto operator<(const UUID& other) const -> bool {
        return data_ < other.data_;
    }

    /**
     * @brief Generate a random node ID for time-based UUIDs
     * @return 6-byte random node ID
     */
    [[nodiscard]] static auto generateRandomNodeId() -> std::array<u8, 6> {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<u8> dis;

        std::array<u8, 6> node_id;
        for (auto& byte : node_id) {
            byte = dis(gen);
        }

        // Set multicast bit to indicate this is not a real MAC address
        node_id[0] |= 0x01;

        return node_id;
    }

private:
    Data data_;
};

/**
 * @brief Stream output operator for UUID
 */
inline auto operator<<(std::ostream& os, const UUID& uuid) -> std::ostream& {
    return os << uuid.toString();
}

}  // namespace atom::algorithm

// Hash specialization for std::unordered_map/set
namespace std {
template <>
struct hash<atom::algorithm::UUID> {
    auto operator()(const atom::algorithm::UUID& uuid) const noexcept
        -> size_t {
        const auto& data = uuid.getData();
        size_t h1 =
            hash<uint64_t>{}(*reinterpret_cast<const uint64_t*>(data.data()));
        size_t h2 = hash<uint64_t>{}(
            *reinterpret_cast<const uint64_t*>(data.data() + 8));
        return h1 ^ (h2 << 1);
    }
};
}  // namespace std

#endif  // ATOM_ALGORITHM_UTILS_UUID_HPP
