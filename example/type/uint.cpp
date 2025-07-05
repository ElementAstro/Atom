#include <iostream>

#include "atom/type/uint.hpp"

int main() {
    try {
        // 使用自定义字面量创建不同类型的无符号整数
        uint8_t a = 123_u8;                    // 创建 uint8_t 类型的值
        uint16_t b = 12345_u16;                // 创建 uint16_t 类型的值
        uint32_t c = 123456789_u32;            // 创建 uint32_t 类型的值
        uint64_t d = 1234567890123456789_u64;  // 创建 uint64_t 类型的值

        // 输出这些值
        std::cout << "a (uint8_t) = " << static_cast<uint32_t>(a)
                  << std::endl;  // uint8_t 需要转换为 uint32_t 以便正确输出
        std::cout << "b (uint16_t) = " << b << std::endl;
        std::cout << "c (uint32_t) = " << c << std::endl;
        std::cout << "d (uint64_t) = " << d << std::endl;

        // 测试越界情况
        try {
            uint8_t e = 300_u8;  // 这个会抛出异常
        } catch (const std::out_of_range& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }

        try {
            uint16_t f = 70000_u16;  // 这个会抛出异常
        } catch (const std::out_of_range& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
