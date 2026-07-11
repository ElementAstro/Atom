/**
 * @file pointer.cpp
 * @brief Demonstrates atom::type::PointerSentinel<T> (a variant pointer holder
 *        over raw / shared / unique / weak pointers with safe invocation).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <memory>

#include "../atom/type/pointer.hpp"

using atom::type::PointerSentinel;

struct Widget {
    int value = 0;
    int doubled() const { return value * 2; }
};

int main() {
    std::cout << "=== From shared_ptr ===\n";
    auto sp = std::make_shared<Widget>(Widget{21});
    PointerSentinel<Widget> sentinel(sp);
    std::cout << "  is_valid = " << sentinel.is_valid() << '\n';
    std::cout << "  get()->value = " << sentinel.get()->value << '\n';

    std::cout << "\n=== invoke a member function ===\n";
    std::cout << "  invoke(&Widget::doubled) = "
              << sentinel.invoke(&Widget::doubled) << '\n';

    std::cout << "\n=== apply a callable ===\n";
    int v = sentinel.apply([](Widget* w) { return w->value + 1; });
    std::cout << "  apply(+1) = " << v << '\n';
    return 0;
}
