#ifndef ATOM_UTILS_CONTAINER_HPP
#define ATOM_UTILS_CONTAINER_HPP

#include <algorithm>
#include <functional>
#include <ranges>
#include <sstream>
#include <vector>

#include "atom/containers/high_performance.hpp"

namespace atom::utils {

template <typename T>
using HashSet = atom::containers::HashSet<T>;

template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

template <typename T>
using Vector = atom::containers::Vector<T>;

template <typename K, typename V>
using Map = atom::containers::Map<K, V>;

template <typename T, size_t N = 16>
using SmallVector = atom::containers::SmallVector<T, N>;

using String = atom::containers::String;

/**
 * @brief Checks if one container is a subset of another container.
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<int> b = {1, 2, 3, 4};
 *   bool result = isSubset(a, b); // returns true
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type> &&
             std::regular<typename Container2::value_type> &&
             std::regular<typename Container1::value_type>
constexpr auto isSubset(const Container1& subset, const Container2& superset)
    -> bool {
    HashSet<typename Container2::value_type> set(superset.begin(),
                                                 superset.end());
    return std::ranges::all_of(
        subset, [&set](const auto& elem) { return set.contains(elem); });
}

/**
 * @brief Checks if a container contains a specific element.
 * @example
 *   Vector<int> v = {1, 2, 3};
 *   bool result = contains(v, 2); // returns true
 */
template <typename Container, typename T>
    requires std::ranges::input_range<Container> &&
             std::equality_comparable_with<typename Container::value_type, T>
constexpr auto contains(const Container& container, const T& value) -> bool {
    return std::ranges::find(container, value) != container.end();
}

/**
 * @brief Converts a container to a HashSet for fast lookup.
 * @example
 *   Vector<int> v = {1, 2, 2, 3};
 *   auto set = toHashSet(v); // returns HashSet with {1, 2, 3}
 */
template <typename Container>
    requires std::ranges::input_range<Container> &&
             std::regular<typename Container::value_type>
auto toHashSet(const Container& container) {
    return HashSet<typename Container::value_type>(container.begin(),
                                                   container.end());
}

/**
 * @brief Checks subset relationship using linear search (less efficient).
 * @example
 *   Vector<int> a = {1, 2};
 *   Vector<int> b = {1, 2, 3};
 *   bool result = isSubsetLinearSearch(a, b); // returns true
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
constexpr auto isSubsetLinearSearch(const Container1& subset,
                                    const Container2& superset) -> bool {
    return std::ranges::all_of(subset, [&superset](const auto& elem) {
        return contains(superset, elem);
    });
}

/**
 * @brief Checks subset relationship using HashSet (more efficient).
 * @example
 *   Vector<int> a = {1, 2};
 *   Vector<int> b = {1, 2, 3, 4};
 *   bool result = isSubsetWithHashSet(a, b); // returns true
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type> &&
             std::regular<typename Container2::value_type>
auto isSubsetWithHashSet(const Container1& subset, const Container2& superset)
    -> bool {
    auto supersetSet = toHashSet(superset);
    return std::ranges::all_of(subset, [&supersetSet](const auto& elem) {
        return supersetSet.contains(elem);
    });
}

/**
 * @brief Returns intersection of two containers.
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<int> b = {2, 3, 4};
 *   auto result = intersection(a, b); // returns {2, 3}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
auto intersection(const Container1& container1, const Container2& container2) {
    Vector<typename Container1::value_type> result;
    result.reserve(std::min(container1.size(), container2.size()));

    auto set2 = toHashSet(container2);
    for (const auto& elem : container1) {
        if (set2.contains(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Returns union of two containers.
 * @example
 *   Vector<int> a = {1, 2};
 *   Vector<int> b = {2, 3};
 *   auto result = unionSet(a, b); // returns {1, 2, 3}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
auto unionSet(const Container1& container1, const Container2& container2) {
    HashSet<typename Container1::value_type> result(container1.begin(),
                                                    container1.end());
    result.insert(container2.begin(), container2.end());
    return Vector<typename Container1::value_type>(result.begin(),
                                                   result.end());
}

/**
 * @brief Returns difference between containers (container1 - container2).
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<int> b = {2, 4};
 *   auto result = difference(a, b); // returns {1, 3}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
auto difference(const Container1& container1, const Container2& container2) {
    Vector<typename Container1::value_type> result;
    result.reserve(container1.size());

    auto set2 = toHashSet(container2);
    for (const auto& elem : container1) {
        if (!set2.contains(elem)) {
            result.push_back(elem);
        }
    }
    return result;
}

/**
 * @brief Returns symmetric difference between containers.
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<int> b = {2, 3, 4};
 *   auto result = symmetricDifference(a, b); // returns {1, 4}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
auto symmetricDifference(const Container1& container1,
                         const Container2& container2) {
    auto diff1 = difference(container1, container2);
    auto diff2 = difference(container2, container1);
    return unionSet(diff1, diff2);
}

/**
 * @brief Checks if two containers are equal (same elements, any order).
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<int> b = {3, 2, 1};
 *   bool result = isEqual(a, b); // returns true
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2> &&
             std::equality_comparable_with<typename Container1::value_type,
                                           typename Container2::value_type>
constexpr auto isEqual(const Container1& container1,
                       const Container2& container2) -> bool {
    if (container1.size() != container2.size()) {
        return false;
    }
    return isSubsetWithHashSet(container1, container2);
}

/**
 * @brief Applies member function to each element and stores results.
 * @example
 *   struct Point { int x, y; };
 *   Vector<Point> points = {{1,2}, {3,4}};
 *   auto xs = applyAndStore(points, &Point::x); // returns {1, 3}
 */
template <typename Container, typename MemberFunc>
auto applyAndStore(const Container& source, MemberFunc memberFunc) {
    using ReturnType = decltype(std::invoke(memberFunc, *source.begin()));
    Vector<ReturnType> result;
    result.reserve(source.size());

    for (const auto& elem : source) {
        result.push_back(std::invoke(memberFunc, elem));
    }

    return result;
}

template <typename T, typename U>
concept HasMemberFunc = std::invocable<U, T>;

/**
 * @brief Transforms container elements using member function.
 * @example
 *   Vector<std::string> strs = {"a", "bb", "ccc"};
 *   auto lengths = transformToVector(strs, &std::string::size); // returns {1,
 * 2, 3}
 */
template <typename Container, typename MemberFunc>
    requires std::ranges::input_range<Container> &&
             HasMemberFunc<typename Container::value_type, MemberFunc>
auto transformToVector(const Container& source, MemberFunc memberFunc) {
    using ReturnType = decltype(std::invoke(memberFunc, *std::begin(source)));
    Vector<ReturnType> result;
    result.reserve(source.size());

    for (const auto& elem : source) {
        result.push_back(std::invoke(memberFunc, elem));
    }

    return result;
}

/**
 * @brief Creates unique map from container of pairs.
 * @example
 *   Vector<std::pair<int, string>> pairs = {{1,"a"}, {1,"b"}, {2,"c"}};
 *   auto uniqueMap = unique(pairs); // returns map with {1:"b", 2:"c"}
 */
template <typename MapContainer>
    requires std::ranges::input_range<MapContainer> && requires {
        typename MapContainer::key_type;
        typename MapContainer::mapped_type;
    }
auto unique(const MapContainer& container) {
    HashMap<typename MapContainer::key_type, typename MapContainer::mapped_type>
        map(container.begin(), container.end());
    return map;
}

/**
 * @brief Removes duplicate elements from container.
 * @example
 *   Vector<int> v = {1, 2, 2, 3};
 *   auto uniqueVec = unique(v); // returns {1, 2, 3}
 */
template <typename Container>
    requires std::ranges::input_range<Container> &&
             std::regular<typename Container::value_type>
auto unique(const Container& container) {
    HashSet<typename Container::value_type> set(container.begin(),
                                                container.end());
    return Vector<typename Container::value_type>(set.begin(), set.end());
}

/**
 * @brief Flattens nested container into single container.
 * @example
 *   Vector<Vector<int>> nested = {{1,2}, {3}, {4,5}};
 *   auto flat = flatten(nested); // returns {1,2,3,4,5}
 */
template <typename Container>
    requires std::ranges::input_range<Container> &&
             std::ranges::input_range<typename Container::value_type>
auto flatten(const Container& container) {
    using InnerContainer = typename Container::value_type;
    Vector<typename InnerContainer::value_type> result;

    size_t totalSize = 0;
    for (const auto& inner : container) {
        totalSize += inner.size();
    }
    result.reserve(totalSize);

    for (const auto& inner : container) {
        result.insert(result.end(), inner.begin(), inner.end());
    }

    return result;
}

/**
 * @brief Zips two containers into container of pairs.
 * @example
 *   Vector<int> a = {1, 2, 3};
 *   Vector<char> b = {'a', 'b', 'c'};
 *   auto zipped = zip(a, b); // returns {{1,'a'}, {2,'b'}, {3,'c'}}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2>
auto zip(const Container1& container1, const Container2& container2) {
    using ValueType1 = typename Container1::value_type;
    using ValueType2 = typename Container2::value_type;
    Vector<std::pair<ValueType1, ValueType2>> result;

    const auto minSize = std::min(container1.size(), container2.size());
    result.reserve(minSize);

    auto it1 = container1.begin();
    auto it2 = container2.begin();

    while (it1 != container1.end() && it2 != container2.end()) {
        result.emplace_back(*it1, *it2);
        ++it1;
        ++it2;
    }

    return result;
}

/**
 * @brief Computes Cartesian product of two containers.
 * @example
 *   Vector<int> a = {1, 2};
 *   Vector<char> b = {'a', 'b'};
 *   auto product = cartesianProduct(a, b); // returns {{1,'a'}, {1,'b'},
 * {2,'a'}, {2,'b'}}
 */
template <typename Container1, typename Container2>
    requires std::ranges::input_range<Container1> &&
             std::ranges::input_range<Container2>
auto cartesianProduct(const Container1& container1,
                      const Container2& container2) {
    using ValueType1 = typename Container1::value_type;
    using ValueType2 = typename Container2::value_type;
    Vector<std::pair<ValueType1, ValueType2>> result;

    result.reserve(container1.size() * container2.size());

    for (const auto& elem1 : container1) {
        for (const auto& elem2 : container2) {
            result.emplace_back(elem1, elem2);
        }
    }

    return result;
}

/**
 * @brief Filters container elements based on predicate.
 * @example
 *   Vector<int> v = {1, 2, 3, 4};
 *   auto even = filter(v, [](int x){ return x % 2 == 0; }); // returns {2, 4}
 */
template <typename Container, typename Predicate>
    requires std::ranges::input_range<Container> &&
             std::predicate<Predicate, typename Container::value_type>
auto filter(const Container& container, Predicate predicate) {
    Vector<typename Container::value_type> result;
    result.reserve(container.size() / 2);

    for (const auto& elem : container) {
        if (predicate(elem)) {
            result.push_back(elem);
        }
    }

    return result;
}

/**
 * @brief Partitions container based on predicate.
 * @example
 *   Vector<int> v = {1, 2, 3, 4};
 *   auto [even, odd] = partition(v, [](int x){ return x % 2 == 0; });
 *   // even = {2, 4}, odd = {1, 3}
 */
template <typename Container, typename Predicate>
    requires std::ranges::input_range<Container> &&
             std::predicate<Predicate, typename Container::value_type>
auto partition(const Container& container, Predicate predicate) {
    Vector<typename Container::value_type> truePart;
    Vector<typename Container::value_type> falsePart;

    const auto halfSize = container.size() / 2;
    truePart.reserve(halfSize);
    falsePart.reserve(halfSize);

    for (const auto& elem : container) {
        if (predicate(elem)) {
            truePart.push_back(elem);
        } else {
            falsePart.push_back(elem);
        }
    }

    return std::make_pair(std::move(truePart), std::move(falsePart));
}

/**
 * @brief Finds first element satisfying predicate.
 * @example
 *   Vector<int> v = {1, 2, 3, 4};
 *   auto result = findIf(v, [](int x){ return x > 2; }); // returns 3
 */
template <typename Container, typename Predicate>
    requires std::ranges::input_range<Container> &&
             std::predicate<Predicate, typename Container::value_type>
constexpr auto findIf(const Container& container, Predicate predicate)
    -> std::optional<typename Container::value_type> {
    for (const auto& elem : container) {
        if (predicate(elem)) {
            return elem;
        }
    }
    return std::nullopt;
}

}  // namespace atom::utils

/**
 * @brief String literal to create Vector<String> from comma-separated values.
 * @example
 *   auto vec = "one, two, three"_vec; // returns Vector<String>{"one", "two",
 * "three"}
 */
inline auto operator""_vec(const char* str, size_t)
    -> atom::containers::Vector<atom::containers::String> {
    atom::containers::Vector<atom::containers::String> vec;
    atom::containers::String token;
    std::istringstream tokenStream(str);

    while (std::getline(tokenStream, token, ',')) {
        const auto start = token.find_first_not_of(" ");
        const auto end = token.find_last_not_of(" ");
        if (start != std::string::npos && end != std::string::npos) {
            vec.push_back(token.substr(start, end - start + 1));
        }
    }

    return vec;
}

#endif  // ATOM_UTILS_CONTAINER_HPP
