// rust_iter.hpp
#pragma once

#include <cstddef>
#include <iterator>
#include <utility>

namespace atom::algorithm {

enum class Ordering { Less, Equal, Greater };

template <typename T>
class Ord {
public:
    static Ordering compare(const T& a, const T& b) {
        if (a < b)
            return Ordering::Less;
        if (a > b)
            return Ordering::Greater;
        return Ordering::Equal;
    }

    class Comparator {
    public:
        bool operator()(const T& a, const T& b) const {
            return compare(a, b) == Ordering::Less;
        }
    };

    template <typename F>
    static auto by_key(F&& key_fn) {
        class ByKey {
        private:
            F m_key_fn;

        public:
            ByKey(F key_fn) : m_key_fn(std::move(key_fn)) {}

            bool operator()(const T& a, const T& b) const {
                auto a_key = m_key_fn(a);
                auto b_key = m_key_fn(b);
                return a_key < b_key;
            }
        };

        return ByKey(std::forward<F>(key_fn));
    }
};

template <typename Iter, typename Func>
class MapIterator {
private:
    Iter m_iter;
    Func m_func;

public:
    using iterator_category =
        typename std::iterator_traits<Iter>::iterator_category;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using value_type = decltype(std::declval<Func>()(*std::declval<Iter>()));
    using pointer = value_type*;
    using reference = value_type&;

    MapIterator(Iter iter, Func func) : m_iter(iter), m_func(func) {}

    value_type operator*() const { return m_func(*m_iter); }

    MapIterator& operator++() {
        ++m_iter;
        return *this;
    }

    MapIterator operator++(int) {
        MapIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const MapIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const MapIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container, typename Func>
class Map {
private:
    Container& m_container;
    Func m_func;

public:
    Map(Container& container, Func func)
        : m_container(container), m_func(func) {}

    auto begin() { return MapIterator(m_container.begin(), m_func); }

    auto end() { return MapIterator(m_container.end(), m_func); }
};

template <typename Container, typename Func>
Map<Container, Func> map(Container& container, Func func) {
    return Map<Container, Func>(container, func);
}

template <typename Iter, typename Pred>
class FilterIterator {
private:
    Iter m_iter;
    Iter m_end;
    Pred m_pred;

    void find_next_valid() {
        while (m_iter != m_end && !m_pred(*m_iter)) {
            ++m_iter;
        }
    }

public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename std::iterator_traits<Iter>::value_type;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using pointer = typename std::iterator_traits<Iter>::pointer;
    using reference = typename std::iterator_traits<Iter>::reference;

    FilterIterator(Iter begin, Iter end, Pred pred)
        : m_iter(begin), m_end(end), m_pred(pred) {
        find_next_valid();
    }

    reference operator*() const { return *m_iter; }

    pointer operator->() const { return &(*m_iter); }

    FilterIterator& operator++() {
        if (m_iter != m_end) {
            ++m_iter;
            find_next_valid();
        }
        return *this;
    }

    FilterIterator operator++(int) {
        FilterIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const FilterIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const FilterIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container, typename Pred>
class Filter {
private:
    Container& m_container;
    Pred m_pred;

public:
    Filter(Container& container, Pred pred)
        : m_container(container), m_pred(pred) {}

    auto begin() {
        return FilterIterator(m_container.begin(), m_container.end(), m_pred);
    }

    auto end() {
        return FilterIterator(m_container.end(), m_container.end(), m_pred);
    }
};

template <typename Container, typename Pred>
Filter<Container, Pred> filter(Container& container, Pred pred) {
    return Filter<Container, Pred>(container, pred);
}

template <typename Iter>
class EnumerateIterator {
private:
    Iter m_iter;
    size_t m_index;

public:
    using iterator_category =
        typename std::iterator_traits<Iter>::iterator_category;
    using difference_type =
        typename std::iterator_traits<Iter>::difference_type;
    using value_type =
        std::pair<size_t, typename std::iterator_traits<Iter>::reference>;
    using pointer = value_type*;
    using reference = value_type;

    EnumerateIterator(Iter iter, size_t index = 0)
        : m_iter(iter), m_index(index) {}

    reference operator*() const { return {m_index, *m_iter}; }

    EnumerateIterator& operator++() {
        ++m_iter;
        ++m_index;
        return *this;
    }

    EnumerateIterator operator++(int) {
        EnumerateIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const EnumerateIterator& other) const {
        return m_iter == other.m_iter;
    }

    bool operator!=(const EnumerateIterator& other) const {
        return !(*this == other);
    }
};

template <typename Container>
class Enumerate {
private:
    Container& m_container;

public:
    explicit Enumerate(Container& container) : m_container(container) {}

    auto begin() { return EnumerateIterator(m_container.begin()); }

    auto end() { return EnumerateIterator(m_container.end()); }
};

template <typename Container>
Enumerate<Container> enumerate(Container& container) {
    return Enumerate<Container>(container);
}

}  // namespace atom::algorithm
