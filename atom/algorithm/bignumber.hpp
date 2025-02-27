#ifndef ATOM_ALGORITHM_BIGNUMBER_HPP
#define ATOM_ALGORITHM_BIGNUMBER_HPP

#include <cctype>
#include <concepts>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace atom::algorithm {

/**
 * @class BigNumber
 * @brief A class to represent and manipulate large numbers with C++20 features.
 */
class BigNumber {
public:
    // 添加默认构造函数
    constexpr BigNumber() noexcept : isNegative_(false), digits_{0} {}

    /**
     * @brief Constructs a BigNumber from a string_view.
     * @param number The string representation of the number.
     * @throws std::invalid_argument If the string is not a valid number.
     */
    explicit BigNumber(std::string_view number);

    /**
     * @brief Constructs a BigNumber from an integer.
     * @tparam T Integer type that satisfies std::integral concept
     */
    template <std::integral T>
    constexpr explicit BigNumber(T number) noexcept;

    // 支持移动语义
    BigNumber(BigNumber&& other) noexcept = default;
    BigNumber& operator=(BigNumber&& other) noexcept = default;

    // 支持拷贝
    BigNumber(const BigNumber&) = default;
    BigNumber& operator=(const BigNumber&) = default;

    ~BigNumber() = default;

    /**
     * @brief 添加两个BigNumber对象
     * @param other 要相加的另一个BigNumber
     * @return 加法结果
     */
    [[nodiscard]] auto add(const BigNumber& other) const -> BigNumber;

    /**
     * @brief 从一个BigNumber减去另一个
     * @param other 要减去的另一个BigNumber
     * @return 减法结果
     */
    [[nodiscard]] auto subtract(const BigNumber& other) const -> BigNumber;

    /**
     * @brief 乘以另一个BigNumber
     * @param other 要相乘的另一个BigNumber
     * @return 乘法结果
     */
    [[nodiscard]] auto multiply(const BigNumber& other) const -> BigNumber;

    /**
     * @brief 使用Karatsuba算法进行优化乘法运算
     * @param other 要相乘的另一个BigNumber
     * @return 乘法结果
     */
    [[nodiscard]] auto multiplyKaratsuba(const BigNumber& other) const
        -> BigNumber;

    /**
     * @brief 除以另一个BigNumber
     * @param other 作为除数的BigNumber
     * @return 除法结果
     * @throws std::invalid_argument 如果除数为零
     */
    [[nodiscard]] auto divide(const BigNumber& other) const -> BigNumber;

    /**
     * @brief 计算幂
     * @param exponent 指数值
     * @return BigNumber的指数结果
     * @throws std::invalid_argument 如果指数为负数
     */
    [[nodiscard]] auto pow(int exponent) const -> BigNumber;

    /**
     * @brief 获取字符串表示
     * @return 大数的字符串表示
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief 从字符串设置值
     * @param newStr 新的字符串表示
     * @return 更新后的BigNumber引用
     * @throws std::invalid_argument 如果字符串不是有效的数字
     */
    auto setString(std::string_view newStr) -> BigNumber&;

    /**
     * @brief 返回此数的负数
     * @return 取反后的BigNumber
     */
    [[nodiscard]] auto negate() const -> BigNumber;

    /**
     * @brief 移除前导零
     * @return 移除前导零后的BigNumber
     */
    [[nodiscard]] auto trimLeadingZeros() const noexcept -> BigNumber;

    /**
     * @brief 判断两个BigNumber是否相等
     * @param other 要比较的BigNumber
     * @return 是否相等
     */
    [[nodiscard]] constexpr auto equals(const BigNumber& other) const noexcept
        -> bool;

    /**
     * @brief 判断与整数是否相等
     * @tparam T 整数类型
     * @param other 要比较的整数
     * @return 是否相等
     */
    template <std::integral T>
    [[nodiscard]] constexpr auto equals(T other) const noexcept -> bool {
        return equals(BigNumber(other));
    }

    /**
     * @brief 判断与字符串表示的数字是否相等
     * @param other 数字字符串
     * @return 是否相等
     */
    [[nodiscard]] auto equals(std::string_view other) const -> bool {
        return equals(BigNumber(other));
    }

    /**
     * @brief 获取数字位数
     * @return 数字的位数
     */
    [[nodiscard]] constexpr auto digits() const noexcept -> size_t {
        return digits_.size();
    }

    /**
     * @brief 检查是否为负数
     * @return 是否为负数
     */
    [[nodiscard]] constexpr auto isNegative() const noexcept -> bool {
        return isNegative_;
    }

    /**
     * @brief 检查是否为正数或零
     * @return 是否为正数或零
     */
    [[nodiscard]] constexpr auto isPositive() const noexcept -> bool {
        return !isNegative();
    }

    /**
     * @brief 检查是否为偶数
     * @return 是否为偶数
     */
    [[nodiscard]] constexpr auto isEven() const noexcept -> bool {
        return digits_.empty() ? true : (digits_[0] % 2 == 0);
    }

    /**
     * @brief 检查是否为奇数
     * @return 是否为奇数
     */
    [[nodiscard]] constexpr auto isOdd() const noexcept -> bool {
        return !isEven();
    }

    /**
     * @brief 获取绝对值
     * @return 绝对值
     */
    [[nodiscard]] auto abs() const -> BigNumber;

    // 运算符重载
    friend auto operator<<(std::ostream& os,
                           const BigNumber& num) -> std::ostream&;

    friend auto operator+(const BigNumber& b1,
                          const BigNumber& b2) -> BigNumber {
        return b1.add(b2);
    }

    friend auto operator-(const BigNumber& b1,
                          const BigNumber& b2) -> BigNumber {
        return b1.subtract(b2);
    }

    friend auto operator*(const BigNumber& b1,
                          const BigNumber& b2) -> BigNumber {
        return b1.multiply(b2);
    }

    friend auto operator/(const BigNumber& b1,
                          const BigNumber& b2) -> BigNumber {
        return b1.divide(b2);
    }

    friend auto operator^(const BigNumber& b1, int b2) -> BigNumber {
        return b1.pow(b2);
    }

    friend auto operator==(const BigNumber& b1,
                           const BigNumber& b2) noexcept -> bool {
        return b1.equals(b2);
    }

    friend auto operator>(const BigNumber& b1, const BigNumber& b2) -> bool;

    friend auto operator<(const BigNumber& b1, const BigNumber& b2) -> bool {
        return !(b1 == b2) && !(b1 > b2);
    }

    friend auto operator>=(const BigNumber& b1, const BigNumber& b2) -> bool {
        return b1 > b2 || b1 == b2;
    }

    friend auto operator<=(const BigNumber& b1, const BigNumber& b2) -> bool {
        return b1 < b2 || b1 == b2;
    }

    // 复合赋值运算符
    auto operator+=(const BigNumber& other) -> BigNumber&;
    auto operator-=(const BigNumber& other) -> BigNumber&;
    auto operator*=(const BigNumber& other) -> BigNumber&;
    auto operator/=(const BigNumber& other) -> BigNumber&;

    // 前缀和后缀增减运算符
    auto operator++() -> BigNumber&;
    auto operator--() -> BigNumber&;
    auto operator++(int) -> BigNumber;
    auto operator--(int) -> BigNumber;

    /**
     * @brief 访问特定位置的数字
     * @param index 要访问的索引
     * @return 该位置的数字
     * @throws std::out_of_range 如果索引超出范围
     */
    [[nodiscard]] constexpr auto at(size_t index) const -> uint8_t;

    /**
     * @brief 下标运算符
     * @param index 要访问的索引
     * @return 该位置的数字
     * @throws std::out_of_range 如果索引超出范围
     */
    auto operator[](size_t index) const -> uint8_t { return at(index); }

    // 添加并行计算支持
    [[nodiscard]] auto parallelMultiply(const BigNumber& other) const
        -> BigNumber;

private:
    bool isNegative_;              ///< 是否为负数
    std::vector<uint8_t> digits_;  ///< 数字存储，个位在前，高位在后

    /**
     * @brief 验证字符串是否为有效数字
     * @param str 要验证的字符串
     * @throws std::invalid_argument 如果字符串不是有效的数字
     */
    static void validateString(std::string_view str);

    void validate() const;

    /**
     * @brief 从字符串初始化数字向量
     * @param str 数字字符串
     */
    void initFromString(std::string_view str);

    /**
     * @brief Karatsuba乘法算法的递归实现
     * @param a 第一个BigNumber的数据
     * @param b 第二个BigNumber的数据
     * @return 计算结果
     */
    static std::vector<uint8_t> karatsubaMultiply(std::span<const uint8_t> a,
                                                  std::span<const uint8_t> b);
};

// 整数类型的构造函数实现
template <std::integral T>
constexpr BigNumber::BigNumber(T number) noexcept : isNegative_(number < 0) {
    // 处理0的特殊情况
    if (number == 0) {
        digits_.push_back(0);
        return;
    }

    // 转换为正数处理
    auto absNumber =
        static_cast<std::make_unsigned_t<T>>(number < 0 ? -number : number);

    // 逐位提取数字
    while (absNumber > 0) {
        digits_.push_back(static_cast<uint8_t>(absNumber % 10));
        absNumber /= 10;
    }
}

constexpr auto BigNumber::equals(const BigNumber& other) const noexcept
    -> bool {
    return isNegative_ == other.isNegative_ && digits_ == other.digits_;
}

constexpr auto BigNumber::at(size_t index) const -> uint8_t {
    if (index >= digits_.size()) {
        throw std::out_of_range("Index out of range in BigNumber::at");
    }
    return digits_[index];
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_BIGNUMBER_HPP