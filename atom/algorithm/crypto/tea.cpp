#include "tea.hpp"

#include <array>
#include <string>

namespace atom::algorithm {

// TEA encryption function
auto teaEncrypt(u32& value0, u32& value1,
                const std::array<u32, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for TEA encryption");
            throw TEAException("Invalid key for TEA encryption");
        }

        u32 sum = 0;
        for (i32 i = 0; i < TEA_NUM_ROUNDS; ++i) {
            sum += TEA_DELTA;
            value0 += ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            value1 += ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
        }
    } catch (const TEAException&) {
        throw;  // Re-throw TEA specific exceptions
    } catch (const std::exception& e) {
        spdlog::error("TEA encryption error: {}", e.what());
        throw TEAException(std::string("TEA encryption error: ") + e.what());
    }
}

// TEA decryption function
auto teaDecrypt(u32& value0, u32& value1,
                const std::array<u32, 4>& key) noexcept(false) -> void {
    try {
        if (!isValidKey(key)) {
            spdlog::error("Invalid key provided for TEA decryption");
            throw TEAException("Invalid key for TEA decryption");
        }

        u32 sum = TEA_DELTA * TEA_NUM_ROUNDS;
        for (i32 i = 0; i < TEA_NUM_ROUNDS; ++i) {
            value1 -= ((value0 << SHIFT_4) + key[2]) ^ (value0 + sum) ^
                      ((value0 >> SHIFT_5) + key[3]);
            value0 -= ((value1 << SHIFT_4) + key[0]) ^ (value1 + sum) ^
                      ((value1 >> SHIFT_5) + key[1]);
            sum -= TEA_DELTA;
        }
    } catch (const TEAException&) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("TEA decryption error: {}", e.what());
        throw TEAException(std::string("TEA decryption error: ") + e.what());
    }
}

}  // namespace atom::algorithm
