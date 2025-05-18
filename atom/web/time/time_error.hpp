/*
 * time_error.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Time Error Definitions

**************************************************/

#ifndef ATOM_WEB_TIME_ERROR_HPP
#define ATOM_WEB_TIME_ERROR_HPP

#include <string>
#include <system_error>


namespace atom::web {

/**
 * @enum TimeError
 * @brief Error codes for time operations
 */
enum class TimeError {
    None,
    InvalidParameter,
    PermissionDenied,
    NetworkError,
    SystemError,
    TimeoutError,
    NotSupported
};

namespace detail {
// 自定义错误类型和错误代码
class time_error_category : public std::error_category {
public:
    const char* name() const noexcept override { return "time_error"; }

    std::string message(int ev) const override {
        switch (static_cast<TimeError>(ev)) {
            case TimeError::None:
                return "Success";
            case TimeError::InvalidParameter:
                return "Invalid parameter";
            case TimeError::PermissionDenied:
                return "Permission denied";
            case TimeError::NetworkError:
                return "Network error";
            case TimeError::SystemError:
                return "System error";
            case TimeError::TimeoutError:
                return "Operation timed out";
            case TimeError::NotSupported:
                return "Operation not supported";
            default:
                return "Unknown error";
        }
    }
};

inline const std::error_category& time_category() {
    static time_error_category category;
    return category;
}
}  // namespace detail

inline std::error_code make_error_code(TimeError e) {
    return {static_cast<int>(e), detail::time_category()};
}

}  // namespace atom::web

#endif  // ATOM_WEB_TIME_ERROR_HPP
