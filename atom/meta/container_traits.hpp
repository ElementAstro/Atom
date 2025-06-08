/*!
 * \file container_traits.hpp
 * \brief Container traits for C++20 with comprehensive container type analysis
 * \author Max Qian <lightapt.com>
 * \date 2024-04-02
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_CONTAINER_TRAITS_HPP
#define ATOM_META_CONTAINER_TRAITS_HPP

#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "atom/meta/abi.hpp"


namespace atom::meta {

/**
 * \brief Primary template for container traits
 * \tparam Container Container type to analyze
 */
template <typename Container>
struct ContainerTraits;

/**
 * \brief Base traits for container types
 * \tparam T Element type
 * \tparam Container Container type
 */
template <typename T, typename Container>
struct ContainerTraitsBase {
    using value_type = T;
    using container_type = Container;
    // Only define size_type and difference_type if present in Container
    using size_type = std::conditional_t<
        requires { typename Container::size_type; },
        typename Container::size_type,
        std::size_t>;
    // Only define difference_type if present, otherwise void for adapters
    using difference_type = std::conditional_t<
        requires { typename Container::difference_type; },
        typename Container::difference_type,
        void>;

    // Default iterator types (will be overridden if available)
    using iterator = void;
    using const_iterator = void;
    using reverse_iterator = void;
    using const_reverse_iterator = void;

    // Container categories
    static constexpr bool is_sequence_container = false;
    static constexpr bool is_associative_container = false;
    static constexpr bool is_unordered_associative_container = false;
    static constexpr bool is_container_adapter = false;

    // Container capabilities
    static constexpr bool has_random_access = false;
    static constexpr bool has_bidirectional_access = false;
    static constexpr bool has_forward_access = false;
    static constexpr bool has_size = true;
    static constexpr bool has_empty = true;
    static constexpr bool has_clear = true;
    static constexpr bool has_begin_end = true;
    static constexpr bool has_rbegin_rend = false;
    static constexpr bool has_front = false;
    static constexpr bool has_back = false;
    static constexpr bool has_push_front = false;
    static constexpr bool has_push_back = false;
    static constexpr bool has_pop_front = false;
    static constexpr bool has_pop_back = false;
    static constexpr bool has_insert = false;
    static constexpr bool has_erase = false;
    static constexpr bool has_emplace = false;
    static constexpr bool has_emplace_front = false;
    static constexpr bool has_emplace_back = false;
    static constexpr bool has_reserve = false;
    static constexpr bool has_capacity = false;
    static constexpr bool has_shrink_to_fit = false;
    static constexpr bool has_subscript = false;
    static constexpr bool has_at = false;
    static constexpr bool has_find = false;
    static constexpr bool has_count = false;
    static constexpr bool has_key_type = false;
    static constexpr bool has_mapped_type = false;
    static constexpr bool is_sorted = false;
    static constexpr bool is_unique = false;
    static constexpr bool is_fixed_size = false;

    static const inline std::string full_name =
        DemangleHelper::demangle(typeid(Container).name());
};

/**
 * \brief Enhanced base for containers with iterator support
 */
template <typename T, typename Container>
struct IteratorContainerTraitsBase : ContainerTraitsBase<T, Container> {
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    static constexpr bool has_begin_end = true;
};

/**
 * \brief Enhanced base for containers with reverse iterator support
 */
template <typename T, typename Container>
struct ReverseIteratorContainerTraitsBase
    : IteratorContainerTraitsBase<T, Container> {
    using reverse_iterator = typename Container::reverse_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;

    static constexpr bool has_rbegin_rend = true;
    static constexpr bool has_bidirectional_access = true;
};

// ===== SEQUENCE CONTAINERS =====

/**
 * \brief Traits for std::vector
 */
template <typename T, typename Allocator>
struct ContainerTraits<std::vector<T, Allocator>>
    : ReverseIteratorContainerTraitsBase<T, std::vector<T, Allocator>> {
    using allocator_type = Allocator;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_random_access = true;
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
    static constexpr bool has_push_back = true;
    static constexpr bool has_pop_back = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_emplace_back = true;
    static constexpr bool has_reserve = true;
    static constexpr bool has_capacity = true;
    static constexpr bool has_shrink_to_fit = true;
    static constexpr bool has_subscript = true;
    static constexpr bool has_at = true;
};

/**
 * \brief Traits for std::deque
 */
template <typename T, typename Allocator>
struct ContainerTraits<std::deque<T, Allocator>>
    : ReverseIteratorContainerTraitsBase<T, std::deque<T, Allocator>> {
    using allocator_type = Allocator;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_random_access = true;
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
    static constexpr bool has_push_front = true;
    static constexpr bool has_push_back = true;
    static constexpr bool has_pop_front = true;
    static constexpr bool has_pop_back = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_emplace_front = true;
    static constexpr bool has_emplace_back = true;
    static constexpr bool has_subscript = true;
    static constexpr bool has_at = true;
    static constexpr bool has_shrink_to_fit = true;
};

/**
 * \brief Traits for std::list
 */
template <typename T, typename Allocator>
struct ContainerTraits<std::list<T, Allocator>>
    : ReverseIteratorContainerTraitsBase<T, std::list<T, Allocator>> {
    using allocator_type = Allocator;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
    static constexpr bool has_push_front = true;
    static constexpr bool has_push_back = true;
    static constexpr bool has_pop_front = true;
    static constexpr bool has_pop_back = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_emplace_front = true;
    static constexpr bool has_emplace_back = true;
};

/**
 * \brief Traits for std::forward_list
 */
template <typename T, typename Allocator>
struct ContainerTraits<std::forward_list<T, Allocator>>
    : IteratorContainerTraitsBase<T, std::forward_list<T, Allocator>> {
    using allocator_type = Allocator;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_forward_access = true;
    static constexpr bool has_front = true;
    static constexpr bool has_push_front = true;
    static constexpr bool has_pop_front = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_emplace_front = true;
    static constexpr bool has_size = false;  // forward_list doesn't have size()
};

/**
 * \brief Traits for std::array
 */
template <typename T, std::size_t N>
struct ContainerTraits<std::array<T, N>>
    : ReverseIteratorContainerTraitsBase<T, std::array<T, N>> {
    static constexpr std::size_t array_size = N;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_random_access = true;
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
    static constexpr bool has_subscript = true;
    static constexpr bool has_at = true;
    static constexpr bool is_fixed_size = true;
    static constexpr bool has_clear = false;  // array cannot be cleared
};

/**
 * \brief Traits for std::string
 */
template <typename CharT, typename Traits, typename Allocator>
struct ContainerTraits<std::basic_string<CharT, Traits, Allocator>>
    : ReverseIteratorContainerTraitsBase<
          CharT, std::basic_string<CharT, Traits, Allocator>> {
    using traits_type = Traits;
    using allocator_type = Allocator;

    static constexpr bool is_sequence_container = true;
    static constexpr bool has_random_access = true;
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
    static constexpr bool has_push_back = true;
    static constexpr bool has_pop_back = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_reserve = true;
    static constexpr bool has_capacity = true;
    static constexpr bool has_shrink_to_fit = true;
    static constexpr bool has_subscript = true;
    static constexpr bool has_at = true;
    static constexpr bool has_find = true;
};

// ===== ASSOCIATIVE CONTAINERS =====

/**
 * \brief Base traits for associative containers
 */
template <typename Key, typename T, typename Container>
struct AssociativeContainerTraitsBase
    : ReverseIteratorContainerTraitsBase<std::pair<const Key, T>, Container> {
    using key_type = Key;
    using mapped_type = T;

    static constexpr bool is_associative_container = true;
    static constexpr bool has_bidirectional_access = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_find = true;
    static constexpr bool has_count = true;
    static constexpr bool has_key_type = true;
    static constexpr bool has_mapped_type = true;
    static constexpr bool is_sorted = true;
};

/**
 * \brief Base traits for set-like containers
 */
template <typename Key, typename Container>
struct SetContainerTraitsBase
    : ReverseIteratorContainerTraitsBase<Key, Container> {
    using key_type = Key;

    static constexpr bool is_associative_container = true;
    static constexpr bool has_bidirectional_access = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_find = true;
    static constexpr bool has_count = true;
    static constexpr bool has_key_type = true;
    static constexpr bool is_sorted = true;
    static constexpr bool is_unique = true;
};

/**
 * \brief Traits for std::map
 */
template <typename Key, typename T, typename Compare, typename Allocator>
struct ContainerTraits<std::map<Key, T, Compare, Allocator>>
    : AssociativeContainerTraitsBase<Key, T,
                                     std::map<Key, T, Compare, Allocator>> {
    using key_compare = Compare;
    using allocator_type = Allocator;

    static constexpr bool is_unique = true;
    static constexpr bool has_subscript = true;  // operator[]
};

/**
 * \brief Traits for std::multimap
 */
template <typename Key, typename T, typename Compare, typename Allocator>
struct ContainerTraits<std::multimap<Key, T, Compare, Allocator>>
    : AssociativeContainerTraitsBase<
          Key, T, std::multimap<Key, T, Compare, Allocator>> {
    using key_compare = Compare;
    using allocator_type = Allocator;

    static constexpr bool is_unique = false;
};

/**
 * \brief Traits for std::set
 */
template <typename Key, typename Compare, typename Allocator>
struct ContainerTraits<std::set<Key, Compare, Allocator>>
    : SetContainerTraitsBase<Key, std::set<Key, Compare, Allocator>> {
    using key_compare = Compare;
    using allocator_type = Allocator;
};

/**
 * \brief Traits for std::multiset
 */
template <typename Key, typename Compare, typename Allocator>
struct ContainerTraits<std::multiset<Key, Compare, Allocator>>
    : SetContainerTraitsBase<Key, std::multiset<Key, Compare, Allocator>> {
    using key_compare = Compare;
    using allocator_type = Allocator;

    static constexpr bool is_unique = false;
};

// ===== UNORDERED ASSOCIATIVE CONTAINERS =====

/**
 * \brief Base traits for unordered associative containers
 */
template <typename Key, typename T, typename Container>
struct UnorderedAssociativeContainerTraitsBase
    : IteratorContainerTraitsBase<std::pair<const Key, T>, Container> {
    using key_type = Key;
    using mapped_type = T;

    static constexpr bool is_unordered_associative_container = true;
    static constexpr bool has_forward_access = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_find = true;
    static constexpr bool has_count = true;
    static constexpr bool has_key_type = true;
    static constexpr bool has_mapped_type = true;
    static constexpr bool has_reserve = true;
};

/**
 * \brief Base traits for unordered set-like containers
 */
template <typename Key, typename Container>
struct UnorderedSetContainerTraitsBase
    : IteratorContainerTraitsBase<Key, Container> {
    using key_type = Key;

    static constexpr bool is_unordered_associative_container = true;
    static constexpr bool has_forward_access = true;
    static constexpr bool has_insert = true;
    static constexpr bool has_erase = true;
    static constexpr bool has_emplace = true;
    static constexpr bool has_find = true;
    static constexpr bool has_count = true;
    static constexpr bool has_key_type = true;
    static constexpr bool has_reserve = true;
    static constexpr bool is_unique = true;
};

/**
 * \brief Traits for std::unordered_map
 */
template <typename Key, typename T, typename Hash, typename KeyEqual,
          typename Allocator>
struct ContainerTraits<std::unordered_map<Key, T, Hash, KeyEqual, Allocator>>
    : UnorderedAssociativeContainerTraitsBase<
          Key, T, std::unordered_map<Key, T, Hash, KeyEqual, Allocator>> {
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

    static constexpr bool is_unique = true;
    static constexpr bool has_subscript = true;  // operator[]
};

/**
 * \brief Traits for std::unordered_multimap
 */
template <typename Key, typename T, typename Hash, typename KeyEqual,
          typename Allocator>
struct ContainerTraits<
    std::unordered_multimap<Key, T, Hash, KeyEqual, Allocator>>
    : UnorderedAssociativeContainerTraitsBase<
          Key, T, std::unordered_multimap<Key, T, Hash, KeyEqual, Allocator>> {
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

    static constexpr bool is_unique = false;
};

/**
 * \brief Traits for std::unordered_set
 */
template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
struct ContainerTraits<std::unordered_set<Key, Hash, KeyEqual, Allocator>>
    : UnorderedSetContainerTraitsBase<
          Key, std::unordered_set<Key, Hash, KeyEqual, Allocator>> {
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
};

/**
 * \brief Traits for std::unordered_multiset
 */
template <typename Key, typename Hash, typename KeyEqual, typename Allocator>
struct ContainerTraits<std::unordered_multiset<Key, Hash, KeyEqual, Allocator>>
    : UnorderedSetContainerTraitsBase<
          Key, std::unordered_multiset<Key, Hash, KeyEqual, Allocator>> {
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

    static constexpr bool is_unique = false;
};

// ===== CONTAINER ADAPTERS =====

/**
 * \brief Base traits for container adapters
 */
template <typename T, typename Container, typename Adapter>
struct ContainerAdapterTraitsBase : ContainerTraitsBase<T, Adapter> {
    using container_type = Container;
    using underlying_container_type = Container;

    static constexpr bool is_container_adapter = true;
    static constexpr bool has_begin_end = false;
    static constexpr bool has_clear = false;
    static constexpr bool has_insert = false;
    static constexpr bool has_erase = false;
};

/**
 * \brief Traits for std::stack
 */
template <typename T, typename Container>
struct ContainerTraits<std::stack<T, Container>>
    : ContainerAdapterTraitsBase<T, Container, std::stack<T, Container>> {
    static constexpr bool has_push_back = true;  // push
    static constexpr bool has_pop_back = true;   // pop
    static constexpr bool has_back = true;       // top
};

/**
 * \brief Traits for std::queue
 */
template <typename T, typename Container>
struct ContainerTraits<std::queue<T, Container>>
    : ContainerAdapterTraitsBase<T, Container, std::queue<T, Container>> {
    static constexpr bool has_push_back = true;  // push
    static constexpr bool has_pop_front = true;  // pop
    static constexpr bool has_front = true;
    static constexpr bool has_back = true;
};

/**
 * \brief Traits for std::priority_queue
 */
template <typename T, typename Container, typename Compare>
struct ContainerTraits<std::priority_queue<T, Container, Compare>>
    : ContainerAdapterTraitsBase<T, Container,
                                 std::priority_queue<T, Container, Compare>> {
    using value_compare = Compare;

    static constexpr bool has_push_back = true;  // push
    static constexpr bool has_pop_back = true;   // pop
    static constexpr bool has_back = true;       // top
    static constexpr bool is_sorted = true;      // maintains heap order
};

// ===== CONTAINER TRAITS FOR REFERENCES =====

/**
 * \brief Traits for container references
 */
template <typename Container>
struct ContainerTraits<Container&> : ContainerTraits<Container> {};

/**
 * \brief Traits for container rvalue references
 */
template <typename Container>
struct ContainerTraits<Container&&> : ContainerTraits<Container> {};

/**
 * \brief Traits for const containers
 */
template <typename Container>
struct ContainerTraits<const Container> : ContainerTraits<Container> {};

// ===== VARIABLE TEMPLATES FOR EASY ACCESS =====

/**
 * \brief Variable template for sequence container check
 */
template <typename Container>
inline constexpr bool is_sequence_container_v =
    ContainerTraits<Container>::is_sequence_container;

/**
 * \brief Variable template for associative container check
 */
template <typename Container>
inline constexpr bool is_associative_container_v =
    ContainerTraits<Container>::is_associative_container;

/**
 * \brief Variable template for unordered associative container check
 */
template <typename Container>
inline constexpr bool is_unordered_associative_container_v =
    ContainerTraits<Container>::is_unordered_associative_container;

/**
 * \brief Variable template for container adapter check
 */
template <typename Container>
inline constexpr bool is_container_adapter_v =
    ContainerTraits<Container>::is_container_adapter;

/**
 * \brief Variable template for random access check
 */
template <typename Container>
inline constexpr bool has_random_access_v =
    ContainerTraits<Container>::has_random_access;

/**
 * \brief Variable template for bidirectional access check
 */
template <typename Container>
inline constexpr bool has_bidirectional_access_v =
    ContainerTraits<Container>::has_bidirectional_access;

/**
 * \brief Variable template for forward access check
 */
template <typename Container>
inline constexpr bool has_forward_access_v =
    ContainerTraits<Container>::has_forward_access;

/**
 * \brief Variable template for subscript operator check
 */
template <typename Container>
inline constexpr bool has_subscript_v =
    ContainerTraits<Container>::has_subscript;

/**
 * \brief Variable template for reserve capability check
 */
template <typename Container>
inline constexpr bool has_reserve_v = ContainerTraits<Container>::has_reserve;

/**
 * \brief Variable template for capacity capability check
 */
template <typename Container>
inline constexpr bool has_capacity_v = ContainerTraits<Container>::has_capacity;

/**
 * \brief Variable template for push_back capability check
 */
template <typename Container>
inline constexpr bool has_push_back_v =
    ContainerTraits<Container>::has_push_back;

/**
 * \brief Variable template for push_front capability check
 */
template <typename Container>
inline constexpr bool has_push_front_v =
    ContainerTraits<Container>::has_push_front;

/**
 * \brief Variable template for insert capability check
 */
template <typename Container>
inline constexpr bool has_insert_v = ContainerTraits<Container>::has_insert;

/**
 * \brief Variable template for fixed size check
 */
template <typename Container>
inline constexpr bool is_fixed_size_v =
    ContainerTraits<Container>::is_fixed_size;

/**
 * \brief Variable template for sorted container check
 */
template <typename Container>
inline constexpr bool is_sorted_v = ContainerTraits<Container>::is_sorted;

/**
 * \brief Variable template for unique container check
 */
template <typename Container>
inline constexpr bool is_unique_v = ContainerTraits<Container>::is_unique;

// ===== UTILITY FUNCTIONS =====

/**
 * \brief Get the iterator category of a container
 * \tparam Container Container type
 * \return Iterator category tag
 */
template <typename Container>
constexpr auto get_iterator_category() {
    if constexpr (ContainerTraits<Container>::has_random_access) {
        return std::random_access_iterator_tag{};
    } else if constexpr (ContainerTraits<Container>::has_bidirectional_access) {
        return std::bidirectional_iterator_tag{};
    } else if constexpr (ContainerTraits<Container>::has_forward_access) {
        return std::forward_iterator_tag{};
    } else {
        return std::input_iterator_tag{};
    }
}

/**
 * \brief Check if container supports efficient random access
 * \tparam Container Container type
 * \return true if container has O(1) random access
 */
template <typename Container>
constexpr bool supports_efficient_random_access() {
    return ContainerTraits<Container>::has_random_access &&
           ContainerTraits<Container>::has_subscript;
}

/**
 * \brief Check if container can grow dynamically
 * \tparam Container Container type
 * \return true if container can change size
 */
template <typename Container>
constexpr bool can_grow_dynamically() {
    return !ContainerTraits<Container>::is_fixed_size &&
           (ContainerTraits<Container>::has_push_back ||
            ContainerTraits<Container>::has_push_front ||
            ContainerTraits<Container>::has_insert);
}

/**
 * \brief Check if container supports key-based lookup
 * \tparam Container Container type
 * \return true if container supports find operations
 */
template <typename Container>
constexpr bool supports_key_lookup() {
    return ContainerTraits<Container>::has_find &&
           ContainerTraits<Container>::has_key_type;
}

/**
 * \brief Container operation detector using SFINAE
 * \tparam Container Container type
 * \tparam Operation Operation type
 */
template <typename Container, typename Operation>
struct container_supports_operation;

/**
 * \brief Specialization for checking push_back operation
 */
template <typename Container>
struct container_supports_operation<
    Container, void(typename ContainerTraits<Container>::value_type)> {
    template <typename C>
    static auto test(int)
        -> decltype(std::declval<C>().push_back(
                        std::declval<
                            typename ContainerTraits<C>::value_type>()),
                    std::true_type{});

    template <typename>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<Container>(0))::value;
};

/**
 * \brief Container pipe class for functional composition with containers
 * \tparam Container Container type
 */
template <typename Container>
class container_pipe;

/**
 * \brief Specialization for sequence containers
 * \tparam Container Sequence container type
 */
template <typename Container>
    requires is_sequence_container_v<Container>
class container_pipe<Container> {
    Container container_;

public:
    /**
     * \brief Constructor
     * \param container Container to wrap
     */
    explicit container_pipe(Container container)
        : container_(std::move(container)) {}

    /**
     * \brief Apply transformation to each element
     * \tparam Func Transformation function
     * \param func Function to apply
     * \return New container with transformed elements
     */
    template <typename Func>
    auto transform(Func func) -> container_pipe<std::vector<
        std::invoke_result_t<Func, typename Container::value_type>>> {
        std::vector<std::invoke_result_t<Func, typename Container::value_type>>
            result;
        if constexpr (has_reserve_v<Container>) {
            result.reserve(container_.size());
        }

        for (const auto& elem : container_) {
            result.push_back(func(elem));
        }

        return container_pipe<decltype(result)>(std::move(result));
    }

    /**
     * \brief Filter elements based on predicate
     * \tparam Pred Predicate function
     * \param pred Predicate to test elements
     * \return New container with filtered elements
     */
    template <typename Pred>
    auto filter(Pred pred) -> container_pipe<Container> {
        Container result;

        for (const auto& elem : container_) {
            if (pred(elem)) {
                if constexpr (has_push_back_v<Container>) {
                    result.push_back(elem);
                } else if constexpr (has_insert_v<Container>) {
                    result.insert(result.end(), elem);
                }
            }
        }

        return container_pipe<Container>(std::move(result));
    }

    /**
     * \brief Get the underlying container
     * \return Reference to container
     */
    const Container& get() const { return container_; }
    Container& get() { return container_; }
};

/**
 * \brief Factory function for container pipes
 * \tparam Container Container type
 * \param container Container to wrap
 * \return Container pipe
 */
template <typename Container>
auto make_container_pipe(Container&& container) {
    return container_pipe<std::decay_t<Container>>(
        std::forward<Container>(container));
}

// ===== MACROS FOR CUSTOM CONTAINER TRAITS =====

/**
 * \brief Macro to define traits for a custom sequence container
 * \param ContainerName Name of the container class
 * \param ValueType Element type
 */
#define DEFINE_SEQUENCE_CONTAINER_TRAITS(ContainerName, ValueType)       \
    template <>                                                          \
    struct ContainerTraits<ContainerName<ValueType>>                     \
        : ReverseIteratorContainerTraitsBase<ValueType,                  \
                                             ContainerName<ValueType>> { \
        static constexpr bool is_sequence_container = true;              \
        /* Add specific capabilities here */                             \
    }

/**
 * \brief Macro to define traits for a custom associative container
 * \param ContainerName Name of the container class
 * \param KeyType Key type
 * \param ValueType Value type
 */
#define DEFINE_ASSOCIATIVE_CONTAINER_TRAITS(ContainerName, KeyType, ValueType) \
    template <>                                                                \
    struct ContainerTraits<ContainerName<KeyType, ValueType>>                  \
        : AssociativeContainerTraitsBase<KeyType, ValueType,                   \
                                         ContainerName<KeyType, ValueType>> {  \
        /* Add specific capabilities here */                                   \
    }

}  // namespace atom::meta

#endif  // ATOM_META_CONTAINER_TRAITS_HPP