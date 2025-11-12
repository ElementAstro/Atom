/* Minimal, clean high_performance facade.
 * This trimmed header provides std-based fallbacks so the project can
 * compile while richer Boost-backed implementations are rebuilt.
 */

#pragma once

#include "../macro.hpp"  // IWYU: keep

#include <array>
#include <cstddef>
#include <deque>
#include <map>
#include <memory_resource>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace atom::containers::hp {

// Conservative aliases that keep existing call sites compiling while the
// richer Boost-backed versions are reconstructed.

template <typename K, typename V, typename Compare = std::less<K>>
using flat_map = std::map<K, V, Compare>;

template <typename K, typename Compare = std::less<K>>
using flat_set = std::set<K, Compare>;

template <typename T>
using stable_vector = std::deque<T>;

template <typename T, std::size_t InlineCapacity>
class small_vector {
public:
    using value_type = T;
    using size_type = std::size_t;

    small_vector() { data_.reserve(InlineCapacity); }
    explicit small_vector(size_type n, const T& value = T())
        : data_(n, value) {}

    void push_back(const T& value) { data_.push_back(value); }
    void push_back(T&& value) { data_.push_back(std::move(value)); }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        return data_.emplace_back(std::forward<Args>(args)...);
    }

    void pop_back() { data_.pop_back(); }

    void clear() noexcept { data_.clear(); }
    void reserve(size_type count) { data_.reserve(count); }

    [[nodiscard]] size_type size() const noexcept { return data_.size(); }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    [[nodiscard]] auto begin() noexcept { return data_.begin(); }
    [[nodiscard]] auto end() noexcept { return data_.end(); }
    [[nodiscard]] auto begin() const noexcept { return data_.begin(); }
    [[nodiscard]] auto end() const noexcept { return data_.end(); }

    [[nodiscard]] T* data() noexcept { return data_.data(); }
    [[nodiscard]] const T* data() const noexcept { return data_.data(); }

    [[nodiscard]] size_type capacity() const noexcept {
        return data_.capacity();
    }

    T& operator[](size_type pos) { return data_[pos]; }
    [[nodiscard]] const T& operator[](size_type pos) const {
        return data_[pos];
    }

    [[nodiscard]] T& front() { return data_.front(); }
    [[nodiscard]] const T& front() const { return data_.front(); }

    [[nodiscard]] T& back() { return data_.back(); }
    [[nodiscard]] const T& back() const { return data_.back(); }

private:
    std::vector<T> data_{};
};

template <typename T, std::size_t N>
using static_vector = std::array<T, N>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using fast_unordered_map = std::unordered_map<K, V, Hash, Eq>;

template <typename K, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using fast_unordered_set = std::unordered_set<K, Hash, Eq>;

using bstring = std::string;

namespace pmr {
template <typename T>
using vector = std::pmr::vector<T>;

template <typename K, typename V, typename C = std::less<K>>
using map = std::pmr::map<K, V, C>;

template <typename K, typename V, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using unordered_map = std::pmr::unordered_map<K, V, Hash, Eq>;

template <typename K, typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>>
using unordered_set = std::pmr::unordered_set<K, Hash, Eq>;
}  // namespace pmr

}  // namespace atom::containers::hp

namespace atom::containers {

template <typename K, typename V>
using HashMap = hp::fast_unordered_map<K, V>;

template <typename T>
using HashSet = hp::fast_unordered_set<T>;

template <typename T>
using Vector = std::vector<T>;

template <typename K, typename V>
using Map = hp::flat_map<K, V>;

template <typename T, std::size_t N = 16>
using SmallVector = std::vector<T>;

using String = std::string;

}  // namespace atom::containers
