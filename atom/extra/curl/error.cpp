#include "error.hpp"

namespace atom::extra::curl {

Error::Error(CURLcode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

Error::Error(CURLMcode code, std::string message)
    : std::runtime_error(std::move(message)),
      code_(static_cast<CURLcode>(code)),
      multi_code_(code) {}

CURLcode Error::code() const noexcept { return code_; }

std::optional<CURLMcode> Error::multi_code() const noexcept {
    return multi_code_;
}

}  // namespace atom::extra::curl