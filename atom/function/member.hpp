#ifndef ATOM_FUNCTION_MEMBER_HPP
#define ATOM_FUNCTION_MEMBER_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>  // For offsetof
#include <cstdint>  // For std::uintptr_t
#include <iostream>
#include <type_traits>

namespace atom::meta {
/**
 * @brief Gets the offset of a member within a structure.
 *
 * @tparam T The type of the structure.
 * @tparam M The type of the member.
 * @param member The member pointer.
 * @return The offset of the member within the structure.
 */
template <typename T, typename M>
constexpr std::size_t member_offset(M T::*member) {
    return reinterpret_cast<std::uintptr_t>(
        &(static_cast<T const volatile*>(nullptr)->*member));
}

/**
 * @brief Gets the size of a member within a structure.
 *
 * @tparam T The type of the structure.
 * @tparam M The type of the member.
 * @param member The member pointer.
 * @return The size of the member.
 */
template <typename T, typename M>
constexpr std::size_t member_size(M T::*member) {
    return sizeof((static_cast<T const volatile*>(nullptr)->*member));
}

/**
 * @brief Gets the total size of a structure.
 *
 * @tparam T The type of the structure.
 * @return The size of the structure.
 */
template <typename T>
constexpr std::size_t struct_size() {
    return sizeof(T);
}

/**
 * @brief Prints the offset and size of all members in a structure.
 *
 * @tparam T The type of the structure.
 * @tparam Members The types of the members.
 * @param members The member pointers.
 */
template <typename T, typename... Members>
void print_member_info(Members T::*... members) {
    (..., (std::cout << "Offset: " << member_offset(members)
                     << ", Size: " << member_size(members) << std::endl));
}

/**
 * @brief Calculates the offset of a member within a structure.
 *
 * @tparam T The type of the structure.
 * @tparam MemberType The type of the member.
 * @param member_ptr The member pointer.
 * @return The offset of the member within the structure.
 */
template <typename T, typename MemberType>
constexpr std::size_t offset_of(MemberType T::*member_ptr) {
    return reinterpret_cast<std::size_t>(
        &(static_cast<T const volatile*>(nullptr)->*member_ptr));
}

/**
 * @brief Converts a member pointer to the containing object pointer.
 *
 * @tparam T The type of the structure.
 * @tparam MemberType The type of the member.
 * @param member_ptr The member pointer.
 * @param member_ptr_address The address of the member.
 * @return The pointer to the containing object.
 */
template <typename T, typename MemberType>
T* pointer_to_object(MemberType T::*member_ptr,
                     MemberType* member_ptr_address) {
    static_assert(std::is_member_pointer<decltype(member_ptr)>::value,
                  "member_ptr must be a member pointer");
    std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(member_ptr_address);
    std::uintptr_t object_address = address - offset_of(member_ptr);
    return reinterpret_cast<T*>(object_address);
}

/**
 * @brief Converts a const member pointer to the containing const object
 * pointer.
 *
 * @tparam T The type of the structure.
 * @tparam MemberType The type of the member.
 * @param member_ptr The member pointer.
 * @param member_ptr_address The address of the member.
 * @return The const pointer to the containing object.
 */
template <typename T, typename MemberType>
const T* pointer_to_object(MemberType T::*member_ptr,
                           const MemberType* member_ptr_address) {
    static_assert(std::is_member_pointer<decltype(member_ptr)>::value,
                  "member_ptr must be a member pointer");
    std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(member_ptr_address);
    std::uintptr_t object_address = address - offset_of(member_ptr);
    return reinterpret_cast<const T*>(object_address);
}

/**
 * @brief Converts a member pointer to the containing container pointer.
 *
 * @tparam Container The type of the container.
 * @tparam T The type of the member.
 * @tparam MemberPtr The type of the member pointer.
 * @param ptr The pointer to the member.
 * @param member_ptr The member pointer.
 * @return The pointer to the containing container.
 */
template <typename Container, typename T, typename MemberPtr>
constexpr Container* container_of(T* ptr, MemberPtr Container::*member_ptr) {
    assert(ptr != nullptr && "Pointer must not be null.");
    Container dummy;
    auto member_addr = &(dummy.*member_ptr);
    auto base_addr = &dummy;
    auto offset = reinterpret_cast<std::uintptr_t>(member_addr) -
                  reinterpret_cast<std::uintptr_t>(base_addr);
    return reinterpret_cast<Container*>(reinterpret_cast<char*>(ptr) - offset);
}

/**
 * @brief Converts a member pointer to the containing base class pointer,
 * supporting inheritance.
 *
 * @tparam Base The type of the base class.
 * @tparam Derived The type of the derived class.
 * @tparam T The type of the member.
 * @param ptr The pointer to the member.
 * @param member_ptr The member pointer.
 * @return The pointer to the containing base class.
 */
template <typename Base, typename Derived, typename T>
constexpr Base* container_of(T* ptr, T Derived::*member_ptr) {
    assert(ptr != nullptr && "Pointer must not be null.");
    auto offset = reinterpret_cast<std::uintptr_t>(
                      &(static_cast<Derived*>(nullptr)->*member_ptr)) -
                  reinterpret_cast<std::uintptr_t>(nullptr);
    return reinterpret_cast<Base*>(reinterpret_cast<std::uintptr_t>(ptr) -
                                   offset);
}

/**
 * @brief Converts a const member pointer to the containing const container
 * pointer.
 *
 * @tparam Container The type of the container.
 * @tparam T The type of the member.
 * @tparam MemberPtr The type of the member pointer.
 * @param ptr The const pointer to the member.
 * @param member_ptr The member pointer.
 * @return The const pointer to the containing container.
 */
template <typename Container, typename T, typename MemberPtr>
constexpr const Container* container_of(const T* ptr,
                                        MemberPtr Container::*member_ptr) {
    assert(ptr != nullptr && "Pointer must not be null.");
    auto offset = reinterpret_cast<std::uintptr_t>(
                      &(static_cast<Container*>(nullptr)->*member_ptr)) -
                  reinterpret_cast<std::uintptr_t>(nullptr);
    return reinterpret_cast<const Container*>(
        reinterpret_cast<std::uintptr_t>(ptr) - offset);
}

/**
 * @brief Converts a const member pointer to the containing const base class
 * pointer, supporting inheritance.
 *
 * @tparam Base The type of the base class.
 * @tparam Derived The type of the derived class.
 * @tparam T The type of the member.
 * @param ptr The const pointer to the member.
 * @param member_ptr The member pointer.
 * @return The const pointer to the containing base class.
 */
template <typename Base, typename Derived, typename T>
constexpr const Base* container_of(const T* ptr, T Derived::*member_ptr) {
    assert(ptr != nullptr && "Pointer must not be null.");
    auto offset = reinterpret_cast<std::uintptr_t>(
                      &(static_cast<Derived*>(nullptr)->*member_ptr)) -
                  reinterpret_cast<std::uintptr_t>(nullptr);
    return reinterpret_cast<const Base*>(reinterpret_cast<std::uintptr_t>(ptr) -
                                         offset);
}

/**
 * @brief Finds the container of a range of elements.
 *
 * @tparam Container The type of the container.
 * @tparam ElemPtr The type of the element pointer.
 * @param container The container.
 * @param ptr The pointer to the element.
 * @return The pointer to the container element if found, nullptr otherwise.
 */
template <typename Container, typename ElemPtr>
auto container_of_range(Container& container, ElemPtr ptr) ->
    typename Container::value_type* {
    if (ptr == nullptr) {
        return nullptr;
    }
    auto it = std::find(container.begin(), container.end(), *ptr);
    if (it != container.end()) {
        return &(*it);
    }
    return nullptr;
}

/**
 * @brief Finds the container of elements that match a given predicate.
 *
 * @tparam Container The type of the container.
 * @tparam Predicate The type of the predicate.
 * @param container The container.
 * @param pred The predicate function.
 * @return The pointer to the container element if found, nullptr otherwise.
 */
template <typename Container, typename Predicate>
auto container_of_if_range(Container& container, Predicate pred) ->
    typename Container::value_type* {
    auto it = std::find_if(container.begin(), container.end(), pred);
    if (it != container.end()) {
        return &(*it);
    }
    return nullptr;
}
}  // namespace atom::meta

#endif  // ATOM_FUNCTION_MEMBER_HPP