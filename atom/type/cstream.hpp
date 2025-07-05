#ifndef ATOM_TYPE_CONTAINERS_STREAMS_HPP
#define ATOM_TYPE_CONTAINERS_STREAMS_HPP

#include <algorithm>
#include <functional>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace atom::type {

/**
 * @brief A functor for accumulating containers.
 *
 * @tparam C The type of the container.
 */
template <typename C>
struct ContainerAccumulate {
    using value_type = typename C::value_type;

    /**
     * @brief Accumulates the source container into the destination container.
     *
     * @param dest The destination container.
     * @param source The source container.
     * @return C& The accumulated container.
     */
    auto operator()(C& dest, const C& source) const -> C& {
        dest.insert(dest.end(), source.begin(), source.end());
        return dest;
    }
};

/**
 * @brief A functor that returns the input value.
 *
 * @tparam V The type of the value.
 */
template <typename V>
struct identity {
    /**
     * @brief Returns the input value.
     *
     * @param v The input value.
     * @return V The same value.
     */
    constexpr auto operator()(const V& v) const -> V { return v; }
};

/**
 * @brief A stream-like class for performing operations on containers.
 *
 * @tparam C The type of the container.
 */
template <typename C>
class cstream {
public:
    using value_type = typename C::value_type;
    using iterator = typename C::iterator;
    using const_iterator = typename C::const_iterator;

    /**
     * @brief Constructs a cstream from a container reference.
     *
     * @param c The container reference.
     */
    explicit cstream(C& c) : container_ref_{c} {}

    /**
     * @brief Constructs a cstream from an rvalue container.
     *
     * @param c The rvalue container.
     */
    explicit cstream(C&& c) : moved_{std::move(c)}, container_ref_{moved_} {}

    /**
     * @brief Gets the reference to the container.
     *
     * @return C& The container reference.
     */
    auto getRef() -> C& { return container_ref_; }

    /**
     * @brief Moves the container out of the stream.
     *
     * @return C&& The moved container.
     */
    auto getMove() -> C&& { return std::move(container_ref_); }

    /**
     * @brief Gets a copy of the container.
     *
     * @return C The copied container.
     */
    auto get() const -> C { return container_ref_; }

    /**
     * @brief Converts the stream to an rvalue container.
     *
     * @return C&& The moved container.
     */
    explicit operator C&&() { return getMove(); }

    /**
     * @brief Sorts the container.
     *
     * @tparam BinaryFunction The type of the comparison function.
     * @param op The comparison function.
     * @return cstream<C>& The sorted stream.
     */
    template <typename BinaryFunction = std::less<value_type>>
    auto sorted(const BinaryFunction& op = {}) -> cstream<C>& {
        std::sort(container_ref_.begin(), container_ref_.end(), op);
        return *this;
    }

    /**
     * @brief Transforms the container using a function.
     *
     * @tparam T The type of the destination container.
     * @tparam UnaryFunction The type of the transformation function.
     * @param transform_f The transformation function.
     * @return cstream<T> The transformed stream.
     */
    template <typename T, typename UnaryFunction>
    auto transform(UnaryFunction transform_f) const -> cstream<T> {
        T dest;
        dest.reverse(container_ref_.size());
        std::transform(container_ref_.begin(), container_ref_.end(),
                       std::back_inserter(dest), transform_f);
        return cstream<T>(std::move(dest));
    }

    /**
     * @brief Removes elements from the container based on a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param remove_f The predicate function.
     * @return cstream<C>& The stream with elements removed.
     */
    template <typename UnaryFunction>
    auto remove(UnaryFunction remove_f) -> cstream<C>& {
        auto new_end = std::remove_if(container_ref_.begin(),
                                      container_ref_.end(), remove_f);
        container_ref_.erase(new_end, container_ref_.end());
        return *this;
    }

    /**
     * @brief Erases a specific value from the container.
     *
     * @tparam ValueType The type of the value.
     * @param v The value to erase.
     * @return cstream<C>& The stream with the value erased.
     */
    template <typename ValueType>
    auto erase(const ValueType& v) -> cstream<C>& {
        auto new_end =
            std::remove(container_ref_.begin(), container_ref_.end(), v);
        container_ref_.erase(new_end, container_ref_.end());
        return *this;
    }

    /**
     * @brief Filters the container based on a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param filter The predicate function.
     * @return cstream<C>& The filtered stream.
     */
    template <typename UnaryFunction>
    auto filter(UnaryFunction filter_func) -> cstream<C>& {
        return remove(
            [&filter_func](const value_type& v) { return !filter_func(v); });
    }

    /**
     * @brief Creates a copy of the container and filters it based on a
     * predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param filter The predicate function.
     * @return cstream<C> The filtered stream.
     */
    template <typename UnaryFunction>
    auto cpFilter(UnaryFunction filter_func) const -> cstream<C> {
        C c;
        c.reserve(container_ref_.size());
        std::copy_if(container_ref_.begin(), container_ref_.end(),
                     std::back_inserter(c), filter_func);
        return cstream<C>(std::move(c));
    }

    /**
     * @brief Accumulates the elements of the container using a binary function.
     *
     * @tparam UnaryFunction The type of the binary function.
     * @param initial The initial value.
     * @param op The binary function.
     * @return value_type The accumulated value.
     */
    template <typename UnaryFunction = std::plus<value_type>>
    auto accumulate(value_type initial = {}, UnaryFunction op = {}) const
        -> value_type {
        return std::accumulate(container_ref_.begin(), container_ref_.end(),
                               initial, op);
    }

    /**
     * @brief Applies a function to each element of the container.
     *
     * @tparam UnaryFunction The type of the function.
     * @param f The function to apply.
     * @return cstream<C>& The stream.
     */
    template <typename UnaryFunction>
    auto forEach(UnaryFunction f) -> cstream<C>& {
        std::for_each(container_ref_.begin(), container_ref_.end(), f);
        return *this;
    }

    /**
     * @brief Checks if all elements satisfy a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param f The predicate function.
     * @return bool True if all elements satisfy the predicate, false otherwise.
     */
    template <typename UnaryFunction>
    auto all(UnaryFunction f) const -> bool {
        return std::all_of(container_ref_.begin(), container_ref_.end(), f);
    }

    /**
     * @brief Checks if any element satisfies a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param f The predicate function.
     * @return bool True if any element satisfies the predicate, false
     * otherwise.
     */
    template <typename UnaryFunction>
    auto any(UnaryFunction f) const -> bool {
        return std::any_of(container_ref_.begin(), container_ref_.end(), f);
    }

    /**
     * @brief Checks if no elements satisfy a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param f The predicate function.
     * @return bool True if no elements satisfy the predicate, false otherwise.
     */
    template <typename UnaryFunction>
    auto none(UnaryFunction f) const -> bool {
        return std::none_of(container_ref_.begin(), container_ref_.end(), f);
    }

    /**
     * @brief Creates a copy of the container.
     *
     * @return cstream<C> The copied stream.
     */
    auto copy() const -> cstream<C> { return cstream<C>{C(container_ref_)}; }

    /**
     * @brief Gets the size of the container.
     *
     * @return std::size_t The size of the container.
     */
    [[nodiscard]] auto size() const -> std::size_t {
        return container_ref_.size();
    }

    /**
     * @brief Counts the number of elements that satisfy a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param f The predicate function.
     * @return std::size_t The count of elements that satisfy the predicate.
     */
    template <typename UnaryFunction>
    auto count(UnaryFunction f) const -> std::size_t {
        return std::count_if(container_ref_.begin(), container_ref_.end(), f);
    }

    /**
     * @brief Counts the number of occurrences of a value.
     *
     * @param v The value to count.
     * @return std::size_t The count of the value.
     */
    auto count(const value_type& v) const -> std::size_t {
        return std::count(container_ref_.begin(), container_ref_.end(), v);
    }

    /**
     * @brief Checks if the container contains a value.
     *
     * @param value The value to check.
     * @return bool True if the container contains the value, false otherwise.
     */
    auto contains(const value_type& value) const -> bool {
        return std::find(container_ref_.begin(), container_ref_.end(), value) !=
               container_ref_.end();
    }

    /**
     * @brief Gets the minimum element in the container.
     *
     * @return value_type The minimum element.
     */
    auto min() const -> value_type {
        return *std::min_element(container_ref_.begin(), container_ref_.end());
    }

    /**
     * @brief Gets the maximum element in the container.
     *
     * @return value_type The maximum element.
     */
    auto max() const -> value_type {
        return *std::max_element(container_ref_.begin(), container_ref_.end());
    }

    /**
     * @brief Calculates the mean of the elements in the container.
     *
     * @return double The mean value.
     */
    [[nodiscard]] auto mean() const -> double {
        return static_cast<double>(accumulate()) / static_cast<double>(size());
    }

    /**
     * @brief Gets the first element in the container.
     *
     * @return std::optional<value_type> The first element, or std::nullopt if
     * the container is empty.
     */
    auto first() const -> std::optional<value_type> {
        if (container_ref_.empty()) {
            return std::nullopt;
        }
        return {*container_ref_.begin()};
    }

    /**
     * @brief Gets the first element that satisfies a predicate.
     *
     * @tparam UnaryFunction The type of the predicate function.
     * @param f The predicate function.
     * @return std::optional<value_type> The first element that satisfies the
     * predicate, or std::nullopt if no such element exists.
     */
    template <typename UnaryFunction>
    auto first(UnaryFunction f) const -> std::optional<value_type> {
        auto it = std::find_if(container_ref_.begin(), container_ref_.end(), f);
        if (it == container_ref_.end()) {
            return std::nullopt;
        }
        return {*it};
    }

    /**
     * @brief Maps the elements of the container using a function.
     *
     * @tparam UnaryFunction The type of the mapping function.
     * @param f The mapping function.
     * @return cstream<C> The mapped stream.
     */
    template <typename UnaryFunction>
    auto map(UnaryFunction f) const -> cstream<C> {
        C c;
        c.reserve(container_ref_.size());
        std::transform(container_ref_.begin(), container_ref_.end(),
                       std::back_inserter(c), f);
        return cstream<C>(std::move(c));
    }

    /**
     * @brief Flat maps the elements of the container using a function.
     *
     * @tparam UnaryFunction The type of the flat mapping function.
     * @param f The flat mapping function.
     * @return cstream<C> The flat mapped stream.
     */
    template <typename UnaryFunction>
    auto flatMap(UnaryFunction f) const -> cstream<C> {
        C c;
        for (const auto& item : container_ref_) {
            auto subContainer = f(item);
            c.insert(c.end(), subContainer.begin(), subContainer.end());
        }
        return cstream<C>(std::move(c));
    }

    /**
     * @brief Removes duplicate elements from the container.
     *
     * @return cstream<C>& The stream with duplicates removed.
     */
    auto distinct() -> cstream<C>& {
        std::sort(container_ref_.begin(), container_ref_.end());
        auto last = std::unique(container_ref_.begin(), container_ref_.end());
        container_ref_.erase(last, container_ref_.end());
        return *this;
    }

    /**
     * @brief Reverses the elements of the container.
     *
     * @return cstream<C>& The stream with elements reversed.
     */
    auto reverse() -> cstream<C>& {
        std::reverse(container_ref_.begin(), container_ref_.end());
        return *this;
    }

private:
    C moved_;
    C& container_ref_;
};

/**
 * @brief A functor for joining containers with a separator.
 *
 * @tparam C The type of the container.
 * @tparam Add The type of the addition function.
 */
template <typename C, typename Add = std::plus<C>>
struct JoinAccumulate {
    C separator;
    Add adder;

    /**
     * @brief Joins the source container into the destination container with a
     * separator.
     *
     * @param dest The destination container.
     * @param source The source container.
     * @return C The joined container.
     */
    auto operator()(C& dest, const C& source) const -> C {
        return dest.empty() ? source : adder(adder(dest, separator), source);
    }
};

/**
 * @brief A utility struct for working with pairs.
 *
 * @tparam A The type of the first element.
 * @tparam B The type of the second element.
 */
template <typename A, typename B>
struct Pair {
    /**
     * @brief Gets the first element of the pair.
     *
     * @param p The pair.
     * @return A The first element.
     */
    static auto first(const std::pair<A, B>& p) -> A { return p.first; }

    /**
     * @brief Gets the second element of the pair.
     *
     * @param p The pair.
     * @return B The second element.
     */
    static auto second(const std::pair<A, B>& p) -> B { return p.second; }
};

/**
 * @brief Creates a cstream from a container reference.
 *
 * @tparam T The type of the container.
 * @param t The container reference.
 * @return cstream<T> The created stream.
 */
template <typename T>
auto makeStream(T& t) -> cstream<T> {
    return cstream<T>{t};
}

/**
 * @brief Creates a cstream from a container rvalue.
 *
 * @tparam T The type of the container.
 * @param t The container rvalue.
 * @return cstream<T> The created stream.
 */
template <typename T>
auto makeStream(T&& t) -> cstream<T> {
    return cstream<T>{std::forward<T>(t)};
}

/**
 * @brief Creates a cstream from a container copy.
 *
 * @tparam T The type of the container.
 * @param t The container copy.
 * @return cstream<T> The created stream.
 */
template <typename T>
auto makeStreamCopy(const T& t) -> cstream<T> {
    return cstream<T>{T{t}};
}

/**
 * @brief Creates a cstream from a container.
 *
 * @tparam N The element type.
 * @tparam T The pointer type.
 * @param t The container pointer.
 * @param size The size of the container.
 * @return cstream<std::vector<N>> The created stream.
 */
template <typename N, typename T = N>
auto cpstream(const T* t, std::size_t size) -> cstream<std::vector<N>> {
    std::vector<N> data(t, t + size);
    return makeStream(std::move(data));
}

}  // namespace atom::type

#endif  // ATOM_TYPE_CONTAINERS_STREAMS_HPP
