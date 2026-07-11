/**
 * @file trackable.cpp
 * @brief Demonstrates atom::type::Trackable<T> (an observable value).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/trackable.hpp"

using atom::type::Trackable;

int main() {
    Trackable<int> temperature(20);

    std::cout << "=== subscribe to changes ===\n";
    temperature.subscribe([](const int& newValue, const int& oldValue) {
        std::cout << "  changed: " << oldValue << " -> " << newValue << '\n';
    });

    std::cout << "  initial get() = " << temperature.get() << '\n';
    temperature = 25;  // triggers the observer
    temperature = 30;  // triggers the observer
    std::cout << "  final get()   = " << temperature.get() << '\n';
    return 0;
}
