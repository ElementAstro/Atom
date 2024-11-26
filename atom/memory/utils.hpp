#ifndef ATOM_MEMORY_UTILS_HPP
#define ATOM_MEMORY_UTILS_HPP

#include <memory>
#include <type_traits>
#include <utility>

namespace atom::memory {
template <typename T, typename... Args>
struct IsConstructible {
    static constexpr bool value = std::is_constructible_v<T, Args...>;
};

template <typename T, typename... Args>
using ConstructorArguments_t =
    std::enable_if_t<IsConstructible<T, Args...>::value, std::shared_ptr<T>>;

template <typename T, typename... Args>
auto makeShared(Args&&... args) -> ConstructorArguments_t<T, Args...> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

template <typename T, typename... Args>
auto makeUnique(Args&&... args) -> ConstructorArguments_t<T, Args...> {
    if constexpr (IsConstructible<T, Args...>::value) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    } else {
        static_assert(IsConstructible<T, Args...>::value,
                      "Arguments do not match any constructor of the type T");
        return nullptr;
    }
}

}  // namespace atom::memory

#endif // ATOM_MEMORY_UTILS_HPP