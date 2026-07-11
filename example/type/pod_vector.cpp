/**
 * @file pod_vector.cpp
 * @brief Demonstrates atom::type::PodVector<T> (a vector for plain-old-data).
 *
 * Rewritten from scratch: the previous revision was corrupted (comments glued
 * into code) and did not compile.
 */

#include <iostream>

#include "../atom/type/pod_vector.hpp"

using atom::type::PodVector;

int main() {
    std::cout << "=== Fill and read ===\n";
    PodVector<int> v;
    for (int i = 1; i <= 5; ++i) {
        v.pushBack(i * 10);
    }
    std::cout << "  size = " << v.size() << '\n';
    std::cout << "  elements:";
    for (int i = 0; i < v.size(); ++i) {
        std::cout << ' ' << v[i];
    }
    std::cout << '\n';

    std::cout << "\n=== Range-for ===\n";
    long sum = 0;
    for (int x : v) {
        sum += x;
    }
    std::cout << "  sum = " << sum << '\n';

    std::cout << "\n=== popBack ===\n";
    v.popBack();
    std::cout << "  size after popBack = " << v.size() << '\n';

    return 0;
}
