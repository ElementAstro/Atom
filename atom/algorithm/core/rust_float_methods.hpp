// rust_float_methods.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>

#include "rust_error.hpp"
#include "rust_option.hpp"
#include "rust_result.hpp"
#include "rust_types.hpp"

#undef NAN

namespace atom::algorithm {

template <typename Float,
          typename = std::enable_if_t<std::is_floating_point_v<Float>>>
class FloatMethods {
public:
    static constexpr Float INFINITY_VAL =
        std::numeric_limits<Float>::infinity();
    static constexpr Float NEG_INFINITY =
        -std::numeric_limits<Float>::infinity();
    static constexpr Float NAN = std::numeric_limits<Float>::quiet_NaN();
    static constexpr Float MIN = std::numeric_limits<Float>::lowest();
    static constexpr Float MAX = std::numeric_limits<Float>::max();
    static constexpr Float EPSILON = std::numeric_limits<Float>::epsilon();
    static constexpr Float PI = static_cast<Float>(3.14159265358979323846);
    static constexpr Float TAU = PI * 2;
    static constexpr Float E = static_cast<Float>(2.71828182845904523536);
    static constexpr Float SQRT_2 = static_cast<Float>(1.41421356237309504880);
    static constexpr Float LN_2 = static_cast<Float>(0.69314718055994530942);
    static constexpr Float LN_10 = static_cast<Float>(2.30258509299404568402);

    template <typename ToType>
    static Option<ToType> try_into(Float value) {
        if (std::is_integral_v<ToType>) {
            if (value <
                    static_cast<Float>(std::numeric_limits<ToType>::min()) ||
                value >
                    static_cast<Float>(std::numeric_limits<ToType>::max()) ||
                std::isnan(value)) {
                return Option<ToType>::none();
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        } else if (std::is_floating_point_v<ToType>) {
            if (value < std::numeric_limits<ToType>::lowest() ||
                value > std::numeric_limits<ToType>::max()) {
                return Option<ToType>::none();
            }
            return Option<ToType>::some(static_cast<ToType>(value));
        }
        return Option<ToType>::none();
    }

    static bool is_nan(Float x) { return std::isnan(x); }

    static bool is_infinite(Float x) { return std::isinf(x); }

    static bool is_finite(Float x) { return std::isfinite(x); }

    static bool is_normal(Float x) { return std::isnormal(x); }

    static bool is_subnormal(Float x) {
        return std::fpclassify(x) == FP_SUBNORMAL;
    }

    static bool is_sign_positive(Float x) { return std::signbit(x) == 0; }

    static bool is_sign_negative(Float x) { return std::signbit(x) != 0; }

    static Float abs(Float x) { return std::abs(x); }

    static Float floor(Float x) { return std::floor(x); }

    static Float ceil(Float x) { return std::ceil(x); }

    static Float round(Float x) { return std::round(x); }

    static Float trunc(Float x) { return std::trunc(x); }

    static Float fract(Float x) { return x - std::floor(x); }

    static Float sqrt(Float x) { return std::sqrt(x); }

    static Float cbrt(Float x) { return std::cbrt(x); }

    static Float exp(Float x) { return std::exp(x); }

    static Float exp2(Float x) { return std::exp2(x); }

    static Float ln(Float x) { return std::log(x); }

    static Float log2(Float x) { return std::log2(x); }

    static Float log10(Float x) { return std::log10(x); }

    static Float log(Float x, Float base) {
        return std::log(x) / std::log(base);
    }

    static Float pow(Float x, Float y) { return std::pow(x, y); }

    static Float sin(Float x) { return std::sin(x); }

    static Float cos(Float x) { return std::cos(x); }

    static Float tan(Float x) { return std::tan(x); }

    static Float asin(Float x) { return std::asin(x); }

    static Float acos(Float x) { return std::acos(x); }

    static Float atan(Float x) { return std::atan(x); }

    static Float atan2(Float y, Float x) { return std::atan2(y, x); }

    static Float sinh(Float x) { return std::sinh(x); }

    static Float cosh(Float x) { return std::cosh(x); }

    static Float tanh(Float x) { return std::tanh(x); }

    static Float asinh(Float x) { return std::asinh(x); }

    static Float acosh(Float x) { return std::acosh(x); }

    static Float atanh(Float x) { return std::atanh(x); }

    static bool approx_eq(Float a, Float b, Float epsilon = EPSILON) {
        if (a == b)
            return true;

        Float diff = abs(a - b);
        if (a == 0 || b == 0 || diff < std::numeric_limits<Float>::min()) {
            return diff < epsilon;
        }

        return diff / (abs(a) + abs(b)) < epsilon;
    }

    static int total_cmp(Float a, Float b) {
        if (is_nan(a) && is_nan(b))
            return 0;
        if (is_nan(a))
            return 1;
        if (is_nan(b))
            return -1;

        if (a < b)
            return -1;
        if (a > b)
            return 1;
        return 0;
    }

    static Float min(Float a, Float b) {
        if (is_nan(a))
            return b;
        if (is_nan(b))
            return a;
        return a < b ? a : b;
    }

    static Float max(Float a, Float b) {
        if (is_nan(a))
            return b;
        if (is_nan(b))
            return a;
        return a > b ? a : b;
    }

    static Float clamp(Float value, Float min, Float max) {
        if (is_nan(value))
            return min;
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }

    static std::string to_string(Float value, int precision = 6) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << value;
        return oss.str();
    }

    static std::string to_exp_string(Float value, int precision = 6) {
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(precision) << value;
        return oss.str();
    }

    static Result<Float> from_str(const std::string& s) {
        try {
            size_t pos;
            if constexpr (std::is_same_v<Float, float>) {
                float val = std::stof(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(val);
            } else if constexpr (std::is_same_v<Float, double>) {
                double val = std::stod(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(val);
            } else {
                long double val = std::stold(s, &pos);
                if (pos != s.length()) {
                    return Result<Float>::err(ErrorKind::ParseFloatError,
                                              "Failed to parse entire string");
                }
                return Result<Float>::ok(static_cast<Float>(val));
            }
        } catch (const std::exception& e) {
            return Result<Float>::err(ErrorKind::ParseFloatError, e.what());
        }
    }

    static Float random(Float min = 0.0, Float max = 1.0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        if (min > max) {
            std::swap(min, max);
        }

        std::uniform_real_distribution<Float> dist(min, max);
        return dist(gen);
    }

    static std::tuple<Float, Float> modf(Float x) {
        Float int_part;
        Float frac_part = std::modf(x, &int_part);
        return {int_part, frac_part};
    }

    static Float copysign(Float x, Float y) { return std::copysign(x, y); }

    static Float next_up(Float x) { return std::nextafter(x, INFINITY_VAL); }

    static Float next_down(Float x) { return std::nextafter(x, NEG_INFINITY); }

    static Float ulp(Float x) { return next_up(x) - x; }

    static Float to_radians(Float degrees) { return degrees * PI / 180.0f; }

    static Float to_degrees(Float radians) { return radians * 180.0f / PI; }

    static Float hypot(Float x, Float y) { return std::hypot(x, y); }

    static Float hypot(Float x, Float y, Float z) {
        return std::sqrt(x * x + y * y + z * z);
    }

    static Float lerp(Float a, Float b, Float t) { return a + t * (b - a); }

    static Float sign(Float x) {
        if (x > 0)
            return 1.0;
        if (x < 0)
            return -1.0;
        return 0.0;
    }
};

class F32 : public FloatMethods<f32> {
public:
    static Result<f32> from_str(const std::string& s) {
        return FloatMethods<f32>::from_str(s);
    }
};

class F64 : public FloatMethods<f64> {
public:
    static Result<f64> from_str(const std::string& s) {
        return FloatMethods<f64>::from_str(s);
    }
};

}  // namespace atom::algorithm
