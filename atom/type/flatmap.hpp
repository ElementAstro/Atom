/*
 * flatmap.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-2

Description: QuickFlatMap for C++20 with optional Boost support

**************************************************/

#ifndef ATOM_TYPE_FLATMAP_HPP
#define ATOM_TYPE_FLATMAP_HPP

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#else
#include <algorithm>
#include <concepts>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>
#endif

namespace atom::type {

#ifdef ATOM_USE_BOOST

template <typename Key, typename Value, typename Comparator = std::less<Key>>
class QuickFlatMap {
public:
    using value_type = std::pair<Key, Value>;
    using iterator =
        typename boost::container::flat_map<Key, Value, Comparator>::iterator;
    using const_iterator =
        typename boost::container::flat_map<Key, Value,
                                            Comparator>::const_iterator;

    QuickFlatMap() = default;

    template <typename Lookup>
    iterator find(const Lookup& s) noexcept {
        return data_.find(s);
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const noexcept {
        return data_.find(s);
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    iterator begin() noexcept { return data_.begin(); }

    const_iterator begin() const noexcept { return data_.begin(); }

    iterator end() noexcept { return data_.end(); }

    const_iterator end() const noexcept { return data_.end(); }

    Value& operator[](const Key& s) { return data_[s]; }

    Value& at(const Key& s) { return data_.at(s); }

    const Value& at(const Key& s) const { return data_.at(s); }

    template <typename M>
    std::pair<iterator, bool> insertOrAssign(const Key& key, M&& m) {
        return data_.insert_or_assign(key, std::forward<M>(m));
    }

    std::pair<iterator, bool> insert(value_type value) {
        return data_.insert(std::move(value));
    }

    template <typename Itr>
    void assign(Itr first, Itr last) {
        data_.insert(first, last);
    }

    bool contains(const Key& s) const noexcept { return data_.contains(s); }

    bool erase(const Key& s) { return data_.erase(s) > 0; }

private:
    boost::container::flat_map<Key, Value, Comparator> data_;
};

template <typename Key, typename Value, typename Comparator = std::less<Key>>
class QuickFlatMultiMap {
public:
    using value_type = std::pair<Key, Value>;
    using iterator =
        typename boost::container::flat_multimap<Key, Value,
                                                 Comparator>::iterator;
    using const_iterator =
        typename boost::container::flat_multimap<Key, Value,
                                                 Comparator>::const_iterator;

    QuickFlatMultiMap() = default;

    template <typename Lookup>
    iterator find(const Lookup& s) noexcept {
        return data_.find(s);
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const noexcept {
        return data_.find(s);
    }

    template <typename Lookup>
    std::pair<iterator, iterator> equalRange(const Lookup& s) noexcept {
        return data_.equal_range(s);
    }

    template <typename Lookup>
    std::pair<const_iterator, const_iterator> equalRange(
        const Lookup& s) const noexcept {
        return data_.equal_range(s);
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    iterator begin() noexcept { return data_.begin(); }

    const_iterator begin() const noexcept { return data_.begin(); }

    iterator end() noexcept { return data_.end(); }

    const_iterator end() const noexcept { return data_.end(); }

    Value& operator[](const Key& s) {
        return data_.emplace(s, Value())->second;
    }

    Value& at(const Key& s) {
        auto itr = data_.find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    const Value& at(const Key& s) const {
        auto itr = data_.find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    std::pair<iterator, bool> insert(value_type value) {
        auto itr = data_.insert(std::move(value));
        return {itr, true};
    }

    template <typename Itr>
    void assign(Itr first, Itr last) {
        data_.insert(first, last);
    }

    size_t count(const Key& s) const { return data_.count(s); }

    bool contains(const Key& s) const noexcept { return data_.contains(s); }

    bool erase(const Key& s) { return data_.erase(s) > 0; }

private:
    boost::container::flat_multimap<Key, Value, Comparator> data_;
};

#else
template <typename Key, typename Value, typename Comparator = std::equal_to<>>
    requires std::predicate<Comparator, Key, Key>
class QuickFlatMap {
public:
    using value_type = std::pair<Key, Value>;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    QuickFlatMap() = default;

    template <typename Lookup>
    iterator find(const Lookup& s) noexcept {
        return std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const noexcept {
        return std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    iterator begin() noexcept { return data_.begin(); }

    const_iterator begin() const noexcept { return data_.begin(); }

    iterator end() noexcept { return data_.end(); }

    const_iterator end() const noexcept { return data_.end(); }

    Value& operator[](const Key& s) {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        grow();
        return data_.emplace_back(s, Value()).second;
    }

    Value& at(const Key& s) {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    const Value& at(const Key& s) const {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    template <typename M>
    std::pair<iterator, bool> insertOrAssign(const Key& key, M&& m) {
        auto itr = find(key);
        if (itr != data_.end()) {
            itr->second = std::forward<M>(m);
            return {itr, false};
        }
        grow();
        return {data_.emplace(data_.end(), key, std::forward<M>(m)), true};
    }

    std::pair<iterator, bool> insert(value_type value) {
        auto itr = find(value.first);
        if (itr != data_.end()) {
            return {itr, false};
        }
        grow();
        return {data_.insert(data_.end(), std::move(value)), true};
    }

    template <typename Itr>
    void assign(Itr first, Itr last) {
        data_.assign(first, last);
    }

    void grow() {
        if (data_.capacity() == data_.size()) {
            data_.reserve(data_.size() + 2);
        }
    }

    bool contains(const Key& s) const noexcept {
        return find(s) != data_.end();
    }

    bool erase(const Key& s) {
        auto itr = find(s);
        if (itr != data_.end()) {
            data_.erase(itr);
            return true;
        }
        return false;
    }

private:
    std::vector<value_type> data_;
    Comparator comparator_;
};

template <typename Key, typename Value, typename Comparator = std::equal_to<>>
    requires std::predicate<Comparator, Key, Key>
class QuickFlatMultiMap {
public:
    using value_type = std::pair<Key, Value>;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    QuickFlatMultiMap() = default;

    template <typename Lookup>
    iterator find(const Lookup& s) noexcept {
        return std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
    }

    template <typename Lookup>
    const_iterator find(const Lookup& s) const noexcept {
        return std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
    }

    template <typename Lookup>
    std::pair<iterator, iterator> equalRange(const Lookup& s) noexcept {
        auto lower = std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
        if (lower == data_.end()) {
            return {lower, lower};
        }

        auto upper = std::ranges::find_if_not(
            lower, data_.end(),
            [&s, this](const auto& d) { return comparator_(d.first, s); });
        return {lower, upper};
    }

    template <typename Lookup>
    std::pair<const_iterator, const_iterator> equalRange(
        const Lookup& s) const noexcept {
        auto lower = std::ranges::find_if(data_, [&s, this](const auto& d) {
            return comparator_(d.first, s);
        });
        if (lower == data_.cend()) {
            return {lower, lower};
        }

        auto upper = std::ranges::find_if_not(
            lower, data_.cend(),
            [&s, this](const auto& d) { return comparator_(d.first, s); });
        return {lower, upper};
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    iterator begin() noexcept { return data_.begin(); }

    const_iterator begin() const noexcept { return data_.begin(); }

    iterator end() noexcept { return data_.end(); }

    const_iterator end() const noexcept { return data_.end(); }

    Value& operator[](const Key& s) {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        grow();
        return data_.emplace_back(s, Value()).second;
    }

    Value& at(const Key& s) {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    const Value& at(const Key& s) const {
        auto itr = find(s);
        if (itr != data_.end()) {
            return itr->second;
        }
        throw std::out_of_range("Unknown key");
    }

    std::pair<iterator, bool> insert(value_type value) {
        grow();
        return {data_.insert(data_.end(), std::move(value)), true};
    }

    template <typename Itr>
    void assign(Itr first, Itr last) {
        data_.assign(first, last);
    }

    void grow() {
        if (data_.capacity() == data_.size()) {
            data_.reserve(data_.size() + 2);
        }
    }

    size_t count(const Key& s) const {
        auto [lower, upper] = equalRange(s);
        return std::distance(lower, upper);
    }

    bool contains(const Key& s) const noexcept {
        return find(s) != data_.end();
    }

    bool erase(const Key& s) {
        auto [lower, upper] = equalRange(s);
        if (lower != upper) {
            data_.erase(lower, upper);
            return true;
        }
        return false;
    }

private:
    std::vector<value_type> data_;
    Comparator comparator_;
};

#endif

}  // namespace atom::type

#endif  // ATOM_TYPE_FLATMAP_HPP