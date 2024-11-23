#include "atom/type/iter.hpp"

#include <iostream>
#include <vector>
#include <list>
#include <algorithm>

int main() {
    // Example container
    std::vector<int> vec = {1, 2, 3, 4, 5};

    // PointerIterator example
    auto ptrRange = makePointerRange(vec.begin(), vec.end());
    std::cout << "PointerIterator example:" << std::endl;
    for (auto it = ptrRange.first; it != ptrRange.second; ++it) {
        std::cout << **it << " ";
    }
    std::cout << std::endl;

    // EarlyIncIterator example
    auto earlyIncIt = makeEarlyIncIterator(vec.begin());
    std::cout << "EarlyIncIterator example:" << std::endl;
    for (int i = 0; i < vec.size(); ++i) {
        std::cout << *earlyIncIt << " ";
        ++earlyIncIt;
    }
    std::cout << std::endl;

    // TransformIterator example
    auto transformIt = makeTransformIterator(vec.begin(), [](int x) { return x * 2; });
    std::cout << "TransformIterator example:" << std::endl;
    for (int i = 0; i < vec.size(); ++i) {
        std::cout << *transformIt << " ";
        ++transformIt;
    }
    std::cout << std::endl;

    /*
    TODO: Fix this
    // FilterIterator example
    auto filterIt = makeFilterIterator(vec.begin(), vec.end(), [](int x) { return x % 2 == 0; });
    std::cout << "FilterIterator example:" << std::endl;
    for (; filterIt != vec.end(); ++filterIt) {
        std::cout << *filterIt << " ";
    }
    std::cout << std::endl;
    */

    // ReverseIterator example
    auto reverseIt = ReverseIterator(vec.end());
    std::cout << "ReverseIterator example:" << std::endl;
    for (int i = 0; i < vec.size(); ++i) {
        --reverseIt;
        std::cout << *reverseIt << " ";
    }
    std::cout << std::endl;

    // ZipIterator example
    std::vector<int> vec2 = {6, 7, 8, 9, 10};
    auto zipIt = makeZipIterator(vec.begin(), vec2.begin());
    std::cout << "ZipIterator example:" << std::endl;
    for (int i = 0; i < vec.size(); ++i) {
        auto [a, b] = *zipIt;
        std::cout << "(" << a << ", " << b << ") ";
        ++zipIt;
    }
    std::cout << std::endl;

    // processContainer example
    std::list<int> lst = {1, 2, 3, 4, 5};
    std::cout << "processContainer example before:" << std::endl;
    for (const auto& val : lst) {
        std::cout << val << " ";
    }
    std::cout << std::endl;
    processContainer(lst);
    std::cout << "processContainer example after:" << std::endl;
    for (const auto& val : lst) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    return 0;
}