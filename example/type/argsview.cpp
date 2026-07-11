/**
 * @file argsview.cpp
 * @brief Demonstrates atom::type::ArgsView<Args...> (a typed view over a fixed
 *        set of arguments).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/argsview.hpp"

using atom::type::ArgsView;

int main() {
    ArgsView view(1, std::string("two"), 3.0);  // CTAD

    std::cout << "=== size + positional get<I> ===\n";
    std::cout << "  size      = " << view.size() << '\n';
    std::cout << "  get<0>()  = " << view.get<0>() << '\n';
    std::cout << "  get<1>()  = " << view.get<1>() << '\n';
    std::cout << "  get<2>()  = " << view.get<2>() << '\n';
    std::cout << "  empty()   = " << view.empty() << '\n';
    return 0;
}
