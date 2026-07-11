/**
 * @file small_list.cpp
 * @brief Demonstrates atom::type::SmallList<T> (a doubly-linked list).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>

#include "../atom/type/small_list.hpp"

using atom::type::SmallList;

int main() {
    SmallList<int> list;
    std::cout << "=== pushBack / pushFront ===\n";
    list.pushBack(2);
    list.pushBack(3);
    list.pushFront(1);
    std::cout << "  size=" << list.size() << '\n';

    std::cout << "  contents:";
    for (int x : list) {
        std::cout << ' ' << x;
    }
    std::cout << "\n  front=" << list.front() << " back=" << list.back()
              << '\n';

    std::cout << "\n=== popFront / popBack ===\n";
    list.popFront();
    list.popBack();
    std::cout << "  size=" << list.size() << " remaining front=" << list.front()
              << '\n';
    return 0;
}
