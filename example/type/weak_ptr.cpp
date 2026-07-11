/**
 * @file weak_ptr.cpp
 * @brief Demonstrates atom::type::EnhancedWeakPtr<T>.
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <memory>

#include "../atom/type/weak_ptr.hpp"

using atom::type::EnhancedWeakPtr;

int main() {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    std::cout << "=== While the shared_ptr is alive ===\n";
    std::cout << "  expired = " << weak.expired() << '\n';
    if (auto locked = weak.lock()) {
        std::cout << "  locked value = " << *locked << '\n';
    }

    std::cout << "\n=== withLock helper ===\n";
    auto result = weak.withLock([](int& v) { return v + 1; });
    if (result.has_value()) {
        std::cout << "  withLock(+1) = " << *result << '\n';
    }

    std::cout << "\n=== After the shared_ptr is released ===\n";
    shared.reset();
    std::cout << "  expired = " << weak.expired() << '\n';
    std::cout << "  lock() truthy = " << static_cast<bool>(weak.lock()) << '\n';
    return 0;
}
