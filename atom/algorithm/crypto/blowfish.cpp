#include "blowfish.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <future>
#include <span>

#include "atom/error/exception.hpp"

namespace atom::algorithm {

// Initial state constants
static constexpr std::array<u32, 18> INITIAL_P = {
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344, 0xa4093822, 0x299f31d0,
    0x082efa98, 0xec4e6c89, 0x557a8e8c, 0x163f1dbe, 0x37e1e9af, 0x37cda6b7,
    0x58e0f419, 0x3de9c6a1, 0x6e10e33f, 0x28782c2f, 0x1f2b4e36, 0x74855fa2};

static constexpr std::array<std::array<uint32_t, 256>, 4> INITIAL_S = {
    {{0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0xd01adfb7, 0xb8e1afed, 0x6a267e96,
      0xba7c9045, 0xf12c7f99, 0x24a19947, 0xb3916cf7, 0x0801f2e2, 0x858efc16,
      0x636920d8, 0x71574e69, 0xa458fea3, 0xf4933d7e, 0x0d95748f, 0x728eb658,
      0x718bcd58, 0x82154aee, 0x7b54a41d, 0xc25a59b5, 0x9c30d539, 0x2af26013,
      0xc5d1b023, 0x286085f0, 0xca417918, 0xb8db38ef, 0x8e79dcb0, 0x603a180e,
      0x6c9e0e8b, 0xb01e8a3e, 0xd71577c1, 0xbd314b27, 0x78af2fda, 0x55605c60,
      0xe65525f3, 0xaa55ab94, 0x57489862, 0x63e81440, 0x55ca396a, 0x2aab10b6,
      0xb4cc5c34, 0x1141e8ce, 0xa15486af, 0x7c72e993, 0xb3ee1411, 0x636fbc2a,
      0x2ba9c55d, 0x741831f6, 0xce5c3e16, 0x9b87931e, 0xafd6ba33, 0x6c24cf5c,
      0x7a325381, 0x28958677, 0x3b8f4898, 0x6b4bb9af, 0xc4bfe81b, 0x66282193,
      0x61d809cc, 0xfb21a991, 0x487cac60, 0x5dec8032, 0xef845d5d, 0xe98575b1,
      0xdc262302, 0xeb651b88, 0x23893e81, 0xd396acc5, 0x0f6d6ff3, 0x83f44239,
      0x2e0b4482, 0xa4842004, 0x69c8f04a, 0x9e1f9b5e, 0x21c66842, 0xf6e96c9a,
      0x670c9c61, 0xabd388f0, 0x6a51a0d2, 0xd8542f68, 0x960fa728, 0xab5133a3,
      0x6eef0b6c, 0x137a3be4, 0xba3bf050, 0x7efb2a98, 0xa1f1651d, 0x39af0176,
      0x66ca593e, 0x82430e88, 0x8cee8619, 0x456f9fb4, 0x7d84a5c3, 0x3b8b5ebe,
      0xe06f75d8, 0x85c12073, 0x401a449f, 0x56c16aa6, 0x4ed3aa62, 0x363f7706,
      0x1bfedf72, 0x429b023d, 0x37d0d724, 0xd00a1248, 0xdb0fead3, 0x49f1c09b,
      0x075372c9, 0x80991b7b, 0x25d479d8, 0xf6e8def7, 0xe3fe501a, 0xb6794c3b,
      0x976ce0bd, 0x04c006ba, 0xc1a94fb6, 0x409f60c4, 0x5e5c9ec2, 0x196a2463,
      0x68fb6faf, 0x3e6c53b5, 0x1339b2eb, 0x3b52ec6f, 0x6dfc511f, 0x9b30952c,
      0xcc814544, 0xaf5ebd09, 0xbee3d004, 0xde334afd, 0x660f2807, 0x192e4bb3,
      0xc0cba857, 0x45c8740f, 0xd20b5f39, 0xb9d3fbdb, 0x5579c0bd, 0x1a60320a,
      0xd6a100c6, 0x402c7279, 0x679f25fe, 0xfb1fa3cc, 0x8ea5e9f8, 0xdb3222f8,
      0x3c7516df, 0xfd616b15, 0x2f501ec8, 0xad0552ab, 0x323db5fa, 0xfd238760,
      0x53317b48, 0x3e00df82, 0x9e5c57bb, 0xca6f8ca0, 0x1a87562e, 0xdf1769db,
      0xd542a8f6, 0x287effc3, 0xac6732c6, 0x8c4f5573, 0x695b27b0, 0xbbca58c8,
      0xe1ffa35d, 0xb8f011a0, 0x10fa3d98, 0xfd2183b8, 0x4afcb56c, 0x2dd1d35b,
      0x9a53e479, 0xb6f84565, 0xd28e49bc, 0x4bfb9790, 0xe1ddf2da, 0xa4cb7e33,
      0x62fb1341, 0xcee4c6e8, 0xef20cada, 0x36774c01, 0xd07e9efe, 0x2bf11fb4,
      0x95dbda4d, 0xae909198, 0xeaad8e71, 0x6b93d5a0, 0xd08ed1d0, 0xafc725e0,
      0x8e3c5b2f, 0x8e7594b7, 0x8ff6e2fb, 0xf2122b64, 0x8888b812, 0x900df01c},
     {0x4fad5ea0, 0x688fc31c, 0xd1cff191, 0xb3a8c1ad, 0x2f2f2218, 0xbe0e1777,
      0xea752dfe, 0x8b021fa1, 0xe5a0cc0f, 0xb56f74e8, 0x18acf3d6, 0xce89e299,
      0xb4a84fe0, 0xfd13e0b7, 0x7cc43b81, 0xd2ada8d9, 0x165fa266, 0x80957705,
      0x93cc7314, 0x211a1477, 0xe6ad2065, 0x77b5fa86, 0xc75442f5, 0xfb9d35cf,
      0xebcdaf0c, 0x7b3e89a0, 0xd6411bd3, 0xae1e7e49, 0x00250e2d, 0x2071b35e,
      0x226800bb, 0x57b8e0af, 0x2464369b, 0xf009b91e, 0x5563911d, 0x59dfa6aa,
      0x78c14389, 0xd95a537f, 0x207d5ba2, 0x02e5b9c5, 0x83260376, 0x6295cfa9,
      0x11c81968, 0x4e734a41, 0xb3472dca, 0x7b14a94a, 0x1b510052, 0x9a532915,
      0xd60f573f, 0xbc9bc6e4, 0x2b60a476, 0x81e67400, 0x08ba6fb5, 0x571be91f,
      0xf296ec6b, 0x2a0dd915, 0xb6636521, 0xe7b9f9b6, 0xff34052e, 0xc5855664,
      0x53b02d5d, 0xa99f8fa1, 0x08ba4799, 0x6e85076a, 0x4b7a70e9, 0xb5b32944,
      0xdb75092e, 0xc4192623, 0xad6ea6b0, 0x49a7df7d, 0x9cee60b8, 0x8fedb266,
      0xecaa8c71, 0x699a17ff, 0x5664526c, 0xc2b19ee1, 0x193602a5, 0x75094c29,
      0xa0591340, 0xe4183a3e, 0x3f54989a, 0x5b429d65, 0x6b8fe4d6, 0x99f73fd6,
      0xa1d29c07, 0xefe830f5, 0x4d2d38e6, 0xf0255dc1, 0x4cdd2086, 0x8470eb26,
      0x6382e9c6, 0x021ecc5e, 0x09686b3f, 0x3ebaefc9, 0x3c971814, 0x6b6a70a1,
      0x687f3584, 0x52a0e286, 0xb79c5305, 0xaa500737, 0x3e07841c, 0x7fdeae5c,
      0x8e7d44ec, 0x5716f2b8, 0xb03ada37, 0xf0500c0d, 0xf01c1f04, 0x0200b3ff,
      0xae0cf51a, 0x3cb574b2, 0x25837a58, 0xdc0921bd, 0xd19113f9, 0x7ca92ff6,
      0x94324773, 0x22f54701, 0x3ae5e581, 0x37c2dadc, 0xc8b57634, 0x9af3dda7,
      0xa9446146, 0x0fd0030e, 0xecc8c73e, 0xa4751e41, 0xe238cd99, 0x3bea0e2f,
      0x3280bba1, 0x183eb331, 0x4e548b38, 0x4f6db908, 0x6f420d03, 0xf60a04bf,
      0x2cb81290, 0x24977c79, 0x5679b072, 0xbcaf89af, 0xde9a771f, 0xd9930810,
      0xb38bae12, 0xdccf3f2e, 0x5512721f, 0x2e6b7124, 0x501adde6, 0x9f84cd87,
      0x7a584718, 0x7408da17, 0xbc9f9abc, 0xe94b7d8c, 0xec7aec3a, 0xdb851dfa,
      0x63094366, 0xc464c3d2, 0xef1c1847, 0x3215d908, 0xdd433b37, 0x24c2ba16,
      0x12a14d43, 0x2a65c451, 0x50940002, 0x133ae4dd, 0x71dff89e, 0x10314e55,
      0x81ac77d6, 0x5f11199b, 0x043556f1, 0xd7a3c76b, 0x3c11183b, 0x5924a509,
      0xf28fe6ed, 0x97f1fbfa, 0x9ebabf2c, 0x1e153c6e, 0x86e34570, 0xeae96fb1,
      0x860e5e0a, 0x5a3e2ab3, 0x771fe71c, 0x4e3d06fa, 0x2965dcb9, 0x99e71d0f,
      0x803e89d6, 0x5266c825, 0x2e4cc978, 0x9c10b36a, 0xc6150eba, 0x94e2ea78},
     {0xa0e6e70,  0xbfb1d890, 0xca8f3e68, 0x2519a122, 0xc8293d02, 0xa2f8f157,
      0x8ca25e3b, 0x0d6f3522, 0xcc76f1c3, 0x5f0d5937, 0x00458f45, 0x40fd0002,
      0xedc67487, 0xbe79e842, 0xb11c4d55, 0xcbf929d0, 0x7a93dbd6, 0x1b71b526,
      0x53dba84b, 0xe3100197, 0x88265779, 0x8633f018, 0x99f8c9ff, 0x4a60b3bf,
      0x5c100ed8, 0x2ab91c3f, 0x20d1b4d6, 0xf8dbb914, 0xb76e79e0, 0xd60f93b4,
      0x25976c3f, 0xb22d7733, 0xfa78b420, 0x65582185, 0x68ab9802, 0xeecea50f,
      0xdb2f953b, 0x2aef7dad, 0x5b6e2f84, 0x1521b628, 0x29076170, 0xecdd4775,
      0x619f1510, 0x13cca830, 0xeb61bd96, 0x0334fe1e, 0xaa0363cf, 0xb5735c90,
      0x4c70a239, 0xd59e9e0b, 0xcbaade14, 0xeecc86bc, 0x60622ca7, 0x9cab5cab,
      0xb2f3846e, 0x648b1eaf, 0x19bdf0ca, 0xa02369b9, 0x655abb50, 0x40685a32,
      0x3c2ab4b3, 0x319ee9d5, 0xc021b8f7, 0x9b540b19, 0x875fa099, 0x95f7997e,
      0x623d7da8, 0xf837889a, 0x97e32d77, 0x11ed935f, 0x16681281, 0x0e358829,
      0xc7e61fd6, 0x96dedfa1, 0x7858ba99, 0x57f584a5, 0x1b227263, 0x9b83c3ff,
      0x1ac24696, 0xcdb30aeb, 0x532e3054, 0x8fd948e4, 0x6dbc3128, 0x58ebf2ef,
      0x34c6ffea, 0xfe28ed61, 0xee7c3c73, 0x5d4a14d9, 0xe864b7e3, 0x42105d14,
      0x203e13e0, 0x45eee2b6, 0xa3aaabea, 0xdb6c4f15, 0xfacb4fd0, 0xc742f442,
      0xef6abbb5, 0x654f3b1d, 0x41cd2105, 0xd81e799e, 0x86854dc7, 0xe44b476a,
      0x3d816250, 0xcf62a1f2, 0x5b8d2646, 0xfc8883a0, 0xc1c7b6a3, 0x7f1524c3,
      0x69cb7492, 0x47848a0b, 0x5692b285, 0x095bbf00, 0xad19489d, 0x1462b174,
      0x23820e00, 0x58428d2a, 0x0c55f5ea, 0x1dadf43e, 0x233f7061, 0x3372f092,
      0x8d937e41, 0xd65fecf1, 0x6c223bdb, 0x7cde3759, 0xcbee7460, 0x4085f2a7,
      0xce77326e, 0xa6078084, 0x19f8509e, 0xe8efd855, 0x61d99735, 0xa969a7aa,
      0xc50c06c2, 0x5a04abfc, 0x800bcadc, 0x9e447a2e, 0xc3453484, 0xfdd56705,
      0x0e1e9ec9, 0xdb73dbd3, 0x105588cd, 0x675fda79, 0xe3674340, 0xc5c43465,
      0x713e38d8, 0x3d28f89e, 0xf16dff20, 0x153e21e7, 0x8fb03d4a, 0xe6e39f2b,
      0xdb83adf7, 0xd9fd96a2, 0xa099769e, 0x17bfdcf2, 0x74e8344a, 0xc7032091,
      0x447544e5, 0x505c0218, 0x7be0a855, 0xdbe4c803, 0xbf404a2e, 0xeeef2a38,
      0x10b6a374, 0x4167d66b, 0x1c101265, 0x55c6aa7e, 0xdd4a9503, 0xb5279da2,
      0x7f2c8724, 0x37c1be75, 0xada8061c, 0x91e71f04, 0xc4e22f1c, 0x9fbc5984,
      0x6da49b85, 0xb0c0833d, 0xc2de31d6, 0x2f0e9235, 0x17298cdc, 0x58ccf281},
     {0x96e1db2a, 0x6c48916e, 0x3ffd684f, 0x88abe969, 0x4a085c6c, 0xbbc66983,
      0x04ad1397, 0x82eb8ff5, 0xe2bc5ec2, 0x0e1711c1, 0x5b8d9349, 0xf405ed4d,
      0xc3561816, 0x2bf1c0dd, 0x02cd8d2f, 0x4eccaf8d, 0x5f3e2c1e, 0x932e1c51,
      0xa05168d6, 0xcab917cd, 0xb1908a00, 0x4ab825c0, 0x5fa21353, 0x8d325024,
      0x8d725b02, 0x84e5cbdc, 0x0cdcf48e, 0xbe81f2c2, 0x1b4c67f2, 0x5f6e2793,
      0x83117c8a, 0x1028a8a3, 0x866cfcb0, 0x0a6d1061, 0x73360053, 0xc5c5c190,
      0x16b9265c, 0x86d28022, 0x0f16f7d2, 0x8d8904fb, 0x8ae0e5bc, 0x5d072770,
      0x977c6c1a, 0xc53b37a1, 0x0ca8079a, 0x735d46cf, 0xc4a6fe8a, 0x41224f3d,
      0x0ce4218b, 0x8be25f62, 0xadd8e2d9, 0x5c7fb2c8, 0x2804546c, 0x14047eb7,
      0xc2c3d6dc, 0xebd4fc7b, 0x85f0fe8c, 0x0b6b8e5a, 0xe39ed557, 0x887c37a8,
      0xf9bb74d0, 0x61d1e4c7, 0xc4efb647, 0xd5f86079, 0x6351814a, 0x99768e2e,
      0xb494026c, 0x8b6f7fd0, 0x23140665, 0xbe131f6f, 0x450e4974, 0x4c3085dc,
      0x7f869a80, 0x32c7d9d3, 0xb188d2e0, 0x1665ed65, 0x3208d07d, 0x8d0cba4d,
      0x4e23e8c6, 0x6b89fbf0, 0x6f2da68c, 0x8abc279b, 0x514ac3be, 0x5f7abd09,
      0x75cc2699, 0x630d4948, 0x98d0c9e5, 0xfab27a5f, 0xae1e663b, 0x06ab1489,
      0xe205c3cd, 0xc9d9a3e3, 0x7c260953, 0x5a704cbc, 0xec53d43c, 0xce5c3e16,
      0x3868e1a9, 0x85cfbb40, 0x45c3370d, 0x742beb1a, 0x386db04c, 0xb1d219ee,
      0x145225f2, 0x2366c9ab, 0x81920417, 0xf9bcc7f6, 0x9d775adc, 0x12318802,
      0x188c6e52, 0x388d1c03, 0xba66a0cf, 0x02d4d506, 0x78486c5c, 0x7182c980,
      0x05b8d8c1, 0x3c6eeafb, 0x36126857, 0x584e3440, 0x67bd8808, 0x0381dfdd,
      0x77c6a7e5, 0x0b0b595f, 0xc42bf83b, 0x5042f7a0, 0x5ba7db0c, 0xa3768c30,
      0x865a5c9b, 0xf874b172, 0x39154189, 0x65fb0875, 0x4565c95a, 0x1b05f9f5,
      0xb046c6c2, 0xf0ad1015, 0x681499da, 0xeb7768f0, 0x89e3fffe, 0x0c66b641,
      0xcdc326a3, 0xf76a5929, 0x9b540b19, 0xae3d1ed5, 0x2f46f732, 0x8814f634,
      0x9a91ab2e, 0xd93ed3b7, 0xbf5d3af5, 0x31682a0d, 0xb969e222, 0xe6d677b8,
      0x5bd748de, 0x741b47bc, 0xdeaed876, 0x1db956e8, 0xaef08eb5, 0x5e11ca51,
      0xf87e3dd0, 0xe3d3f38d, 0x87c57b57, 0xb8f83bad, 0x4bca1649, 0x0b42f788,
      0xbf44d2f5, 0xb1b872cf, 0x69fa3c42, 0x82c7709e, 0x41ecc7da, 0xb2f200ca,
      0x545b9025, 0x14102f6e, 0x3ad2ff38, 0x8c54fc21, 0xd2227597, 0x4d962d87,
      0xa2f2d784, 0x14ce598f, 0x78a0c7c5, 0xa4f3c544, 0x6e1cd93e, 0x41c4d66b}}};

static constexpr usize BLOCK_SIZE = 8;

/**
 * @brief Converts a byte-like value to std::byte.
 */
template <ByteType T>
[[nodiscard]] static constexpr auto to_byte(T value) noexcept -> std::byte {
    return static_cast<std::byte>(static_cast<unsigned char>(value));
}

/**
 * @brief Converts from std::byte to another byte-like type.
 */
template <ByteType T>
[[nodiscard]] static constexpr auto from_byte(std::byte value) noexcept -> T {
    return static_cast<T>(std::to_integer<unsigned char>(value));
}

template <ByteType T>
void pkcs7_padding(std::span<T> data, usize& length) {
    usize padding_length = BLOCK_SIZE - (length % BLOCK_SIZE);
    if (padding_length == 0) {
        padding_length = BLOCK_SIZE;
    }

    // Ensure sufficient buffer space for padding
    if (data.size() < length + padding_length) {
        spdlog::error("Insufficient buffer space for padding");
        THROW_RUNTIME_ERROR("Insufficient buffer space for padding");
    }

    // Add PKCS7 padding
    auto padding_value = static_cast<T>(padding_length);
    std::fill(data.begin() + length, data.begin() + length + padding_length,
              padding_value);

    length += padding_length;
    spdlog::debug("Padding applied, new length: {}", length);
}

Blowfish::Blowfish(std::span<const std::byte> key) {
    spdlog::info("Initializing Blowfish with key length: {}", key.size());
    validate_key(key);
    init_state(key);
    spdlog::info("Blowfish initialization complete");
}

void Blowfish::validate_key(std::span<const std::byte> key) const {
    if (key.empty() || key.size() > 56) {
        spdlog::error("Invalid key length: {}", key.size());
        THROW_RUNTIME_ERROR(
            "Invalid key length. Must be between 1 and 56 bytes.");
    }
}

void Blowfish::init_state(std::span<const std::byte> key) {
    std::ranges::copy(INITIAL_P, P_.begin());
    std::ranges::copy(INITIAL_S, S_.begin());

    // Using regular loop for P-array initialization
    for (usize i = 0; i < P_ARRAY_SIZE; ++i) {
        u32 data = 0;
        usize key_index = 0;
        data = (std::to_integer<u32>(key[key_index]) << 24) |
               (std::to_integer<u32>(key[(key_index + 1) % key.size()]) << 16) |
               (std::to_integer<u32>(key[(key_index + 2) % key.size()]) << 8) |
               (std::to_integer<u32>(key[(key_index + 3) % key.size()]));
        P_[i] ^= data;
        key_index = (key_index + 4) % key.size();
    }

    // S-box initialization
    for (usize i = 0; i < 4; ++i) {
        for (usize j = 0; j < S_BOX_SIZE; ++j) {
            u32 data = 0;
            usize key_index = 0;
            data =
                (std::to_integer<u32>(key[key_index]) << 24) |
                (std::to_integer<u32>(key[(key_index + 1) % key.size()])
                 << 16) |
                (std::to_integer<u32>(key[(key_index + 2) % key.size()]) << 8) |
                (std::to_integer<u32>(key[(key_index + 3) % key.size()]));
            S_[i][j] ^= data;
            key_index = (key_index + 4) % key.size();
        }
    }
}

u32 Blowfish::F(u32 x) const noexcept {
    unsigned char a = (x >> 24) & 0xFF;
    unsigned char b = (x >> 16) & 0xFF;
    unsigned char c = (x >> 8) & 0xFF;
    unsigned char d = x & 0xFF;

    return (S_[0][a] + S_[1][b]) ^ S_[2][c] + S_[3][d];
}

void Blowfish::encrypt(std::span<std::byte, BLOCK_SIZE> block) noexcept {
    spdlog::debug("Encrypting block");

    u32 left = (std::to_integer<u32>(block[0]) << 24) |
               (std::to_integer<u32>(block[1]) << 16) |
               (std::to_integer<u32>(block[2]) << 8) |
               std::to_integer<u32>(block[3]);
    u32 right = (std::to_integer<u32>(block[4]) << 24) |
                (std::to_integer<u32>(block[5]) << 16) |
                (std::to_integer<u32>(block[6]) << 8) |
                std::to_integer<u32>(block[7]);

    left ^= P_[0];
    for (int i = 1; i <= 16; i += 2) {
        right ^= F(left) ^ P_[i];
        left ^= F(right) ^ P_[i + 1];
    }

    right ^= P_[17];

    block[0] = static_cast<std::byte>((right >> 24) & 0xFF);
    block[1] = static_cast<std::byte>((right >> 16) & 0xFF);
    block[2] = static_cast<std::byte>((right >> 8) & 0xFF);
    block[3] = static_cast<std::byte>(right & 0xFF);
    block[4] = static_cast<std::byte>((left >> 24) & 0xFF);
    block[5] = static_cast<std::byte>((left >> 16) & 0xFF);
    block[6] = static_cast<std::byte>((left >> 8) & 0xFF);
    block[7] = static_cast<std::byte>(left & 0xFF);
}

void Blowfish::decrypt(std::span<std::byte, BLOCK_SIZE> block) noexcept {
    spdlog::debug("Decrypting block");

    u32 left = (std::to_integer<u32>(block[0]) << 24) |
               (std::to_integer<u32>(block[1]) << 16) |
               (std::to_integer<u32>(block[2]) << 8) |
               std::to_integer<u32>(block[3]);
    u32 right = (std::to_integer<u32>(block[4]) << 24) |
                (std::to_integer<u32>(block[5]) << 16) |
                (std::to_integer<u32>(block[6]) << 8) |
                std::to_integer<u32>(block[7]);

    left ^= P_[17];
    for (int i = 16; i >= 1; i -= 2) {
        right ^= F(left) ^ P_[i];
        left ^= F(right) ^ P_[i - 1];
    }

    right ^= P_[0];

    block[0] = static_cast<std::byte>((right >> 24) & 0xFF);
    block[1] = static_cast<std::byte>((right >> 16) & 0xFF);
    block[2] = static_cast<std::byte>((right >> 8) & 0xFF);
    block[3] = static_cast<std::byte>(right & 0xFF);
    block[4] = static_cast<std::byte>((left >> 24) & 0xFF);
    block[5] = static_cast<std::byte>((left >> 16) & 0xFF);
    block[6] = static_cast<std::byte>((left >> 8) & 0xFF);
    block[7] = static_cast<std::byte>(left & 0xFF);
}

void Blowfish::validate_block_size(usize size) {
    if (size % BLOCK_SIZE != 0) {
        spdlog::error("Invalid block size: {}. Must be a multiple of {}", size,
                      BLOCK_SIZE);
        THROW_RUNTIME_ERROR("Invalid block size");
    }
}

void Blowfish::remove_padding(std::span<std::byte> data, usize& length) {
    spdlog::debug("Removing PKCS7 padding");

    if (length == 0)
        return;

    usize padding_len = std::to_integer<usize>(data[length - 1]);
    if (padding_len > BLOCK_SIZE) {
        spdlog::error("Invalid padding length: {}", padding_len);
        THROW_RUNTIME_ERROR("Invalid padding length");
    }

    length -= padding_len;
    std::fill(data.begin() + length, data.end(), std::byte{0});

    spdlog::debug("Padding removed, new length: {}", length);
}

template <ByteType T>
void Blowfish::encrypt_data(std::span<T> data) {
    spdlog::info("Encrypting data of length: {}", data.size());
    validate_block_size(data.size());

    usize length = data.size();
    ::atom::algorithm::pkcs7_padding<T>(data, length);

    // Multi-threaded encryption for optimal performance
    const usize num_blocks = length / BLOCK_SIZE;
    const usize num_threads = std::min(
        num_blocks, static_cast<usize>(std::thread::hardware_concurrency()));

    if (num_threads > 1) {
        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        for (usize t = 0; t < num_threads; ++t) {
            futures.push_back(std::async(
                std::launch::async, [this, data, t, num_blocks, num_threads]() {
                    std::array<std::byte, BLOCK_SIZE> block_buffer;
                    for (usize i = t; i < num_blocks; i += num_threads) {
                        auto block = data.subspan(i * BLOCK_SIZE, BLOCK_SIZE);

                        // Convert to std::byte
                        for (usize j = 0; j < BLOCK_SIZE; ++j) {
                            block_buffer[j] = to_byte(block[j]);
                        }

                        encrypt(std::span<std::byte, BLOCK_SIZE>(block_buffer));

                        // Convert back to original type
                        for (usize j = 0; j < BLOCK_SIZE; ++j) {
                            block[j] = from_byte<T>(block_buffer[j]);
                        }
                    }
                }));
        }

        for (auto& future : futures) {
            future.get();
        }
    } else {
        // Single-threaded approach for small data
        std::array<std::byte, BLOCK_SIZE> block_buffer;
        for (usize i = 0; i < num_blocks; ++i) {
            auto block = data.subspan(i * BLOCK_SIZE, BLOCK_SIZE);

            for (usize j = 0; j < BLOCK_SIZE; ++j) {
                block_buffer[j] = to_byte(block[j]);
            }

            encrypt(std::span<std::byte, BLOCK_SIZE>(block_buffer));

            for (usize j = 0; j < BLOCK_SIZE; ++j) {
                block[j] = from_byte<T>(block_buffer[j]);
            }
        }
    }

    spdlog::info("Data encrypted successfully");
}

template <ByteType T>
void Blowfish::decrypt_data(std::span<T> data, usize& length) {
    spdlog::info("Decrypting data of length: {}", length);
    validate_block_size(length);

    // Multi-threaded decryption
    const usize num_blocks = length / BLOCK_SIZE;
    const usize num_threads = std::min(
        num_blocks, static_cast<usize>(std::thread::hardware_concurrency()));

    if (num_threads > 1) {
        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        for (usize t = 0; t < num_threads; ++t) {
            futures.push_back(std::async(
                std::launch::async, [this, data, t, num_blocks, num_threads]() {
                    std::array<std::byte, BLOCK_SIZE> block_buffer;
                    for (usize i = t; i < num_blocks; i += num_threads) {
                        auto block = data.subspan(i * BLOCK_SIZE, BLOCK_SIZE);

                        for (usize j = 0; j < BLOCK_SIZE; ++j) {
                            block_buffer[j] = to_byte(block[j]);
                        }

                        decrypt(std::span<std::byte, BLOCK_SIZE>(block_buffer));

                        for (usize j = 0; j < BLOCK_SIZE; ++j) {
                            block[j] = from_byte<T>(block_buffer[j]);
                        }
                    }
                }));
        }

        for (auto& future : futures) {
            future.get();
        }
    } else {
        std::array<std::byte, BLOCK_SIZE> block_buffer;
        for (usize i = 0; i < num_blocks; ++i) {
            auto block = data.subspan(i * BLOCK_SIZE, BLOCK_SIZE);

            for (usize j = 0; j < BLOCK_SIZE; ++j) {
                block_buffer[j] = to_byte(block[j]);
            }

            decrypt(std::span<std::byte, BLOCK_SIZE>(block_buffer));

            for (usize j = 0; j < BLOCK_SIZE; ++j) {
                block[j] = from_byte<T>(block_buffer[j]);
            }
        }
    }

    auto byte_span = std::span<std::byte>(
        reinterpret_cast<std::byte*>(data.data()), data.size());
    remove_padding(byte_span, length);

    spdlog::info("Data decrypted successfully, actual length: {}", length);
}

void Blowfish::encrypt_file(std::string_view input_file,
                            std::string_view output_file) {
    spdlog::info("Encrypting file: {}", input_file);

    std::ifstream infile(std::string(input_file),
                         std::ios::binary | std::ios::ate);
    if (!infile) {
        spdlog::error("Failed to open input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to open input file for reading");
    }

    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    // Calculate buffer size including padding
    usize buffer_size = size + (BLOCK_SIZE - (size % BLOCK_SIZE));
    if (size % BLOCK_SIZE == 0) {
        buffer_size += BLOCK_SIZE;  // Add full block of padding when size is
                                    // multiple of BLOCK_SIZE
    }

    std::vector<std::byte> buffer(buffer_size);
    if (!infile.read(reinterpret_cast<char*>(buffer.data()), size)) {
        spdlog::error("Failed to read input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to read input file");
    }

    encrypt_data(std::span<std::byte>(buffer));

    std::ofstream outfile(std::string(output_file), std::ios::binary);
    if (!outfile) {
        spdlog::error("Failed to open output file: {}", output_file);
        THROW_RUNTIME_ERROR("Failed to open output file for writing");
    }

    outfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    spdlog::info("File encrypted successfully: {}", output_file);
}

void Blowfish::decrypt_file(std::string_view input_file,
                            std::string_view output_file) {
    spdlog::info("Decrypting file: {}", input_file);

    std::ifstream infile(std::string(input_file),
                         std::ios::binary | std::ios::ate);
    if (!infile) {
        spdlog::error("Failed to open input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to open input file for reading");
    }

    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(size);
    if (!infile.read(reinterpret_cast<char*>(buffer.data()), size)) {
        spdlog::error("Failed to read input file: {}", input_file);
        THROW_RUNTIME_ERROR("Failed to read input file");
    }

    usize length = buffer.size();
    decrypt_data(std::span<std::byte>(buffer), length);

    std::ofstream outfile(std::string(output_file), std::ios::binary);
    if (!outfile) {
        spdlog::error("Failed to open output file: {}", output_file);
        THROW_RUNTIME_ERROR("Failed to open output file for writing");
    }

    outfile.write(reinterpret_cast<const char*>(buffer.data()), length);
    spdlog::info("File decrypted successfully: {}", output_file);
}

// Template instantiations
template void pkcs7_padding<std::byte>(std::span<std::byte>, usize&);
template void pkcs7_padding<char>(std::span<char>, usize&);
template void pkcs7_padding<unsigned char>(std::span<unsigned char>, usize&);

template void Blowfish::encrypt_data<std::byte>(std::span<std::byte>);
template void Blowfish::encrypt_data<char>(std::span<char>);
template void Blowfish::encrypt_data<unsigned char>(std::span<unsigned char>);
template void Blowfish::decrypt_data<std::byte>(std::span<std::byte>, usize&);
template void Blowfish::decrypt_data<char>(std::span<char>, usize&);
template void Blowfish::decrypt_data<unsigned char>(std::span<unsigned char>,
                                                    usize&);

}  // namespace atom::algorithm
