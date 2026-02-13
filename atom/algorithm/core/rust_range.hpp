// rust_range.hpp
#pragma once

#include <cstddef>
#include <iterator>

#include "rust_types.hpp"

namespace atom::algorithm {

template <typename T>
class Range {
private:
    T m_start;
    T m_end;
    bool m_inclusive;

public:
    class Iterator {
    private:
        T m_current;
        T m_end;
        bool m_inclusive;
        bool m_done;

    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::input_iterator_tag;

        Iterator(T start, T end, bool inclusive)
            : m_current(start),
              m_end(end),
              m_inclusive(inclusive),
              m_done(start > end || (start == end && !inclusive)) {}

        T operator*() const { return m_current; }

        Iterator& operator++() {
            if (m_current == m_end) {
                if (m_inclusive) {
                    m_done = true;
                    m_inclusive = false;
                }
            } else {
                ++m_current;
                m_done =
                    (m_current > m_end) || (m_current == m_end && !m_inclusive);
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const {
            if (m_done && other.m_done)
                return true;
            if (m_done || other.m_done)
                return false;
            return m_current == other.m_current && m_end == other.m_end &&
                   m_inclusive == other.m_inclusive;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
    };

    Range(T start, T end, bool inclusive = false)
        : m_start(start), m_end(end), m_inclusive(inclusive) {}

    Iterator begin() const { return Iterator(m_start, m_end, m_inclusive); }
    Iterator end() const { return Iterator(m_end, m_end, false); }

    bool contains(const T& value) const {
        if (m_inclusive) {
            return value >= m_start && value <= m_end;
        } else {
            return value >= m_start && value < m_end;
        }
    }

    usize len() const {
        if (m_start > m_end)
            return 0;
        usize length = static_cast<usize>(m_end - m_start);
        if (m_inclusive)
            length += 1;
        return length;
    }

    bool is_empty() const {
        return m_start >= m_end && !(m_inclusive && m_start == m_end);
    }
};

template <typename T>
Range<T> range(T start, T end) {
    return Range<T>(start, end, false);
}

template <typename T>
Range<T> range_inclusive(T start, T end) {
    return Range<T>(start, end, true);
}

}  // namespace atom::algorithm
