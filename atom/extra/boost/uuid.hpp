#ifndef ATOM_EXTRA_BOOST_UUID_HPP
#define ATOM_EXTRA_BOOST_UUID_HPP

#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#if __has_include(<boost/uuid/uuid_clock.hpp>) && \
    __has_include(<boost/uuid/time_generator_v1.hpp>)
#include <boost/uuid/time_generator_v1.hpp>
#include <boost/uuid/uuid_clock.hpp>
#define ATOM_EXTRA_BOOST_UUID_HAS_V1 1
#else
#define ATOM_EXTRA_BOOST_UUID_HAS_V1 0
#endif
#include <chrono>
#include <compare>
#include <format>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace atom::extra::boost {

constexpr size_t UUID_SIZE = 16;
constexpr size_t BASE64_RESERVE_SIZE = 22;
constexpr size_t BASE64_ENCODED_SIZE = BASE64_RESERVE_SIZE;
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
    [[nodiscard]] bool isNil() const noexcept { return uuid_.is_nil(); }

    /**
     * @brief Three-way comparison operator
     * @param other UUID to compare with
     * @return Comparison result
     */
    std::strong_ordering operator<=>(const UUID& other) const noexcept {
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
    bool operator==(const UUID& other) const noexcept {
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
    static UUID namespaceDNS() noexcept {
        return UUID(::boost::uuids::ns::dns());
    }

    /**
     * @brief Gets URL namespace UUID
     * @return URL namespace UUID
     */
    static UUID namespaceURL() noexcept {
        return UUID(::boost::uuids::ns::url());
    }

    /**
     * @brief Gets OID namespace UUID
     * @return OID namespace UUID
     */
    static UUID namespaceOID() noexcept {
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
    [[nodiscard]] int version() const noexcept { return uuid_.version(); }

    /**
     * @brief Gets UUID variant
     * @return Variant number
     */
    [[nodiscard]] int variant() const noexcept { return uuid_.variant(); }

    /**
     * @brief Generates version 1 (timestamp-based) UUID
     * @return Generated UUID
     */
    [[nodiscard]] static UUID v1() {
#if ATOM_EXTRA_BOOST_UUID_HAS_V1
        static thread_local ::boost::uuids::time_generator_v1 gen;
        return UUID(gen());
#else
        // Fallback: generate a random (v4) UUID when time-based UUID v1
        // generation is not available in the current Boost version.
        return UUID{};
#endif
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

        auto bytes = toBytes();
        std::string result;
        result.reserve(((bytes.size() + 2) / 3) * 4);

        std::size_t i = 0;
        const std::size_t n = bytes.size();

        // Full 3-byte blocks
        while (i + 3 <= n) {
            const uint32_t num = (static_cast<uint32_t>(bytes[i]) << 16) |
                                 (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                                 static_cast<uint32_t>(bytes[i + 2]);

            result.push_back(base64_chars[(num >> 18) & 0x3F]);
            result.push_back(base64_chars[(num >> 12) & 0x3F]);
            result.push_back(base64_chars[(num >> 6) & 0x3F]);
            result.push_back(base64_chars[num & 0x3F]);
            i += 3;
        }

        const std::size_t remain = n - i;
        if (remain == 1) {
            const uint32_t num = static_cast<uint32_t>(bytes[i]) << 16;
            result.push_back(base64_chars[(num >> 18) & 0x3F]);
            result.push_back(base64_chars[(num >> 12) & 0x3F]);
            result.push_back('=');
            result.push_back('=');
        } else if (remain == 2) {
            const uint32_t num = (static_cast<uint32_t>(bytes[i]) << 16) |
                                 (static_cast<uint32_t>(bytes[i + 1]) << 8);
            result.push_back(base64_chars[(num >> 18) & 0x3F]);
            result.push_back(base64_chars[(num >> 12) & 0x3F]);
            result.push_back(base64_chars[(num >> 6) & 0x3F]);
            result.push_back('=');
        }

        // Strip padding to obtain compact 22-character representation for
        // 16-byte UUIDs while keeping standard Base64 semantics.
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }

        return result;
    }

    /**
     * @brief Gets timestamp from version 1 UUID
     * @return Timestamp as time_point
     * @throws std::runtime_error if UUID is not version 1
     */
    [[nodiscard]] std::chrono::system_clock::time_point getTimestamp() const {
#if ATOM_EXTRA_BOOST_UUID_HAS_V1
        if ((version() != 1)) [[unlikely]] {
            throw std::runtime_error(
                "Timestamp is only available for version 1 UUIDs");
        }

        // Use Boost.Uuid's clock facilities to obtain a chrono-compatible
        // time_point corresponding to the v1 UUID timestamp.
        auto uuidTimePoint = uuid_.time_point_v1();
        return ::boost::uuids::uuid_clock::to_sys(uuidTimePoint);
#else
        throw std::runtime_error(
            "Timestamp is only available for version 1 UUIDs (time-based "
            "UUID generation not supported by this Boost version)");
#endif
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
