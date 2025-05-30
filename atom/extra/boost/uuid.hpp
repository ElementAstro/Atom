#ifndef ATOM_EXTRA_BOOST_UUID_HPP
#define ATOM_EXTRA_BOOST_UUID_HPP

#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <compare>
#include <format>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace atom::extra::boost {

constexpr size_t UUID_SIZE = 16;
constexpr size_t BASE64_ENCODED_SIZE = 22;
constexpr uint64_t TIMESTAMP_DIVISOR = 10000000;
constexpr uint64_t UUID_EPOCH = 0x01B21DD213814000L;

/**
 * @brief High-performance wrapper for Boost.UUID with enhanced functionality
 */
class UUID {
private:
    ::boost::uuids::uuid uuid_;

public:
    /**
     * @brief Default constructor that generates a random UUID (v4)
     */
    UUID() : uuid_(::boost::uuids::random_generator()()) {}

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
        return UUID(gen());
    }

    /**
     * @brief Generates version 4 (random) UUID
     * @return Generated UUID
     */
    [[nodiscard]] static UUID v4() noexcept { return UUID{}; }

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
