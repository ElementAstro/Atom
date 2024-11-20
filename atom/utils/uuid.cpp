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
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>

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

#if defined(__GLIBC__) || defined(__GNU_LIBRARY__) || defined(__ANDROID__)
#include <endian.h>
#elif defined(__APPLE__) && defined(__MACH__)
#include <machine/endian.h>
#elif defined(BSD) || defined(_SYSTYPE_BSD)
#if defined(__OpenBSD__)
#include <machine/endian.h>
#else
#include <sys/endian.h>
#endif
#endif

#if defined(__BYTE_ORDER)
#if defined(__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#define BIGENDIAN
#elif defined(__LITTLE_ENDIAN) && (__BYTE_ORDER == __LITTLE_ENDIAN)
#define LITTLEENDIAN
#endif
#elif defined(_BYTE_ORDER)
#if defined(_BIG_ENDIAN) && (_BYTE_ORDER == _BIG_ENDIAN)
#define BIGENDIAN
#elif defined(_LITTLE_ENDIAN) && (_BYTE_ORDER == _LITTLE_ENDIAN)
#define LITTLEENDIAN
#endif
#elif defined(__BIG_ENDIAN__)
#define BIGENDIAN
#elif defined(__LITTLE_ENDIAN__)
#define LITTLEENDIAN
#else
#if defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) ||         \
    defined(__ia64__) || defined(_IA64) || defined(__IA64__) ||             \
    defined(__ia64) || defined(_M_IA64) || defined(__itanium__) ||          \
    defined(i386) || defined(__i386__) || defined(__i486__) ||              \
    defined(__i586__) || defined(__i686__) || defined(__i386) ||            \
    defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL__) ||         \
    defined(__I86__) || defined(__INTEL__) || defined(__x86_64) ||          \
    defined(__x86_64__) || defined(__amd64__) || defined(__amd64) ||        \
    defined(_M_X64) || defined(__bfin__) || defined(__BFIN__) ||            \
    defined(bfin) || defined(BFIN)

#define LITTLEENDIAN
#elif defined(__m68k__) || defined(M68000) || defined(__hppa__) ||  \
    defined(__hppa) || defined(__HPPA__) || defined(__sparc__) ||   \
    defined(__sparc) || defined(__370__) || defined(__THW_370__) || \
    defined(__s390__) || defined(__s390x__) || defined(__SYSC_ZARCH__)

#define BIGENDIAN

#elif defined(__arm__) || defined(__arm64) || defined(__thumb__) || \
    defined(__TARGET_ARCH_ARM) || defined(__TARGET_ARCH_THUMB) ||   \
    defined(__ARM_ARCH) || defined(_M_ARM) || defined(_M_ARM64)

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || \
    defined(__TOS_WIN__) || defined(__WINDOWS__)

#define LITTLEENDIAN

#else
#error "Cannot determine system endianness."
#endif
#endif
#endif

#if defined(BIGENDIAN)
// Try to use compiler intrinsics
#if defined(__INTEL_COMPILER) || defined(__ICC)
#define betole16(x) _bswap16(x)
#define betole32(x) _bswap(x)
#define betole64(x) _bswap64(x)
#elif defined(__GNUC__)  // GCC and CLANG
#define betole16(x) __builtin_bswap16(x)
#define betole32(x) __builtin_bswap32(x)
#define betole64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)  // MSVC
#include <stdlib.h>
#define betole16(x) _byteswap_ushort(x)
#define betole32(x) _byteswap_ulong(x)
#define betole64(x) _byteswap_uint64(x)
#else
#define FALLBACK_SWAP
#define betole16(x) swap_u16(x)
#define betole32(x) swap_u32(x)
#define betole64(x) swap_u64(x)
#endif
#define betole128(x) swap_u128(x)
#define betole256(x) swap_u256(x)
#else
#define betole16(x) (x)
#define betole32(x) (x)
#define betole64(x) (x)
#define betole128(x) (x)
#define betole256(x) (x)
#endif  // BIGENDIAN

#if defined(BIGENDIAN)
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>

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

#if defined(FALLBACK_SWAP)
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
#endif  // FALLBACK_SWAP

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

#include "random.hpp"

namespace atom::utils {
namespace detail {
/*
Converts a 128-bits unsigned int to an UUIDv4 string representation.
Uses SIMD via Intel's AVX2 instruction set.
*/
void inline m128itos(__m128i x, char* mem) {
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
}

/*
  Converts an UUIDv4 string representation to a 128-bits unsigned int.
  Uses SIMD via Intel's AVX2 instruction set.
 */
__m128i inline stom128i(const char* mem) {
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
    __m256i subMask = _mm256_blendv_epi8(DIGITS_OFFSET, ALPHA_OFFSET, alpha);
    a = _mm256_sub_epi8(a, subMask);
    a = _mm256_shuffle_epi8(a, UNWEAVE);
    a = _mm256_sllv_epi32(a, SHIFT);
    a = _mm256_hadd_epi32(a, _mm256_setzero_si256());
    a = _mm256_permute4x64_epi64(a, 0b00001000);

    return _mm256_castsi256_si128(a);
}

}  // namespace detail
UUID::UUID() { generateRandom(); }

UUID::UUID(const std::array<uint8_t, 16>& data) : data_(data) {}

auto UUID::toString() const -> std::string {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < data_.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(data_[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }
    return oss.str();
}

auto UUID::fromString(const std::string& str) -> UUID {
    UUID uuid;
    size_t pos = 0;
    for (unsigned char& i : uuid.data_) {
        if (str[pos] == '-') {
            ++pos;
        }
        i = std::stoi(str.substr(pos, 2), nullptr, 16);
        pos += 2;
    }
    return uuid;
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
    uuid = UUID::fromString(str);
    return is;
}

auto UUID::getData() const -> std::array<uint8_t, 16> { return data_; }

auto UUID::version() const -> uint8_t { return (data_[6] & 0xF0) >> 4; }

auto UUID::variant() const -> uint8_t { return (data_[8] & 0xC0) >> 6; }

auto UUID::generateV3(const UUID& namespace_uuid,
                      const std::string& name) -> UUID {
    return generateNameBased<EVP_md5>(namespace_uuid, name, 3);
}

auto UUID::generateV4() -> UUID {
    // Generate a random UUID (version 4)
    UUID uuid;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);

    for (auto& byte : uuid.data_) {
        byte = dist(gen);
    }

    // Set version to 4 (randomly generated UUID)
    uuid.data_[6] = (uuid.data_[6] & 0x0F) | 0x40;

    // Set the variant (RFC 4122 variant)
    uuid.data_[8] = (uuid.data_[8] & 0x3F) | 0x80;

    return uuid;
}

auto UUID::generateV5(const UUID& namespace_uuid,
                      const std::string& name) -> UUID {
    return generateNameBased<EVP_sha1>(namespace_uuid, name, 5);
}

auto UUID::generateV1() -> UUID {
    UUID uuid;
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    auto timestamp = static_cast<uint64_t>(millis * 10000) + 0x01B21DD213814000;

    Random<std::mt19937, std::uniform_int_distribution<>> rand(
        1, std::numeric_limits<int>::max());

    uint16_t clockSeq = static_cast<uint16_t>(rand() % 0x4000);
    uint64_t node = generateNode();

    std::memcpy(uuid.data_.data(), &timestamp, 8);
    std::memcpy(uuid.data_.data() + 8, &clockSeq, 2);
    std::memcpy(uuid.data_.data() + 10, &node, 6);

    uuid.data_[6] = (uuid.data_[6] & 0x0F) | 0x10;  // Version 1
    uuid.data_[8] = (uuid.data_[8] & 0x3F) | 0x80;  // Variant

    return uuid;
}

void UUID::generateRandom() {
    Random<std::mt19937, std::uniform_int_distribution<>> gen(
        1, std::numeric_limits<int>::max());
    for (auto& byte : data_) {
        byte = static_cast<uint8_t>(gen());
    }

    data_[6] = (data_[6] & 0x0F) | 0x40;  // Version 4
    data_[8] = (data_[8] & 0x3F) | 0x80;  // Variant
}

auto UUID::generateNode() -> uint64_t {
    std::random_device rd;
    std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFF);
    return dist(rd) | 0x010000000000;  // Multicast bit set to 1
}

auto getMAC() -> std::string {
    std::string mac;

#if defined(_WIN32)
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufferSize = sizeof(adapterInfo);

    if (GetAdaptersInfo(adapterInfo, &bufferSize) == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (UINT i = 0; i < pAdapterInfo->AddressLength; i++) {
            oss << std::setw(2) << static_cast<int>(pAdapterInfo->Address[i]);
        }
        mac = oss.str();
    }

#elif defined(__linux__) || defined(__APPLE__)
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
        return mac;
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
        close(sock);
        return mac;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) {
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
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
                << static_cast<int>(
                       static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[i]));
        }
        mac = oss.str();
    }

    close(sock);
#endif

    return mac;
}

auto getCPUSerial() -> std::string {
    std::string cpuSerial;

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
            if (line.find("Serial") != std::string::npos) {
                std::istringstream iss(line);
                std::string key;
                std::string value;
                if (std::getline(iss, key, ':') && std::getline(iss, value)) {
                    cpuSerial = value;
                    break;
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
    }
    cpuSerial = result;
#else
#error "Unsupported platform"
#endif

    return cpuSerial;
}

auto formatUUID(const std::string& uuid) -> std::string {
    std::string formattedUUID;
    formattedUUID.reserve(36);

    for (size_t i = 0; i < uuid.length(); i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            formattedUUID.push_back('-');
        }
        formattedUUID.push_back(uuid[i]);
    }

    return formattedUUID;
}

auto generateUniqueUUID() -> std::string {
    std::string mac = getMAC();
    std::string cpuSerial = getCPUSerial();

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < mac.length(); i += 2) {
        oss << std::setw(2)
            << static_cast<int>(std::stoul(mac.substr(i, 2), nullptr, 16));
    }
    for (size_t i = 0; i < cpuSerial.length(); i += 2) {
        oss << std::setw(2)
            << static_cast<int>(
                   std::stoul(cpuSerial.substr(i, 2), nullptr, 16));
    }

    std::string uuid = oss.str();
    uuid.erase(std::remove_if(uuid.begin(), uuid.end(), ::isspace), uuid.end());
    uuid = formatUUID(uuid);

    return uuid;
}

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
}  // namespace atom::utils
