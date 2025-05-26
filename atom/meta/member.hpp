#ifndef ATOM_FUNCTION_MEMBER_HPP
#define ATOM_FUNCTION_MEMBER_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include "atom/type/expected.hpp"

#if ATOM_ENABLE_DEBUG
#include <iostream>
#endif

namespace atom::meta {

/**
 * @brief Exception thrown when member pointer operations fail
 */
class member_pointer_error : public std::runtime_error {
public:
    explicit member_pointer_error(
        const std::string& msg,
        std::source_location loc = std::source_location::current())
        : std::runtime_error(
              std::format("{}:{}: {}", loc.file_name(), loc.line(), msg)) {}
};

/**
 * @brief Concept for member pointers
 */
template <typename T>
concept member_pointer = std::is_member_pointer_v<T>;

/**
 * @brief Gets the offset of a member within a structure
 */
template <typename T, typename M>
consteval std::size_t member_offset(M T::* member) noexcept {
    return static_cast<std::size_t>(reinterpret_cast<std::ptrdiff_t>(
        &(static_cast<T const volatile*>(nullptr)->*member)));
}

/**
 * @brief Gets the size of a member within a structure
 */
template <typename T, typename M>
consteval std::size_t member_size(M T::* member) noexcept {
    return sizeof((static_cast<T const volatile*>(nullptr)->*member));
}

/**
 * @brief Gets the total size of a structure
 */
template <typename T>
consteval std::size_t struct_size() noexcept {
    return sizeof(T);
}

/**
 * @brief Gets the alignment of a member within a structure
 */
template <typename T, typename M>
consteval std::size_t member_alignment(
    [[maybe_unused]] M T::* member) noexcept {
    return alignof(M);
}

#if ATOM_ENABLE_DEBUG
/**
 * @brief Prints the detailed information of all members in a structure
 */
template <typename T, typename... Members>
void print_member_info(const std::string& struct_name,
                       Members T::*... members) {
    std::cout << "Structure: " << struct_name << " (Size: " << struct_size<T>()
              << ", Alignment: " << alignof(T) << ")\n";

    (...,
     (std::cout << std::format("  Member at offset {}: size {}, alignment {}\n",
                               member_offset(members), member_size(members),
                               member_alignment<T>(members))));
}
#endif

/**
 * @brief Validates that a member pointer is not null
 */
template <typename T, typename M>
constexpr void validate_member_ptr(M T::* member_ptr,
                                   std::string_view operation) {
    if (member_ptr == nullptr) {
        throw member_pointer_error(
            std::format("Invalid member pointer in {}", operation));
    }
}

/**
 * @brief Validates that a pointer is not null
 */
template <typename T>
constexpr void validate_pointer(const T* ptr, std::string_view operation) {
    if (ptr == nullptr) {
        throw member_pointer_error(
            std::format("Null pointer in {}", operation));
    }
}

/**
 * @brief Calculates the offset of a member within a structure
 * @throws member_pointer_error if member_ptr is null
 */
template <typename T, typename MemberType>
constexpr std::size_t offset_of(MemberType T::* member_ptr) {
    validate_member_ptr(member_ptr, "offset_of");
    return static_cast<std::size_t>(reinterpret_cast<std::ptrdiff_t>(
        &(static_cast<T const volatile*>(nullptr)->*member_ptr)));
}

/**
 * @brief Type-safe container_of implementation using type::expected for error
 * handling
 */
template <typename Container, typename T, member_pointer MemberPtr>
type::expected<Container*, member_pointer_error> safe_container_of(
    T* ptr, MemberPtr Container::* member_ptr) noexcept {
    try {
        if (ptr == nullptr) {
            return type::unexpected(
                member_pointer_error("Null pointer in safe_container_of"));
        }
        if (member_ptr == nullptr) {
            return type::unexpected(member_pointer_error(
                "Null member pointer in safe_container_of"));
        }

        std::size_t offset = offset_of(member_ptr);
        return reinterpret_cast<Container*>(reinterpret_cast<std::byte*>(ptr) -
                                            offset);
    } catch (const member_pointer_error& e) {
        return type::unexpected(e);
    } catch (...) {
        return type::unexpected(
            member_pointer_error("Unknown error in safe_container_of"));
    }
}

/**
 * @brief Converts a member pointer to the containing object pointer
 * @throws member_pointer_error if validation fails
 */
template <typename T, typename MemberType>
T* pointer_to_object(MemberType T::* member_ptr,
                     MemberType* member_ptr_address) {
    static_assert(std::is_member_pointer_v<decltype(member_ptr)>,
                  "member_ptr must be a member pointer");

    validate_member_ptr(member_ptr, "pointer_to_object");
    validate_pointer(member_ptr_address, "pointer_to_object");

    std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(member_ptr_address);
    std::uintptr_t object_address = address - offset_of(member_ptr);
    return reinterpret_cast<T*>(object_address);
}

/**
 * @brief Converts a const member pointer to the containing const object pointer
 * @throws member_pointer_error if validation fails
 */
template <typename T, typename MemberType>
const T* pointer_to_object(MemberType T::* member_ptr,
                           const MemberType* member_ptr_address) {
    static_assert(std::is_member_pointer_v<decltype(member_ptr)>,
                  "member_ptr must be a member pointer");

    validate_member_ptr(member_ptr, "pointer_to_object");
    validate_pointer(member_ptr_address, "pointer_to_object");

    std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(member_ptr_address);
    std::uintptr_t object_address = address - offset_of(member_ptr);
    return reinterpret_cast<const T*>(object_address);
}

/**
 * @brief Converts a member pointer to the containing container pointer
 * @throws member_pointer_error if validation fails
 */
template <typename Container, typename T, typename MemberPtr>
Container* container_of(T* ptr, MemberPtr Container::* member_ptr) {
    validate_pointer(ptr, "container_of");
    validate_member_ptr(member_ptr, "container_of");

    std::size_t offset = offset_of(member_ptr);
    return reinterpret_cast<Container*>(reinterpret_cast<std::byte*>(ptr) -
                                        offset);
}

/**
 * @brief Converts a const member pointer to the containing const container
 * pointer
 * @throws member_pointer_error if validation fails
 */
template <typename Container, typename T, typename MemberPtr>
const Container* container_of(const T* ptr, MemberPtr Container::* member_ptr) {
    validate_pointer(ptr, "container_of");
    validate_member_ptr(member_ptr, "container_of");

    std::size_t offset = offset_of(member_ptr);
    return reinterpret_cast<const Container*>(
        reinterpret_cast<const std::byte*>(ptr) - offset);
}

/**
 * @brief Finds the container of a range of elements using C++20 ranges
 */
template <std::ranges::range Container, typename T>
auto container_of_range(Container& container, const T* ptr)
    -> type::expected<typename Container::value_type*, member_pointer_error> {
    try {
        validate_pointer(ptr, "container_of_range");

        auto it = std::ranges::find(container, *ptr);
        if (it != std::ranges::end(container)) {
            return &(*it);
        }
        return type::unexpected(
            member_pointer_error("Element not found in container"));
    } catch (const member_pointer_error& e) {
        return type::unexpected(e);
    }
}

/**
 * @brief Finds the container of elements that match a given predicate using
 * C++20 ranges
 */
template <std::ranges::range Container, typename Predicate>
auto container_of_if_range(Container& container, Predicate pred)
    -> type::expected<typename Container::value_type*, member_pointer_error> {
    auto it = std::ranges::find_if(container, pred);
    if (it != std::ranges::end(container)) {
        return &(*it);
    }
    return type::unexpected(
        member_pointer_error("No matching element found in container"));
}

/**
 * @brief Check if a pointer points to a member of a specific object
 */
template <typename T, typename M>
bool is_member_of(const T* obj, const M* member_ptr, M T::* member) noexcept {
    try {
        validate_pointer(obj, "is_member_of");
        validate_pointer(member_ptr, "is_member_of");
        validate_member_ptr(member, "is_member_of");

        const M* actual_member = &(obj->*member);
        return actual_member == member_ptr;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Helper for dependent_false static_assert
 */
template <typename>
struct dependent_false : std::false_type {};

/**
 * @brief Get member from index in a tuple-like structure
 * Requires C++20 and a tuple-compatible structure
 */
template <size_t Index, typename T>
auto& get_member_by_index(T& obj) {
    if constexpr (requires { std::get<Index>(obj); }) {
        return std::get<Index>(obj);
    } else {
        static_assert(dependent_false<T>::value,
                      "Type does not support tuple-like element access");
    }
}

/**
 * @brief Apply a function to each member of an object with known member
 * pointers
 */
template <typename T, typename F, typename... Members>
void for_each_member(T& obj, F&& func, Members T::*... members) {
    (..., func(obj.*members));
}

/**
 * @brief Calculate memory layout statistics for a structure
 */
template <typename T>
struct memory_layout_stats {
    std::size_t size;
    std::size_t alignment;
    std::size_t potential_padding;

    static constexpr memory_layout_stats compute() noexcept {
        return {sizeof(T), alignof(T), sizeof(T) - compute_min_size<T>()};
    }

private:
    template <typename U>
    static constexpr std::size_t compute_min_size() noexcept {
        if constexpr (std::is_empty_v<U>) {
            return 0;
        } else {
            return sizeof(U);
        }
    }
};

}  // namespace atom::meta

#endif  // ATOM_FUNCTION_MEMBER_HPP
