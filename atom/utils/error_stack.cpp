/*
 * error_stack.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: Error Stack

**************************************************/

#include "error_stack.hpp"

#include <algorithm>
#include <ctime>
#include <execution>
#include <sstream>

#include "atom/log/loguru.hpp"
#include "atom/utils/time.hpp"

namespace atom::error {

std::ostream &operator<<(std::ostream &os, const ErrorInfo &error) {
    try {
        os << "{" << R"("errorMessage": ")" << error.errorMessage << "\","
           << R"("moduleName": ")" << error.moduleName << "\","
           << R"("functionName": ")" << error.functionName << "\","
           << "\"line\": " << error.line << "," << R"("fileName": ")"
           << error.fileName << "\"," << "\"timestamp\": \""
           << atom::utils::timeStampToString(error.timestamp) << "\","
           << "\"uuid\": \"" << error.uuid << "\"" << "}";
    } catch (const std::exception &e) {
        os << "{\"error\": \"Failed to format error info: " << e.what()
           << "\"}";
    }
    return os;
}

std::string operator<<([[maybe_unused]] const std::string &str,
                       const ErrorInfo &error) {
    try {
        std::stringstream ss;
        ss << "{" << R"("errorMessage": ")" << error.errorMessage << "\","
           << R"("moduleName": ")" << error.moduleName << "\","
           << R"("functionName": ")" << error.functionName << "\","
           << "\"line\": " << error.line << "," << R"("fileName": ")"
           << error.fileName << "\"," << R"("timestamp": ")"
           << atom::utils::timeStampToString(error.timestamp) << "\","
           << R"("uuid": ")" << error.uuid << "\"" << "}";
        return ss.str();
    } catch (const std::exception &e) {
        return std::string("{\"error\": \"Failed to format error info: ") +
               e.what() + "\"}";
    }
}

auto ErrorStack::createShared() -> std::shared_ptr<ErrorStack> {
    return std::make_shared<ErrorStack>();
}

auto ErrorStack::createUnique() -> std::unique_ptr<ErrorStack> {
    return std::make_unique<ErrorStack>();
}

void ErrorStack::clearFilteredModules() {
    std::lock_guard lock(mutex_);
    filteredModules_.clear();
}

void ErrorStack::printFilteredErrorStack() const {
    try {
        std::lock_guard lock(mutex_);

        for (const auto &error : errorStack_) {
            if (!std::ranges::contains(filteredModules_, error.moduleName)) {
                LOG_F(ERROR, "{}", error.errorMessage);
            }
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to print error stack: {}", e.what());
    }
}

auto ErrorStack::getFilteredErrorsByModule(std::string_view moduleName) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size());  // Pre-allocate for efficiency

        std::string moduleNameStr(moduleName);
        std::copy_if(errorStack_.begin(), errorStack_.end(),
                     std::back_inserter(errors),
                     [&moduleNameStr, this](const ErrorInfo &error) {
                         return error.moduleName == moduleNameStr &&
                                !std::ranges::contains(filteredModules_,
                                                       error.moduleName);
                     });

        return errors;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to get filtered errors: {}", e.what());
        return {};
    }
}

auto ErrorStack::getCompressedErrors() const -> std::string {
    try {
        std::lock_guard lock(mutex_);

        if (compressedErrorStack_.empty()) {
            return "";
        }

        std::stringstream compressedErrors;

        for (const auto &error : compressedErrorStack_) {
            compressedErrors << error.errorMessage << " ";
        }

        return compressedErrors.str();
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to get compressed errors: {}", e.what());
        return "";
    }
}

void ErrorStack::updateCompressedErrors() {
    try {
        // No need for mutex lock as this is called from methods that already
        // have the lock
        compressedErrorStack_.clear();

        // Reserve space based on the original stack size for better performance
        compressedErrorStack_.reserve(errorStack_.size());

        for (const auto &error : errorStack_) {
            auto iter = std::ranges::find_if(
                compressedErrorStack_,
                [&error](const ErrorInfo &compressedError) {
                    return compressedError.errorMessage == error.errorMessage &&
                           compressedError.moduleName == error.moduleName;
                });

            if (iter != compressedErrorStack_.end()) {
                iter->timestamp = error.timestamp;
            } else {
                compressedErrorStack_.push_back(error);
            }
        }

        sortCompressedErrorStack();
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to update compressed errors: {}", e.what());
        // Attempt to recover by clearing
        compressedErrorStack_.clear();
    }
}

void ErrorStack::sortCompressedErrorStack() {
    try {
        // Using parallel execution policy for large collections
        if (compressedErrorStack_.size() > 1000) {
            std::sort(std::execution::par_unseq, compressedErrorStack_.begin(),
                      compressedErrorStack_.end(),
                      [](const ErrorInfo &error1, const ErrorInfo &error2) {
                          return error1.timestamp > error2.timestamp;
                      });
        } else {
            std::sort(compressedErrorStack_.begin(),
                      compressedErrorStack_.end(),
                      [](const ErrorInfo &error1, const ErrorInfo &error2) {
                          return error1.timestamp > error2.timestamp;
                      });
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to sort compressed error stack: {}", e.what());
    }
}

bool ErrorStack::isEmpty() const noexcept {
    std::lock_guard lock(mutex_);
    return errorStack_.empty();
}

size_t ErrorStack::size() const noexcept {
    std::lock_guard lock(mutex_);
    return errorStack_.size();
}

std::optional<ErrorInfo> ErrorStack::getLatestError() const {
    std::lock_guard lock(mutex_);
    if (errorStack_.empty()) {
        return std::nullopt;
    }

    auto latest = std::max_element(errorStack_.begin(), errorStack_.end(),
                                   [](const ErrorInfo &a, const ErrorInfo &b) {
                                       return a.timestamp < b.timestamp;
                                   });

    return *latest;
}

void ErrorStack::clear() noexcept {
    try {
        std::lock_guard lock(mutex_);
        errorStack_.clear();
        compressedErrorStack_.clear();
    } catch (...) {
        // Ensure we don't throw from a noexcept function
    }
}

}  // namespace atom::error
