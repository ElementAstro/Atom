#ifndef ATOM_TYPE_COMPAT_EXPECTED_HPP
#define ATOM_TYPE_COMPAT_EXPECTED_HPP

#include <string>

// Compatibility layer for expected/unexpected
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
  #include <expected>
  namespace atom::type {
    template <typename T, typename E = std::string>
    using expected = std::expected<T, E>;
    template <typename E>
    using unexpected = std::unexpected<E>;
  }
#else
  #include "atom/type/expected.hpp"
  namespace atom::type {
    template <typename T, typename E = std::string>
    using expected = ::atom::type::expected<T, E>;
    template <typename E>
    using unexpected = ::atom::type::unexpected<E>;
  }
#endif

#endif // ATOM_TYPE_COMPAT_EXPECTED_HPP
