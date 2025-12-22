/*
 * scoped_context.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Scoped error context implementation

**************************************************/

#include "scoped_context.hpp"

namespace atom::error {

ScopedErrorContext::ScopedErrorContext(int errorCode,
                                       const std::string& message)
    : context_(ErrorContext::create(errorCode, message)) {}

ScopedErrorContext::ScopedErrorContext(std::shared_ptr<ErrorContext> context)
    : context_(std::move(context)) {}

ScopedErrorContext::~ScopedErrorContext() = default;

ScopedErrorContext::ScopedErrorContext(ScopedErrorContext&& other) noexcept =
    default;

ScopedErrorContext& ScopedErrorContext::operator=(
    ScopedErrorContext&& other) noexcept = default;

auto ScopedErrorContext::getContext() const -> std::shared_ptr<ErrorContext> {
    return context_;
}

auto ScopedErrorContext::operator->() const -> ErrorContext* {
    return context_.get();
}

ScopedErrorContext::operator bool() const noexcept {
    return context_ != nullptr;
}

ScopedErrorContext& ScopedErrorContext::setUserData(const std::string& key,
                                                    std::any value) {
    if (context_) [[likely]] {
        context_->setUserData(key, std::move(value));
    }
    return *this;
}

ScopedErrorContext& ScopedErrorContext::addTag(const std::string& tag) {
    if (context_) [[likely]] {
        context_->addTag(tag);
    }
    return *this;
}

ScopedErrorContext& ScopedErrorContext::setCorrelationId(
    const std::string& correlationId) {
    if (context_) [[likely]] {
        context_->setCorrelationId(correlationId);
    }
    return *this;
}

}  // namespace atom::error
