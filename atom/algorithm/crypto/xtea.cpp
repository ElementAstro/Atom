#include "xtea.hpp"

#include <string>

namespace atom::algorithm {

// XTEA encryption function with enhanced security and validation
auto xteaEncrypt(u32& value0, u32& value1, const XTEAKey& key) noexcept(false)
    -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for XTEA encryption");
            throw TEAException("Invalid key for XTEA encryption");
        }

        u32 sum = 0;
        for (i32 i = 0; i < TEA_NUM_ROUNDS; ++i) {
            value0 += (((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1) ^
                      (sum + key[sum & KEY_MASK]);
            sum += TEA_DELTA;
            value1 += (((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0) ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("XTEA encryption error: {}", e.what());
        throw TEAException(std::string("XTEA encryption error: ") + e.what());
    }
}

// XTEA decryption function with enhanced security and validation
auto xteaDecrypt(u32& value0, u32& value1, const XTEAKey& key) noexcept(false)
    -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for XTEA decryption");
            throw TEAException("Invalid key for XTEA decryption");
        }

        u32 sum = TEA_DELTA * TEA_NUM_ROUNDS;
        for (i32 i = 0; i < TEA_NUM_ROUNDS; ++i) {
            value1 -= (((value0 << SHIFT_4) ^ (value0 >> SHIFT_5)) + value0) ^
                      (sum + key[(sum >> SHIFT_11) & KEY_MASK]);
            sum -= TEA_DELTA;
            value0 -= (((value1 << SHIFT_4) ^ (value1 >> SHIFT_5)) + value1) ^
                      (sum + key[sum & KEY_MASK]);
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("XTEA decryption error: {}", e.what());
        throw TEAException(std::string("XTEA decryption error: ") + e.what());
    }
}

}  // namespace atom::algorithm
