/**
 * @file args.cpp
 * @brief Demonstrates atom::type::Args (a heterogeneous key→value bag).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <string>

#include "../atom/type/args.hpp"

using atom::type::Args;

int main() {
    Args args;
    std::cout << "=== set / get<T> ===\n";
    args.set("count", 42);
    args.set("name", std::string("widget"));
    args.set("ratio", 3.14);

    std::cout << "  count = " << args.get<int>("count") << '\n';
    std::cout << "  name  = " << args.get<std::string>("name") << '\n';
    std::cout << "  ratio = " << args.get<double>("ratio") << '\n';

    std::cout << "\n=== contains ===\n";
    std::cout << "  contains(\"name\")    = " << args.contains("name") << '\n';
    std::cout << "  contains(\"missing\") = " << args.contains("missing")
              << '\n';
    return 0;
}
