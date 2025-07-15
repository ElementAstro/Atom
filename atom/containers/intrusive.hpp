/*
 * atom/containers/intrusive.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: Boost Intrusive Containers

**************************************************/

#pragma once

#include "../macro.hpp"

// Enable only if ATOM_HAS_BOOST_INTRUSIVE is defined and Boost intrusive
// library is available
#if defined(ATOM_HAS_BOOST_INTRUSIVE)

#include <boost/functional/hash.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/unordered_set.hpp>

namespace atom {
namespace containers {
namespace intrusive {

// Define common list hooks
using list_base_hook = boost::intrusive::list_base_hook<>;
using set_base_hook = boost::intrusive::set_base_hook<>;
using unordered_set_base_hook = boost::intrusive::unordered_set_base_hook<>;
using slist_base_hook = boost::intrusive::slist_base_hook<>;

/**
 * @brief Intrusive list
 *
 * Intrusive list requires element types to contain a hook, avoiding additional
 * memory allocation. Very suitable for managing large numbers of objects,
 * reducing memory fragmentation and improving cache performance.
 *
 * Usage example:
 * class MyClass : public atom::containers::intrusive::list_base_hook {
 *   // Class members and methods
 * };
 *
 * atom::containers::intrusive::list<MyClass> my_list;
 *
 * @tparam T Element type that must inherit from list_base_hook
 */
template <typename T>
using list = boost::intrusive::list<T>;

/**
 * @brief Intrusive singly-linked list
 *
 * Lighter than doubly-linked list, but only supports forward traversal
 *
 * @tparam T Element type that must inherit from slist_base_hook
 */
template <typename T>
using slist = boost::intrusive::slist<T>;

/**
 * @brief Intrusive ordered set
 *
 * Elements are sorted by key, providing fast lookup while avoiding memory
 * allocation overhead
 *
 * @tparam T Element type that must inherit from set_base_hook
 * @tparam Compare Function object type for comparing elements
 */
template <typename T, typename Compare = std::less<T>>
using set = boost::intrusive::set<T, boost::intrusive::compare<Compare>>;

/**
 * @brief Intrusive unordered set
 *
 * Implements fast lookup through hashing, avoiding node allocation overhead of
 * standard unordered containers
 *
 * @tparam T Element type that must inherit from unordered_set_base_hook
 * @tparam Hash Hash function object type
 * @tparam Equal Function object type for element equality comparison
 */
template <typename T, typename Hash = boost::hash<T>,
          typename Equal = std::equal_to<T>>
class unordered_set {
private:
    // Basic configuration for hash table buckets
    static constexpr std::size_t NumBuckets = 128;
    using bucket_type = boost::intrusive::unordered_set<T>::bucket_type;
    bucket_type buckets_[NumBuckets];

    using unordered_set_type = boost::intrusive::unordered_set<
        T, boost::intrusive::hash<Hash>, boost::intrusive::equal<Equal>,
        boost::intrusive::constant_time_size<true>>;

    unordered_set_type set_;

public:
    using iterator = typename unordered_set_type::iterator;
    using const_iterator = typename unordered_set_type::const_iterator;

    unordered_set()
        : set_(boost::intrusive::bucket_traits(buckets_, NumBuckets)) {}

    /**
     * @brief Insert element into unordered set
     *
     * @param value Element to insert
     * @return std::pair<iterator, bool>
     * Contains iterator to inserted element and flag indicating successful
     * insertion
     */
    std::pair<iterator, bool> insert(T& value) { return set_.insert(value); }

    /**
     * @brief Remove element from unordered set
     *
     * @param value Element to remove
     * @return bool Returns true if element was removed
     */
    bool remove(T& value) { return set_.erase(value) > 0; }

    /**
     * @brief Find element
     *
     * @param value Element to find
     * @return iterator Iterator to found element, returns end() if not found
     */
    iterator find(const T& value) { return set_.find(value); }

    /**
     * @brief Return begin iterator
     */
    iterator begin() { return set_.begin(); }

    /**
     * @brief Return end iterator
     */
    iterator end() { return set_.end(); }

    /**
     * @brief Check if container is empty
     */
    bool empty() const { return set_.empty(); }

    /**
     * @brief Return number of elements in container
     */
    std::size_t size() const { return set_.size(); }

    /**
     * @brief Clear container
     */
    void clear() { set_.clear(); }
};

/**
 * @brief Helper base class for linkable types
 *
 * This class simplifies creating objects that support multiple intrusive
 * containers. If you need an object that can be placed in list, set, and
 * unordered_set simultaneously, you can inherit from this class.
 */
class intrusive_base : public list_base_hook,
                       public set_base_hook,
                       public unordered_set_base_hook,
                       public slist_base_hook {
protected:
    // Protected constructor to prevent direct instantiation
    intrusive_base() = default;

    // Allow derived class destruction
    virtual ~intrusive_base() = default;

    // Disable copying
    intrusive_base(const intrusive_base&) = delete;
    intrusive_base& operator=(const intrusive_base&) = delete;

    // Enable moving
    intrusive_base(intrusive_base&&) = default;
    intrusive_base& operator=(intrusive_base&&) = default;
};

}  // namespace intrusive
}  // namespace containers
}  // namespace atom

#endif  // defined(ATOM_HAS_BOOST_INTRUSIVE)
