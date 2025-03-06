/*
 * uuid.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: UUID Generator

**************************************************/

#include "uuid.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <objbase.h>
// clang-format on
#elif defined(__linux__) || defined(__APPLE__)
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid/uuid.h>
#endif

// Standard endianness detection
#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BIGENDIAN
#endif
#elif defined(_WIN32)
#define LITTLEENDIAN
#elif defined(__APPLE__)
#include <machine/endian.h>
#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
#define BIGENDIAN
#endif
#else
#include <endian.h>
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
#define BIGENDIAN
#endif
#endif

#if defined(BIGENDIAN)
// Try to use compiler intrinsics
#if defined(__INTEL_COMPILER) || defined(__ICC)
#define betole16(x) _bswap16(x)
#define betole32(x) _bswap(x)
#define betole64(x) _bswap64(x)
#elif defined(__GNUC__) || defined(__clang__)  // GCC and CLANG
#define betole16(x) __builtin_bswap16(x)
#define betole32(x) __builtin_bswap32(x)
#define betole64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)  // MSVC
#include <stdlib.h>
#define betole16(x) _byteswap_ushort(x)
#define betole32(x) _byteswap_ulong(x)
#define betole64(x) _byteswap_uint64(x)
#else
// Use C++20 std::byteswap if available
#if __cpp_lib_byteswap >= 202110L
#define betole16(x) std::byteswap(x)
#define betole32(x) std::byteswap(x)
#define betole64(x) std::byteswap(x)
#else
#define FALLBACK_SWAP
#define betole16(x) swap_u16(x)
#define betole32(x) swap_u32(x)
#define betole64(x) swap_u64(x)
#endif
#endif

#if ATOM_USE_SIMD
#define betole128(x) swap_u128(x)
#define betole256(x) swap_u256(x)
#endif

#else  // Little endian
#define betole16(x) (x)
#define betole32(x) (x)
#define betole64(x) (x)
#if ATOM_USE_SIMD
#define betole128(x) (x)
#define betole256(x) (x)
#endif
#endif  // BIGENDIAN

#if ATOM_USE_SIMD
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>

#if defined(BIGENDIAN)
inline __m128i swap_u128(__m128i value) {
    const __m128i shuffle =
        _mm_set_epi64x(0x0001020304050607, 0x08090a0b0c0d0e0f);
    return _mm_shuffle_epi8(value, shuffle);
}

inline __m256i swap_u256(__m256i value) {
    const __m256i shuffle =
        _mm256_set_epi64x(0x0001020304050607, 0x08090a0b0c0d0e0f,
                          0x0001020304050607, 0x08090a0b0c0d0e0f);
    return _mm256_shuffle_epi8(value, shuffle);
}
#endif  // BIGENDIAN
#endif  // ATOM_USE_SIMD

#if defined(FALLBACK_SWAP) && !defined(__cpp_lib_byteswap)
#include <stdint.h>
inline uint16_t swap_u16(uint16_t value) {
    return ((value & 0xFF00u) >> 8u) | ((value & 0x00FFu) << 8u);
}
inline uint32_t swap_u32(uint32_t value) {
    return ((value & 0xFF000000u) >> 24u) | ((value & 0x00FF0000u) >> 8u) |
           ((value & 0x0000FF00u) << 8u) | ((value & 0x000000FFu) << 24u);
}
inline uint64_t swap_u64(uint64_t value) {
    return ((value & 0xFF00000000000000u) >> 56u) |
           ((value & 0x00FF000000000000u) >> 40u) |
           ((value & 0x0000FF0000000000u) >> 24u) |
           ((value & 0x000000FF00000000u) >> 8u) |
           ((value & 0x00000000FF000000u) << 8u) |
           ((value & 0x0000000000FF0000u) << 24u) |
           ((value & 0x000000000000FF00u) << 40u) |
           ((value & 0x00000000000000FFu) << 56u);
}
#endif  // FALLBACK_SWAP && !__cpp_lib_byteswap

namespace atom::utils {

#if ATOM_USE_SIMD
namespace detail {
/*
Converts a 128-bits unsigned int to an UUIDv4 string representation.
Uses SIMD via Intel's AVX2 instruction set.
*/
void inline m128itos(__m128i x, char* mem) {
    if (!mem) {
        throw std::invalid_argument("Output buffer cannot be null");
    }

    try {
        // Expand each byte in x to two bytes in res
        // i.e. 0x12345678 -> 0x0102030405060708
        // Then translate each byte to its hex ascii representation
        // i.e. 0x0102030405060708 -> 0x3132333435363738
        const __m256i MASK = _mm256_set1_epi8(0x0F);
        const __m256i ADD = _mm256_set1_epi8(0x06);
        const __m256i ALPHA_MASK = _mm256_set1_epi8(0x10);
        const __m256i ALPHA_OFFSET = _mm256_set1_epi8(0x57);

        __m256i a = _mm256_castsi128_si256(x);
        __m256i as = _mm256_srli_epi64(a, 4);
        __m256i lo = _mm256_unpacklo_epi8(as, a);
        __m128i hi = _mm256_castsi256_si128(_mm256_unpackhi_epi8(as, a));
        __m256i c = _mm256_inserti128_si256(lo, hi, 1);
        __m256i d = _mm256_and_si256(c, MASK);
        __m256i alpha = _mm256_slli_epi64(
            _mm256_and_si256(_mm256_add_epi8(d, ADD), ALPHA_MASK), 3);
        __m256i offset =
            _mm256_blendv_epi8(_mm256_slli_epi64(ADD, 3), ALPHA_OFFSET, alpha);
        __m256i res = _mm256_add_epi8(d, offset);

        // Add dashes between blocks as specified in RFC-4122
        // 8-4-4-4-12
        const __m256i DASH_SHUFFLE =
            _mm256_set_epi32(0x0b0a0908, 0x07060504, 0x80030201, 0x00808080,
                             0x0d0c800b, 0x0a090880, 0x07060504, 0x03020100);
        const __m256i DASH =
            _mm256_set_epi64x(0x0000000000000000ull, 0x2d000000002d0000ull,
                              0x00002d000000002d, 0x0000000000000000ull);

        __m256i resd = _mm256_shuffle_epi8(res, DASH_SHUFFLE);
        resd = _mm256_or_si256(resd, DASH);

        _mm256_storeu_si256((__m256i*)mem, betole256(resd));
        *(uint16_t*)(mem + 16) = betole16(_mm256_extract_epi16(res, 7));
        *(uint32_t*)(mem + 32) = betole32(_mm256_extract_epi32(res, 7));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("SIMD UUID conversion failed: ") +
                                 e.what());
    }
}

/*
  Converts an UUIDv4 string representation to a 128-bits unsigned int.
  Uses SIMD via Intel's AVX2 instruction set.
*/
__m128i inline stom128i(const char* mem) {
    if (!mem) {
        throw std::invalid_argument("Input buffer cannot be null");
    }

    try {
        // Validate input string has correct length
        for (int i = 0; i < 36; i++) {
            if ((i == 8 || i == 13 || i == 18 || i == 23) && mem[i] != '-') {
                throw std::invalid_argument(
                    "Invalid UUID format: missing dash at position " +
                    std::to_string(i));
            } else if ((i != 8 && i != 13 && i != 18 && i != 23) &&
                       !std::isxdigit(mem[i])) {
                throw std::invalid_argument(
                    "Invalid UUID format: non-hexadecimal character at "
                    "position " +
                    std::to_string(i));
            }
        }

        // Remove dashes and pack hex ascii bytes in a 256-bits int
        const __m256i DASH_SHUFFLE =
            _mm256_set_epi32(0x80808080, 0x0f0e0d0c, 0x0b0a0908, 0x06050403,
                             0x80800f0e, 0x0c0b0a09, 0x07060504, 0x03020100);

        __m256i x = betole256(_mm256_loadu_si256((__m256i*)mem));
        x = _mm256_shuffle_epi8(x, DASH_SHUFFLE);
        x = _mm256_insert_epi16(x, betole16(*(uint16_t*)(mem + 16)), 7);
        x = _mm256_insert_epi32(x, betole32(*(uint32_t*)(mem + 32)), 7);

        // Build a mask to apply a different offset to alphas and digits
        const __m256i SUB = _mm256_set1_epi8(0x2F);
        const __m256i MASK = _mm256_set1_epi8(0x20);
        const __m256i ALPHA_OFFSET = _mm256_set1_epi8(0x28);
        const __m256i DIGITS_OFFSET = _mm256_set1_epi8(0x01);
        const __m256i UNWEAVE =
            _mm256_set_epi32(0x0f0d0b09, 0x0e0c0a08, 0x07050301, 0x06040200,
                             0x0f0d0b09, 0x0e0c0a08, 0x07050301, 0x06040200);
        const __m256i SHIFT =
            _mm256_set_epi32(0x00000000, 0x00000004, 0x00000000, 0x00000004,
                             0x00000000, 0x00000004, 0x00000000, 0x00000004);

        // Translate ascii bytes to their value
        // i.e. 0x3132333435363738 -> 0x0102030405060708
        // Shift hi-digits
        // i.e. 0x0102030405060708 -> 0x1002300450067008
        // Horizontal add
        // i.e. 0x1002300450067008 -> 0x12345678
        __m256i a = _mm256_sub_epi8(x, SUB);
        __m256i alpha = _mm256_slli_epi64(_mm256_and_si256(a, MASK), 2);
        __m256i subMask =
            _mm256_blendv_epi8(DIGITS_OFFSET, ALPHA_OFFSET, alpha);
        a = _mm256_sub_epi8(a, subMask);
        a = _mm256_shuffle_epi8(a, UNWEAVE);
        a = _mm256_sllv_epi32(a, SHIFT);
        a = _mm256_hadd_epi32(a, _mm256_setzero_si256());
        a = _mm256_permute4x64_epi64(a, 0b00001000);

        return _mm256_castsi256_si128(a);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("SIMD UUID conversion failed: ") +
                                 e.what());
    }
}

}  // namespace detail
#endif

UUID::UUID() {
    try {
        generateRandom();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to generate random UUID: ") + e.what());
    }
}

UUID::UUID(const std::array<uint8_t, 16>& data) : data_(data) {}

UUID::UUID(std::span<const uint8_t> bytes) {
    if (bytes.size() != 16) {
        throw std::invalid_argument("UUID must be exactly 16 bytes");
    }
    std::copy(bytes.begin(), bytes.end(), data_.begin());
}

auto UUID::toString() const -> std::string {
    try {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < data_.size(); ++i) {
            oss << std::setw(2) << static_cast<int>(data_[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                oss << '-';
            }
        }
        return oss.str();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to convert UUID to string: ") + e.what());
    }
}

auto UUID::fromString(std::string_view str) -> type::expected<UUID, UuidError> {
    // Check if the string is a valid UUID format
    if (!isValidUUID(str)) {
        return type::unexpected(UuidError::InvalidFormat);
    }

    try {
        UUID uuid;
        size_t pos = 0;
        size_t count = 0;

        for (auto& byte : uuid.data_) {
            while (pos < str.size() && str[pos] == '-') {
                ++pos;
            }

            if (pos + 2 > str.size()) {
                return type::unexpected(UuidError::InvalidLength);
            }

            // Extract two hex chars and convert to byte
            std::string_view hexPair = str.substr(pos, 2);
            try {
                byte = static_cast<uint8_t>(
                    std::stoi(std::string(hexPair), nullptr, 16));
            } catch (...) {
                return type::unexpected(UuidError::ConversionFailed);
            }

            pos += 2;
            count++;
        }

        if (count != 16) {
            return type::unexpected(UuidError::InvalidLength);
        }

        return uuid;
    } catch (...) {
        return type::unexpected(UuidError::InternalError);
    }
}

auto UUID::isValidUUID(std::string_view str) noexcept -> bool {
    // Check basic length
    if (str.size() != 36 && str.size() != 32) {
        return false;
    }

    // Simple format validation with regular expression
    static const std::regex uuidPattern1(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-"
        "F]{12}$");
    static const std::regex uuidPattern2("^[0-9a-fA-F]{32}$");

    try {
        std::string s(str);
        return std::regex_match(s, uuidPattern1) ||
               std::regex_match(s, uuidPattern2);
    } catch (...) {
        // If regex throws (shouldn't happen), fail safely
        return false;
    }
}

auto UUID::operator==(const UUID& other) const -> bool {
    return data_ == other.data_;
}

auto UUID::operator!=(const UUID& other) const -> bool {
    return !(*this == other);
}

auto UUID::operator<(const UUID& other) const -> bool {
    return data_ < other.data_;
}

auto operator<<(std::ostream& os, const UUID& uuid) -> std::ostream& {
    return os << uuid.toString();
}

auto operator>>(std::istream& is, UUID& uuid) -> std::istream& {
    std::string str;
    is >> str;

    auto result = UUID::fromString(str);
    if (!result) {
        is.setstate(std::ios_base::failbit);
        return is;
    }

    uuid = result.value();
    return is;
}

auto UUID::getData() const noexcept -> const std::array<uint8_t, 16>& {
    return data_;
}

auto UUID::version() const noexcept -> uint8_t {
    return (data_[6] & 0xF0) >> 4;
}

auto UUID::variant() const noexcept -> uint8_t {
    return (data_[8] & 0xC0) >> 6;
}

auto UUID::generateV3(const UUID& namespace_uuid,
                      std::string_view name) -> UUID {
    return generateNameBased<EVP_md5>(namespace_uuid, name, 3);
}

auto UUID::generateV4() -> UUID {
    try {
        // Generate a random UUID (version 4) using thread-safe random number
        // generation
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);

        std::array<uint8_t, 16> uuid_data;

        // Use ranges/views for filling the array (C++20)
        std::ranges::generate(uuid_data, [&dist]() { return dist(gen); });

        // Set version to 4 (randomly generated UUID)
        uuid_data[6] = (uuid_data[6] & 0x0F) | 0x40;

        // Set the variant (RFC 4122 variant)
        uuid_data[8] = (uuid_data[8] & 0x3F) | 0x80;

        return UUID(uuid_data);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to generate UUID v4: ") +
                                 e.what());
    }
}

auto UUID::generateV5(const UUID& namespace_uuid,
                      std::string_view name) -> UUID {
    return generateNameBased<EVP_sha1>(namespace_uuid, name, 5);
}

auto UUID::generateV1() -> UUID {
    try {
        UUID uuid;

        // Get high precision timestamp
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();

        // Get nanoseconds for higher precision
        auto nanosecs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
                .count();

        // RFC 4122 time is in 100ns intervals
        auto timestamp =
            static_cast<uint64_t>(nanosecs / 100) + 0x01B21DD213814000;

        // Create a thread-local generator for better performance
        thread_local std::random_device rd;
        thread_local std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint16_t> dist(0, 0x3FFF);

        uint16_t clockSeq = dist(gen);  // 14-bit clock sequence
        uint64_t node = generateNode();

        // Time low, mid, high parts
        uuid.data_[0] = static_cast<uint8_t>(timestamp >> 24);
        uuid.data_[1] = static_cast<uint8_t>(timestamp >> 16);
        uuid.data_[2] = static_cast<uint8_t>(timestamp >> 8);
        uuid.data_[3] = static_cast<uint8_t>(timestamp);

        uuid.data_[4] = static_cast<uint8_t>(timestamp >> 40);
        uuid.data_[5] = static_cast<uint8_t>(timestamp >> 32);

        uuid.data_[6] = static_cast<uint8_t>(timestamp >> 56);
        uuid.data_[7] = static_cast<uint8_t>(timestamp >> 48);

        // Clock sequence
        uuid.data_[8] = static_cast<uint8_t>(clockSeq >> 8);
        uuid.data_[9] = static_cast<uint8_t>(clockSeq);

        // Node
        for (int i = 0; i < 6; i++) {
            uuid.data_[10 + i] =
                static_cast<uint8_t>((node >> (8 * (5 - i))) & 0xFF);
        }

        uuid.data_[6] = (uuid.data_[6] & 0x0F) | 0x10;  // Version 1
        uuid.data_[8] = (uuid.data_[8] & 0x3F) | 0x80;  // Variant

        return uuid;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to generate UUID v1: ") +
                                 e.what());
    }
}

void UUID::generateRandom() {
    try {
        // Create a hardware-based random generator if available
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(
            0, std::numeric_limits<uint32_t>::max());

        // Fill 4 bytes at a time for better performance
        for (size_t i = 0; i < data_.size(); i += 4) {
            uint32_t rand = dist(gen);
            size_t remaining = std::min<size_t>(4, data_.size() - i);
            for (size_t j = 0; j < remaining; ++j) {
                data_[i + j] = static_cast<uint8_t>((rand >> (j * 8)) & 0xFF);
            }
        }

        data_[6] = (data_[6] & 0x0F) | 0x40;  // Version 4
        data_[8] = (data_[8] & 0x3F) | 0x80;  // Variant
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to generate random UUID data: ") + e.what());
    }
}

auto UUID::generateNode() -> uint64_t {
    try {
        // First try to use MAC address
        std::string mac = getMAC();
        if (!mac.empty() && mac.length() >= 12) {
            uint64_t node = 0;
            for (size_t i = 0; i < std::min<size_t>(12, mac.length()); i += 2) {
                try {
                    node = (node << 8) | static_cast<uint64_t>(std::stoi(
                                             mac.substr(i, 2), nullptr, 16));
                } catch (...) {
                    // If conversion fails, fall back to random
                    goto useRandom;
                }
            }
            return node;
        }

    useRandom:
        // Fall back to pseudo-random number with multicast bit set
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFF);
        return dist(gen) | 0x010000000000;  // Multicast bit set to 1
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to generate node ID: ") +
                                 e.what());
    }
}

auto getMAC() -> std::string {
    std::string mac;

#if defined(_WIN32)
    try {
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD bufferSize = sizeof(adapterInfo);

        if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
            PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (UINT i = 0; i < pAdapterInfo->AddressLength; i++) {
                oss << std::setw(2)
                    << static_cast<int>(pAdapterInfo->Address[i]);
            }
            mac = oss.str();
        }
    } catch (...) {
        // Silently handle exceptions and return empty string
    }

#elif defined(__linux__) || defined(__APPLE__)
    try {
        struct ifreq ifr;
        struct ifconf ifc;
        char buf[1024];
        bool success = false;

        // Use RAII for socket management
        auto sock = [&]() -> int {
            int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (fd == -1) {
                throw std::runtime_error("Failed to create socket");
            }
            return fd;
        }();

        // Ensure socket is closed on exit
        struct SocketCloser {
            int fd;
            ~SocketCloser() {
                if (fd >= 0)
                    close(fd);
            }
        } socketCloser{sock};

        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
            return mac;
        }

        struct ifreq* it = ifc.ifc_req;
        const struct ifreq* const end =
            it + (ifc.ifc_len / sizeof(struct ifreq));

        for (; it != end; ++it) {
            std::strncpy(ifr.ifr_name, it->ifr_name, IFNAMSIZ - 1);
            ifr.ifr_name[IFNAMSIZ - 1] = 0;

            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
                if (!(ifr.ifr_flags & IFF_LOOPBACK)) {
                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                        success = true;
                        break;
                    }
                }
            }
        }

        if (success) {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (int i = 0; i < 6; i++) {
                oss << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(
                           ifr.ifr_hwaddr.sa_data[i]));
            }
            mac = oss.str();
        }
    } catch (...) {
        // Silently handle exceptions and return empty string
    }
#endif

    return mac;
}

auto getCPUSerial() -> std::string {
    std::string cpuSerial;

    try {
#if defined(_WIN32)
        int cpuInfo[4] = {0};
        __cpuid(cpuInfo, 1);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i : cpuInfo) {
            oss << std::setw(8) << i;
        }
        cpuSerial = oss.str();

#elif defined(__linux__)
        std::ifstream file("/proc/cpuinfo");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("Serial") != std::string::npos ||
                    line.find("processor") != std::string::npos ||
                    line.find("cpu family") != std::string::npos) {
                    std::istringstream iss(line);
                    std::string key;
                    std::string value;
                    if (std::getline(iss, key, ':') &&
                        std::getline(iss, value)) {
                        // Use only numeric and hex characters
                        value.erase(
                            std::remove_if(value.begin(), value.end(),
                                           [](char c) {
                                               return !std::isxdigit(c) &&
                                                      !std::isdigit(c);
                                           }),
                            value.end());
                        cpuSerial += value;
                    }
                }
            }
            file.close();
        }

#elif defined(__APPLE__)
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(
            popen("sysctl -n machdep.cpu.brand_string", "r"), pclose);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            // Clean up non-hex, non-digit characters
            result.erase(std::remove_if(result.begin(), result.end(),
                                        [](char c) {
                                            return !std::isxdigit(c) &&
                                                   !std::isdigit(c);
                                        }),
                         result.end());
            cpuSerial = result;
        }
#endif
    } catch (const std::exception& e) {
        // Log error but return empty string to avoid breaking UUID generation
        // std::cerr << "Failed to get CPU serial: " << e.what() << std::endl;
    }

    return cpuSerial;
}

auto formatUUID(std::string_view uuid) -> std::string {
    if (uuid.empty()) {
        return {};
    }

    // Remove any existing dashes
    std::string cleaned;
    cleaned.reserve(uuid.size());

    std::copy_if(uuid.begin(), uuid.end(), std::back_inserter(cleaned),
                 [](char c) { return c != '-'; });

    // Check if we have enough characters for a valid UUID
    if (cleaned.size() < 32) {
        return {};  // Not enough characters for a valid UUID
    }

    std::string formatted;
    formatted.reserve(36);  // 32 hex chars + 4 dashes

    // Use ranges to transform and add dashes at specific positions
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            formatted.push_back('-');
        } else {
            size_t pos =
                i -
                (i > 23 ? 4 : (i > 18 ? 3 : (i > 13 ? 2 : (i > 8 ? 1 : 0))));
            if (pos < cleaned.size()) {
                formatted.push_back(cleaned[pos]);
            }
        }
    }

    return formatted;
}

auto generateUniqueUUID() -> std::string {
    try {
        // Use a combination of system unique identifiers
        auto mac = getMAC();
        auto cpuSerial = getCPUSerial();
        auto timestamp =
            std::chrono::system_clock::now().time_since_epoch().count();
        auto pid = static_cast<uint64_t>(getpid());

        // Use a cryptographic hash to mix the inputs
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        if (!mdctx) {
            throw std::runtime_error("Failed to create message digest context");
        }

        // Ensure cleanup
        auto mdctx_guard =
            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
                mdctx, &EVP_MD_CTX_free);

        if (EVP_DigestInit_ex(mdctx, EVP_sha1(), nullptr) != 1) {
            throw std::runtime_error("Failed to initialize digest");
        }

        // Update with all available system identifiers
        EVP_DigestUpdate(mdctx, mac.data(), mac.size());
        EVP_DigestUpdate(mdctx, cpuSerial.data(), cpuSerial.size());
        EVP_DigestUpdate(mdctx, &timestamp, sizeof(timestamp));
        EVP_DigestUpdate(mdctx, &pid, sizeof(pid));

        // Add some random data for additional entropy
        std::array<unsigned char, 16> random_bytes;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        for (auto& byte : random_bytes) {
            byte = static_cast<unsigned char>(dis(gen));
        }
        EVP_DigestUpdate(mdctx, random_bytes.data(), random_bytes.size());

        // Finalize the hash
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
            throw std::runtime_error("Failed to finalize digest");
        }

        // Format as UUID
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < std::min(hash_len, 16u); i++) {
            oss << std::setw(2) << static_cast<int>(hash[i]);
        }

        std::string uuid_str = oss.str();

        // Set version (4) and variant bits for UUID format compliance
        uuid_str[12] = '4';  // Version 4

        // Set variant bits (10xx) - variant 1
        char variantChar = uuid_str[16];
        int variant = (std::isdigit(variantChar)
                           ? variantChar - '0'
                           : (std::tolower(variantChar) - 'a' + 10)) &
                      0x3;
        variant |= 0x8;  // Set the high bit (10xx pattern)
        uuid_str[16] = (variant < 10) ? ('0' + variant) : ('a' + variant - 10);

        return formatUUID(uuid_str);
    } catch (const std::exception& e) {
        // If the complex method fails, fall back to a simple method
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint8_t> dis(0, 255);

        std::array<uint8_t, 16> data;
        for (auto& byte : data) {
            byte = dis(gen);
        }

        // Set version to 4
        data[6] = (data[6] & 0x0F) | 0x40;
        // Set variant to RFC 4122
        data[8] = (data[8] & 0x3F) | 0x80;

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < data.size(); ++i) {
            oss << std::setw(2) << static_cast<int>(data[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) {
                oss << '-';
            }
        }
        return oss.str();
    }
}

#if ATOM_USE_SIMD
FastUUID::FastUUID() {}

FastUUID::FastUUID(const FastUUID& other) {
    __m128i x = _mm_load_si128((__m128i*)other.data);
    _mm_store_si128((__m128i*)data, x);
}

FastUUID::FastUUID(__m128i x) { _mm_store_si128((__m128i*)data, x); }

FastUUID::FastUUID(uint64_t x, uint64_t y) {
    __m128i z = _mm_set_epi64x(x, y);
    _mm_store_si128((__m128i*)data, z);
}

FastUUID::FastUUID(const uint8_t* bytes) {
    __m128i x = _mm_loadu_si128((__m128i*)bytes);
    _mm_store_si128((__m128i*)data, x);
}

FastUUID::FastUUID(const std::string& bytes) {
    __m128i x = betole128(_mm_loadu_si128((__m128i*)bytes.data()));
    _mm_store_si128((__m128i*)data, x);
}

void FastUUID::fromStr(const char* raw) {
    _mm_store_si128((__m128i*)data, detail::stom128i(raw));
}

FastUUID& FastUUID::operator=(const FastUUID& other) {
    if (&other == this) {
        return *this;
    }
    __m128i x = _mm_load_si128((__m128i*)other.data);
    _mm_store_si128((__m128i*)data, x);
    return *this;
}

bool operator==(const FastUUID& lhs, const FastUUID& rhs) {
    __m128i x = _mm_load_si128((__m128i*)lhs.data);
    __m128i y = _mm_load_si128((__m128i*)rhs.data);

    __m128i neq = _mm_xor_si128(x, y);
    return _mm_test_all_zeros(neq, neq);
}

bool operator<(const FastUUID& lhs, const FastUUID& rhs) {
    uint64_t* x = (uint64_t*)lhs.data;
    uint64_t* y = (uint64_t*)rhs.data;
    return *x < *y || (*x == *y && *(x + 1) < *(y + 1));
}

bool operator!=(const FastUUID& lhs, const FastUUID& rhs) {
    return !(lhs == rhs);
}
bool operator>(const FastUUID& lhs, const FastUUID& rhs) { return rhs < lhs; }
bool operator<=(const FastUUID& lhs, const FastUUID& rhs) {
    return !(lhs > rhs);
}
bool operator>=(const FastUUID& lhs, const FastUUID& rhs) {
    return !(lhs < rhs);
}

std::string FastUUID::bytes() const {
    std::string mem;
    bytes(mem);
    return mem;
}

void FastUUID::bytes(std::string& out) const {
    out.resize(sizeof(data));
    bytes((char*)out.data());
}

void FastUUID::bytes(char* bytes) const {
    __m128i x = betole128(_mm_load_si128((__m128i*)data));
    _mm_storeu_si128((__m128i*)bytes, x);
}

std::string FastUUID::str() const {
    std::string mem;
    str(mem);
    return mem;
}

void FastUUID::str(std::string& s) const {
    s.resize(36);
    str((char*)s.data());
}

void FastUUID::str(char* res) const {
    __m128i x = _mm_load_si128((__m128i*)data);
    detail::m128itos(x, res);
}

std::ostream& operator<<(std::ostream& stream, const FastUUID& uuid) {
    return stream << uuid.str();
}

std::istream& operator>>(std::istream& stream, FastUUID& uuid) {
    std::string s;
    stream >> s;
    uuid = FastUUID(s);
    return stream;
}

size_t FastUUID::hash() const {
    const uint64_t a = *((uint64_t*)data);
    const uint64_t b = *((uint64_t*)&data[8]);
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

template <typename RNG>
FastUUIDGenerator<RNG>::FastUUIDGenerator()
    : generator(new RNG(std::random_device()())),
      distribution(std::numeric_limits<uint64_t>::min(),
                   std::numeric_limits<uint64_t>::max()) {}

template <typename RNG>
FastUUIDGenerator<RNG>::FastUUIDGenerator(uint64_t seed)
    : generator(new RNG(seed)),
      distribution(std::numeric_limits<uint64_t>::min(),
                   std::numeric_limits<uint64_t>::max()) {}

template <typename RNG>
FastUUIDGenerator<RNG>::FastUUIDGenerator(RNG& gen)
    : generator(gen),
      distribution(std::numeric_limits<uint64_t>::min(),
                   std::numeric_limits<uint64_t>::max()) {}

template <typename RNG>
FastUUID FastUUIDGenerator<RNG>::getUUID() {
    const __m128i AND_MASK =
        _mm_set_epi64x(0xFFFFFFFFFFFFFF3Full, 0xFF0FFFFFFFFFFFFFull);
    const __m128i OR_MASK =
        _mm_set_epi64x(0x0000000000000080ull, 0x0040000000000000ull);
    __m128i n =
        _mm_set_epi64x(distribution(*generator), distribution(*generator));
    __m128i uuid = _mm_or_si128(_mm_and_si128(n, AND_MASK), OR_MASK);

    return {uuid};
}

// Explicit template instantiation to avoid linker errors
template class FastUUIDGenerator<std::mt19937>;
template class FastUUIDGenerator<std::mt19937_64>;
template class FastUUIDGenerator<std::ranlux48>;
template class FastUUIDGenerator<std::knuth_b>;

// Add efficient batch UUID generation for parallel workloads
std::vector<FastUUID> generateUUIDBatch(size_t count) {
    static thread_local FastUUIDGenerator<std::mt19937_64> generator;

    std::vector<FastUUID> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        result.push_back(generator.getUUID());
    }

    return result;
}

// Parallel batch generation using C++20 concurrency features
std::vector<FastUUID> generateUUIDBatchParallel(size_t count) {
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const size_t num_threads = std::max(1u, hardware_threads);
    const size_t batch_size = count / num_threads + (count % num_threads != 0);

    std::vector<FastUUID> result(count);
    std::vector<std::future<void>> futures;

    for (size_t t = 0; t < num_threads; ++t) {
        size_t start_idx = t * batch_size;
        size_t end_idx = std::min(start_idx + batch_size, count);

        if (start_idx >= count)
            break;

        futures.push_back(
            std::async(std::launch::async, [&result, start_idx, end_idx]() {
                FastUUIDGenerator<std::mt19937_64> gen;
                for (size_t i = start_idx; i < end_idx; ++i) {
                    result[i] = gen.getUUID();
                }
            }));
    }

    // Wait for all threads to complete
    for (auto& f : futures) {
        f.wait();
    }

    return result;
}
#endif
}  // namespace atom::utils
