#include "atom/type/expected.hpp"

#include <iostream>
#include <string>

using namespace atom::type;

// 示例错误枚举
enum class DivisionError {
    DivideByZero,
    Overflow
};

// 安全除法函数，返回expected
expected<double, DivisionError> safe_divide(double a, double b) {
    if (b == 0.0) {
        return unexpected(DivisionError::DivideByZero);
    }
    
    if (a > 1e100 && b < 1e-100) {
        return unexpected(DivisionError::Overflow);
    }
    
    return a / b;
}

// fnmatch示例对应的错误处理
enum class PatternError {
    InvalidPattern,
    EmptyInput
};

// 一个简单的模式匹配函数
expected<bool, PatternError> simple_pattern_match(const std::string& pattern, 
                                                 const std::string& input) {
    if (pattern.empty()) {
        return unexpected(PatternError::InvalidPattern);
    }
    
    if (input.empty()) {
        return unexpected(PatternError::EmptyInput);
    }
    
    // 简单实现，仅作演示
    return pattern == "*" || pattern == input;
}

int main() {
    // 除法示例
    auto result1 = safe_divide(10.0, 2.0);
    if (result1) {
        std::cout << "除法结果: " << result1.value() << std::endl;
    } else {
        std::cout << "除法错误" << std::endl;
    }
    
    auto result2 = safe_divide(10.0, 0.0);
    if (!result2) {
        std::cout << "预期的错误: 除数为零" << std::endl;
    }
    
    // 模式匹配示例
    auto match1 = simple_pattern_match("*", "任何字符串");
    if (match1 && match1.value()) {
        std::cout << "模式匹配成功" << std::endl;
    }
    
    auto match2 = simple_pattern_match("", "测试");
    if (!match2) {
        std::cout << "预期的错误: 无效模式" << std::endl;
    }
    
    // 演示monadic操作
    auto result3 = safe_divide(10.0, 2.0)
                    .and_then([](double val) -> expected<std::string, DivisionError> {
                        return "结果是: " + std::to_string(val);
                    });
                    
    if (result3) {
        std::cout << result3.value() << std::endl;
    }
    
    return 0;
}