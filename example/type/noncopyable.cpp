/**
 * @file noncopyable.cpp
 * @brief Demonstrates atom::type::NonCopyable (a base that deletes copy ops).
 *
 * Rewritten from scratch: the previous revision was corrupted and did not
 * compile.
 */

#include <iostream>
#include <utility>

#include "../atom/type/noncopyable.hpp"

// A resource handle that must not be copied (but may be moved).
class UniqueResource : public atom::type::NonCopyable {
public:
    explicit UniqueResource(int id) : id_(id) {}
    UniqueResource(UniqueResource&& other) noexcept
        : id_(std::exchange(other.id_, -1)) {}
    int id() const { return id_; }

private:
    int id_;
};

int main() {
    UniqueResource a(7);
    std::cout << "=== NonCopyable ===\n";
    std::cout << "  a.id() = " << a.id() << '\n';

    // Copying is a compile error:
    //   UniqueResource b = a;   // <- deleted copy constructor

    UniqueResource b = std::move(a);  // moving is allowed
    std::cout << "  after move: b.id()=" << b.id() << " a.id()=" << a.id()
              << '\n';
    return 0;
}
