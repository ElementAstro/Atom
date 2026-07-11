/**
 * @file static_string.cpp
 * @brief Demonstrates atom::type::StaticString<N> (fixed-capacity string).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string_view>

#include "../atom/type/static_string.hpp"

using atom::type::StaticString;

int main() {
    StaticString<16> s("Hello");
    std::cout << "=== Basics ===\n";
    std::cout << "  value=" << s.data() << " size=" << s.size()
              << " capacity=" << s.capacity() << '\n';
    std::cout << "  empty=" << s.empty() << '\n';

    std::cout << "\n=== append ===\n";
    s.append(", ");
    s.append("world");
    std::cout << "  value=" << s.data() << " size=" << s.size() << '\n';

    std::cout << "\n=== as string_view + find ===\n";
    std::string_view sv = s;
    std::cout << "  view=\"" << sv << "\"\n";
    std::cout << "  find(\"world\")=" << s.find("world") << '\n';

    std::cout << "\n=== resize ===\n";
    s.resize(5);
    std::cout << "  after resize(5): \"" << std::string_view(s.data(), s.size())
              << "\"\n";
    return 0;
}
