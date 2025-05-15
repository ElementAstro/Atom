#ifndef ATOM_UTILS_LINQ_HPP
#define ATOM_UTILS_LINQ_HPP

#include <algorithm>
#include <list>
#include <numeric>
#include <optional>
#include <ranges>

// Import high-performance containers
#include "atom/containers/high_performance.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_set.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#endif

namespace atom::utils {

// Helper to deduce return type for selectMany
template <typename T>
auto flatten(
    const atom::containers::Vector<atom::containers::Vector<T>>& nested) {
    atom::containers::Vector<T> flat;
    for (const auto& sublist : nested) {
        flat.insert(flat.end(), sublist.begin(), sublist.end());
    }
    return flat;
}

template <typename T>
class Enumerable {
public:
    explicit Enumerable(const atom::containers::Vector<T>& elements)
        : elements_(elements) {}

    // ======== Filters and Reorders ========
    // Filter
    [[nodiscard]] auto where(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
#ifdef ATOM_USE_BOOST
        boost::copy_if(elements_, std::back_inserter(result), predicate);
#else
        for (const T& element : elements_ | std::views::filter(predicate)) {
            result.push_back(element);
        }
#endif
        return Enumerable(result);
    }

    template <typename U, typename BinaryOperation>
    [[nodiscard]] auto reduce(U init, BinaryOperation binary_op) const -> U {
        return std::accumulate(elements_.begin(), elements_.end(), init,
                               binary_op);
    }

    [[nodiscard]] auto whereI(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        for (size_t index = 0; index < elements_.size(); ++index) {
            if (predicate(elements_[index], index)) {
                result.push_back(elements_[index]);
            }
        }
        return Enumerable(result);
    }

    [[nodiscard]] auto take(size_t count) const -> Enumerable<T> {
        return Enumerable(atom::containers::Vector<T>(
            elements_.begin(),
            elements_.begin() + std::min(count, elements_.size())));
    }

    [[nodiscard]] auto takeWhile(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
#ifdef ATOM_USE_BOOST
        for (const auto& element : elements_) {
            if (!predicate(element)) {
                break;
            }
            result.push_back(element);
        }
#else
        for (const auto& element : elements_) {
            if (!predicate(element)) {
                break;
            }
            result.push_back(element);
        }
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto takeWhileI(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        for (size_t index = 0; index < elements_.size(); ++index) {
            if (!predicate(elements_[index], index)) {
                break;
            }
            result.push_back(elements_[index]);
        }
        return Enumerable(result);
    }

    [[nodiscard]] auto skip(size_t count) const -> Enumerable<T> {
        return Enumerable(atom::containers::Vector<T>(
            elements_.begin() + std::min(count, elements_.size()),
            elements_.end()));
    }

    [[nodiscard]] auto skipWhile(auto predicate) const -> Enumerable<T> {
        auto iterator =
#ifdef ATOM_USE_BOOST
            boost::find_if_not(elements_, predicate);
#else
            std::find_if_not(elements_.begin(), elements_.end(), predicate);
#endif
        return Enumerable(
            atom::containers::Vector<T>(iterator, elements_.end()));
    }

    [[nodiscard]] auto skipWhileI(auto predicate) const -> Enumerable<T> {
        auto iterator =
#ifdef ATOM_USE_BOOST
            boost::find_if_not(elements_, [&](const T& element) -> bool {
                return predicate(element, &element - &elements_[0]);
            });
#else
            std::find_if_not(elements_.begin(), elements_.end(),
                             [index = 0, &predicate](const T& element) mutable {
                                 return predicate(element, index++);
                             });
#endif
        return Enumerable(
            atom::containers::Vector<T>(iterator, elements_.end()));
    }

    [[nodiscard]] auto orderBy() const -> Enumerable<T> {
        atom::containers::Vector<T> result = elements_;
#ifdef ATOM_USE_BOOST
        boost::sort(result);
#else
        std::sort(result.begin(), result.end());
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto orderBy(auto transformer) const -> Enumerable<T> {
        atom::containers::Vector<T> result = elements_;
#ifdef ATOM_USE_BOOST
        boost::sort(result, [&](const T& a, const T& b) {
            return transformer(a) < transformer(b);
        });
#else
        std::sort(result.begin(), result.end(),
                  [&transformer](const T& elementA, const T& elementB) {
                      return transformer(elementA) < transformer(elementB);
                  });
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto distinct() const -> Enumerable<T> {
        atom::containers::Vector<T> result;
#ifdef ATOM_USE_BOOST
        boost::container::flat_set<T> set(elements_.begin(), elements_.end());
        boost::copy(set, std::back_inserter(result));
#else
        atom::containers::HashSet<T> set(elements_.begin(), elements_.end());
        result.assign(set.begin(), set.end());
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto distinct(auto transformer) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
#ifdef ATOM_USE_BOOST
        boost::container::flat_set<
            std::invoke_result_t<decltype(transformer), T>>
            set;
        for (const auto& element : elements_) {
            auto transformed = transformer(element);
            if (set.insert(transformed).second) {
                result.push_back(element);
            }
        }
#else
        atom::containers::HashSet<
            std::invoke_result_t<decltype(transformer), T>>
            set(/*bucket_count=*/16,
                /*hash=*/
                std::hash<std::invoke_result_t<decltype(transformer), T>>(),
                /*equal=*/
                std::equal_to<
                    std::invoke_result_t<decltype(transformer), T>>());
        for (const auto& element : elements_) {
            auto transformed = transformer(element);
            if (set.insert(transformed).second) {
                result.push_back(element);
            }
        }
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto append(const atom::containers::Vector<T>& items) const
        -> Enumerable<T> {
        atom::containers::Vector<T> result = elements_;
#ifdef ATOM_USE_BOOST
        boost::copy(items, std::back_inserter(result));
#else
        result.insert(result.end(), items.begin(), items.end());
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto prepend(const atom::containers::Vector<T>& items) const
        -> Enumerable<T> {
        atom::containers::Vector<T> result = items;
#ifdef ATOM_USE_BOOST
        boost::copy(elements_, std::back_inserter(result));
#else
        result.insert(result.end(), elements_.begin(), elements_.end());
#endif
        return Enumerable(result);
    }

    [[nodiscard]] auto concat(const Enumerable<T>& other) const
        -> Enumerable<T> {
        return append(other.elements_);
    }

    [[nodiscard]] auto reverse() const -> Enumerable<T> {
        atom::containers::Vector<T> result = elements_;
#ifdef ATOM_USE_BOOST
        boost::reverse(result);
#else
        std::reverse(result.begin(), result.end());
#endif
        return Enumerable(result);
    }

    template <typename U>
    [[nodiscard]] auto cast() const -> Enumerable<U> {
        atom::containers::Vector<U> result;
#ifdef ATOM_USE_BOOST
        boost::transform(
            elements_, std::back_inserter(result),
            [](const T& element) -> U { return static_cast<U>(element); });
#else
        result.reserve(elements_.size());
        for (const T& element : elements_) {
            result.push_back(static_cast<U>(element));
        }
#endif
        return Enumerable<U>(result);
    }

    // ======== Transformers ========
    template <typename U>
    [[nodiscard]] auto select(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<U> result;
#ifdef ATOM_USE_BOOST
        boost::transform(elements_, std::back_inserter(result), transformer);
#else
        result.reserve(elements_.size());
        for (const auto& element : elements_) {
            result.push_back(transformer(element));
        }
#endif
        return Enumerable<U>(result);
    }

    template <typename U>
    [[nodiscard]] auto selectI(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<U> result;
        result.reserve(elements_.size());
        for (size_t index = 0; index < elements_.size(); ++index) {
            result.push_back(transformer(elements_[index], index));
        }
        return Enumerable<U>(result);
    }

    template <typename U>
    [[nodiscard]] auto groupBy(auto transformer) const -> Enumerable<U> {
        // Initialize the hash map with default values to support
        // non-default-constructible keys like pybind11::object
        atom::containers::HashMap<U, atom::containers::Vector<T>> groups(
            /*bucket_count=*/16,
            /*hash=*/std::hash<U>(),
            /*equal=*/std::equal_to<U>());
#ifdef ATOM_USE_BOOST
        boost::for_each(elements_, [&](const T& element) {
            groups[transformer(element)].push_back(element);
        });
#else
        for (const T& element : elements_) {
            groups[transformer(element)].push_back(element);
        }
#endif
        atom::containers::Vector<U> keys;
#ifdef ATOM_USE_BOOST
        for (const auto& group : groups) {
            keys.push_back(group.first);
        }
#else
        keys.reserve(groups.size());
        for (const auto& group : groups) {
            keys.push_back(group.first);
        }
#endif
        return Enumerable<U>(keys);
    }

    template <typename U>
    [[nodiscard]] auto selectMany(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<atom::containers::Vector<U>> nested;
#ifdef ATOM_USE_BOOST
        boost::transform(elements_, std::back_inserter(nested), transformer);
#else
        nested.reserve(elements_.size());
        for (const T& element : elements_) {
            nested.push_back(transformer(element));
        }
#endif
        return Enumerable<U>(flatten(nested));
    }

    // ======== Aggregators ========
    [[nodiscard]] auto all(auto predicate = [](const T&) { return true; }) const
        -> bool {
#ifdef ATOM_USE_BOOST
        return boost::all_of(elements_, predicate);
#else
        return std::all_of(elements_.begin(), elements_.end(), predicate);
#endif
    }

    [[nodiscard]] auto any(auto predicate = [](const T&) { return true; }) const
        -> bool {
#ifdef ATOM_USE_BOOST
        return boost::any_of(elements_, predicate);
#else
        return std::any_of(elements_.begin(), elements_.end(), predicate);
#endif
    }

    [[nodiscard]] auto sum() const -> T {
        return std::accumulate(elements_.begin(), elements_.end(), T{});
    }

    template <typename U>
    [[nodiscard]] auto sum(auto transformer) const -> U {
        U result{};
#ifdef ATOM_USE_BOOST
        boost::for_each(elements_, [&](const T& element) {
            result += transformer(element);
        });
#else
        for (const auto& element : elements_) {
            result += transformer(element);
        }
#endif
        return result;
    }

    [[nodiscard]] auto avg() const -> double {
        if (elements_.empty())
            return 0.0;

        if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(sum()) / elements_.size();
        } else {
            // For non-numeric types, return count as double
            return static_cast<double>(elements_.size());
        }
    }

    template <typename U>
    [[nodiscard]] auto avg(auto transformer) const -> U {
        return elements_.empty()
                   ? U{}
                   : sum<U>(transformer) / static_cast<U>(elements_.size());
    }

    [[nodiscard]] auto min() const -> T {
        if (elements_.empty()) {
            return T{};
        }
#ifdef ATOM_USE_BOOST
        return *boost::min_element(elements_);
#else
        return *std::min_element(elements_.begin(), elements_.end());
#endif
    }

    [[nodiscard]] auto min(auto transformer) const -> T {
        if (elements_.empty()) {
            return T{};
        }
#ifdef ATOM_USE_BOOST
        return *boost::min_element(elements_, [&](const T& a, const T& b) {
            return transformer(a) < transformer(b);
        });
#else
        return *std::min_element(
            elements_.begin(), elements_.end(),
            [&transformer](const T& elementA, const T& elementB) {
                return transformer(elementA) < transformer(elementB);
            });
#endif
    }

    [[nodiscard]] auto max() const -> T {
        if (elements_.empty()) {
            return T{};
        }
#ifdef ATOM_USE_BOOST
        return *boost::max_element(elements_);
#else
        return *std::max_element(elements_.begin(), elements_.end());
#endif
    }

    [[nodiscard]] auto max(auto transformer) const -> T {
        if (elements_.empty()) {
            return T{};
        }
#ifdef ATOM_USE_BOOST
        return *boost::max_element(elements_, [&](const T& a, const T& b) {
            return transformer(a) < transformer(b);
        });
#else
        return *std::max_element(
            elements_.begin(), elements_.end(),
            [&transformer](const T& elementA, const T& elementB) {
                return transformer(elementA) < transformer(elementB);
            });
#endif
    }

    [[nodiscard]] auto count() const -> size_t { return elements_.size(); }

    [[nodiscard]] auto count(auto predicate) const -> size_t {
#ifdef ATOM_USE_BOOST
        return boost::count_if(elements_, predicate);
#else
        return std::count_if(elements_.begin(), elements_.end(), predicate);
#endif
    }

    [[nodiscard]] auto contains(const T& value) const -> bool {
#ifdef ATOM_USE_BOOST
        return boost::contains(elements_, value);
#else
        return std::find(elements_.begin(), elements_.end(), value) !=
               elements_.end();
#endif
    }

    [[nodiscard]] auto elementAt(size_t index) const -> T {
        return elements_.at(index);
    }

    [[nodiscard]] auto first() const -> T {
        return elements_.empty() ? T{} : elements_.front();
    }

    [[nodiscard]] auto first(auto predicate) const -> T {
#ifdef ATOM_USE_BOOST
        auto it = boost::find_if(elements_, predicate);
#else
        auto it = std::find_if(elements_.begin(), elements_.end(), predicate);
#endif
        return it != elements_.end() ? *it : T{};
    }

    [[nodiscard]] auto firstOrDefault() const -> std::optional<T> {
        return elements_.empty() ? std::nullopt
                                 : std::optional<T>(elements_.front());
    }

    [[nodiscard]] auto firstOrDefault(auto predicate) const
        -> std::optional<T> {
#ifdef ATOM_USE_BOOST
        auto it = boost::find_if(elements_, predicate);
#else
        auto it = std::find_if(elements_.begin(), elements_.end(), predicate);
#endif
        return it != elements_.end() ? std::optional<T>(*it) : std::nullopt;
    }

    [[nodiscard]] auto last() const -> T {
        return elements_.empty() ? T{} : elements_.back();
    }

    [[nodiscard]] auto last(auto predicate) const -> T {
#ifdef ATOM_USE_BOOST
        auto it =
            boost::find_if(elements_ | boost::adaptors::reverse, predicate);
#else
        auto it = std::find_if(elements_.rbegin(), elements_.rend(), predicate);
#endif
        return it != elements_.rend() ? *it : T{};
    }

    [[nodiscard]] auto lastOrDefault() const -> std::optional<T> {
        return elements_.empty() ? std::nullopt
                                 : std::optional<T>(elements_.back());
    }

    [[nodiscard]] auto lastOrDefault(auto predicate) const -> std::optional<T> {
#ifdef ATOM_USE_BOOST
        auto it =
            boost::find_if(elements_ | boost::adaptors::reverse, predicate);
#else
        auto it = std::find_if(elements_.rbegin(), elements_.rend(), predicate);
#endif
        return it != elements_.rend() ? std::optional<T>(*it) : std::nullopt;
    }

    // ======== Conversion Methods ========
    [[nodiscard]] auto toSet() const -> atom::containers::HashSet<T> {
        return atom::containers::HashSet<T>(elements_.begin(), elements_.end());
    }

    [[nodiscard]] auto toVector() const -> atom::containers::Vector<T> {
        return elements_;
    }

    // Standard container conversions kept for compatibility
    [[nodiscard]] auto toStdSet() const -> std::set<T> {
        return std::set<T>(elements_.begin(), elements_.end());
    }

    [[nodiscard]] auto toStdList() const -> std::list<T> {
        return std::list<T>(elements_.begin(), elements_.end());
    }

    [[nodiscard]] auto toStdDeque() const -> std::deque<T> {
        return std::deque<T>(elements_.begin(), elements_.end());
    }

    [[nodiscard]] auto toStdVector() const -> std::vector<T> {
        return std::vector<T>(elements_.begin(), elements_.end());
    }

    // ======== Printing ========
    void print() const {
#ifdef __DEBUG__
        for (const T& element : elements_) {
            std::cout << element << " ";
        }
        std::cout << std::endl;
#endif  // __DEBUG__
    }

private:
    atom::containers::Vector<T> elements_;
};

// Create an Enumerable from a range/container
template <typename Container>
auto from(const Container& container) {
    atom::containers::Vector<typename Container::value_type> vec(
        container.begin(), container.end());
    return Enumerable<typename Container::value_type>(vec);
}

// Create an Enumerable from initializer list
template <typename T>
auto from(std::initializer_list<T> items) {
    atom::containers::Vector<T> vec(items.begin(), items.end());
    return Enumerable<T>(vec);
}

// Range generator
template <typename T>
auto range(T start, T end, T step = 1) {
    atom::containers::Vector<T> result;
    for (T i = start; i < end; i += step) {
        result.push_back(i);
    }
    return Enumerable<T>(result);
}

}  // namespace atom::utils

#endif
