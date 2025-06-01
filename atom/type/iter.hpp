/*!
 * \file iter.hpp
 * \brief High-performance iterator utilities and adapters
 * \author Max Qian <lightapt.com>
 * \date 2024-4-26
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_EXPERIMENTAL_ITERATOR_HPP
#define ATOM_EXPERIMENTAL_ITERATOR_HPP

#include <algorithm>
#include <iterator>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#if ENABLE_DEBUG
#include <iostream>
#endif

/*!
 * \brief An iterator that returns pointers to the elements of another iterator
 * \tparam IteratorT The type of the underlying iterator
 */
template <typename IteratorT>
class PointerIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = decltype(&*std::declval<IteratorT>());
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

private:
    IteratorT iter_;

public:
    constexpr PointerIterator() = default;

    /*!
     * \brief Constructs a PointerIterator from an underlying iterator
     * \param iterator The underlying iterator
     */
    constexpr explicit PointerIterator(IteratorT iterator) noexcept(
        std::is_nothrow_move_constructible_v<IteratorT>)
        : iter_(std::move(iterator)) {}

    /*!
     * \brief Dereferences the iterator to return a pointer to the element
     * \return A pointer to the element
     */
    constexpr auto operator*() const noexcept -> value_type { return &*iter_; }

    /*!
     * \brief Pre-increment operator
     * \return Reference to the incremented iterator
     */
    constexpr auto operator++() noexcept -> PointerIterator& {
        ++iter_;
        return *this;
    }

    /*!
     * \brief Post-increment operator
     * \return A copy of the iterator before incrementing
     */
    constexpr auto operator++(int) noexcept -> PointerIterator {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    /*!
     * \brief Equality comparison operator
     * \param other The other iterator to compare with
     * \return True if the iterators are equal, false otherwise
     */
    constexpr auto operator==(const PointerIterator& other) const noexcept
        -> bool = default;
};

/*!
 * \brief Creates a range of PointerIterator from two iterators
 * \tparam IteratorT The type of the underlying iterator
 * \param begin The beginning of the range
 * \param end The end of the range
 * \return A pair of PointerIterator representing the range
 */
template <typename IteratorT>
constexpr auto makePointerRange(IteratorT begin, IteratorT end) noexcept {
    return std::make_pair(PointerIterator<IteratorT>(begin),
                          PointerIterator<IteratorT>(end));
}

/*!
 * \brief Processes a container by erasing elements pointed to by the iterators
 * \tparam ContainerT The type of the container
 * \param container The container to process
 */
template <typename ContainerT>
void processContainer(ContainerT& container) {
    if (container.size() <= 2)
        return;

    auto beginIter = std::next(container.begin());
    auto endIter = std::prev(container.end());

    std::vector<std::optional<typename ContainerT::value_type*>> ptrs;
    ptrs.reserve(std::distance(beginIter, endIter));

    auto ptrPair = makePointerRange(beginIter, endIter);
    for (auto iter = ptrPair.first; iter != ptrPair.second; ++iter) {
        ptrs.push_back(*iter);
    }

    for (auto& ptrOpt : ptrs) {
        if (ptrOpt) {
            auto ptr = *ptrOpt;
#if ENABLE_DEBUG
            std::cout << "pointer addr: " << static_cast<const void*>(&ptr)
                      << '\n';
            std::cout << "point to: " << static_cast<const void*>(ptr) << '\n';
            std::cout << "value: " << *ptr << '\n';
#endif
            container.erase(
                std::find(container.begin(), container.end(), *ptr));
        }
    }
}

/*!
 * \brief An iterator that increments the underlying iterator early
 * \tparam I The type of the underlying iterator
 */
template <std::input_or_output_iterator I>
class EarlyIncIterator {
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;

    constexpr EarlyIncIterator() = default;

    /*!
     * \brief Constructs an EarlyIncIterator from an underlying iterator
     * \param iterator The underlying iterator
     */
    constexpr explicit EarlyIncIterator(I iterator) noexcept(
        std::is_nothrow_move_constructible_v<I>)
        : current_(std::move(iterator)) {}

    /*!
     * \brief Pre-increment operator
     * \return Reference to the incremented iterator
     */
    constexpr auto operator++() noexcept -> EarlyIncIterator& {
        ++current_;
        return *this;
    }

    /*!
     * \brief Post-increment operator
     * \return A copy of the iterator before incrementing
     */
    constexpr auto operator++(int) noexcept -> EarlyIncIterator {
        auto tmp = *this;
        ++current_;
        return tmp;
    }

    /*!
     * \brief Equality comparison operator
     * \param iter1 The first iterator to compare
     * \param iter2 The second iterator to compare
     * \return True if the iterators are equal, false otherwise
     */
    friend constexpr auto operator==(const EarlyIncIterator& iter1,
                                     const EarlyIncIterator& iter2) noexcept
        -> bool {
        return iter1.current_ == iter2.current_;
    }

    /*!
     * \brief Dereferences the iterator
     * \return The value pointed to by the underlying iterator
     */
    constexpr auto operator*() const noexcept(noexcept(*std::declval<I>())) {
        return *current_;
    }

private:
    I current_{};
};

/*!
 * \brief Creates an EarlyIncIterator from an underlying iterator
 * \tparam I The type of the underlying iterator
 * \param iterator The underlying iterator
 * \return An EarlyIncIterator
 */
template <std::input_or_output_iterator I>
constexpr auto makeEarlyIncIterator(I iterator) noexcept
    -> EarlyIncIterator<I> {
    return EarlyIncIterator<I>(std::move(iterator));
}

/*!
 * \brief An iterator that applies a transformation function to the elements
 * \tparam IteratorT The type of the underlying iterator
 * \tparam FuncT The type of the transformation function
 */
template <typename IteratorT, typename FuncT>
class TransformIterator {
public:
    using iterator_category =
        typename std::iterator_traits<IteratorT>::iterator_category;
    using value_type = std::invoke_result_t<
        FuncT, typename std::iterator_traits<IteratorT>::reference>;
    using difference_type =
        typename std::iterator_traits<IteratorT>::difference_type;
    using pointer = value_type*;
    using reference = value_type;

private:
    IteratorT iter_;
    FuncT func_;

public:
    constexpr TransformIterator() = default;

    /*!
     * \brief Constructs a TransformIterator from an underlying iterator and a
     * transformation function
     * \param iterator The underlying iterator
     * \param function The transformation function
     */
    constexpr TransformIterator(IteratorT iterator, FuncT function) noexcept(
        std::is_nothrow_move_constructible_v<IteratorT> &&
        std::is_nothrow_move_constructible_v<FuncT>)
        : iter_(std::move(iterator)), func_(std::move(function)) {}

    /*!
     * \brief Dereferences the iterator and applies the transformation function
     * \return The transformed value
     */
    constexpr auto operator*() const noexcept(noexcept(func_(*iter_)))
        -> reference {
        return func_(*iter_);
    }

    /*!
     * \brief Pre-increment operator
     * \return Reference to the incremented iterator
     */
    constexpr auto operator++() noexcept -> TransformIterator& {
        ++iter_;
        return *this;
    }

    /*!
     * \brief Post-increment operator
     * \return A copy of the iterator before incrementing
     */
    constexpr auto operator++(int) noexcept -> TransformIterator {
        auto tmp = *this;
        ++iter_;
        return tmp;
    }

    /*!
     * \brief Equality comparison operator
     * \param other The other iterator to compare with
     * \return True if the iterators are equal, false otherwise
     */
    constexpr auto operator==(const TransformIterator& other) const noexcept
        -> bool {
        return iter_ == other.iter_;
    }
};

/*!
 * \brief Creates a TransformIterator from an underlying iterator and a
 * transformation function
 * \tparam IteratorT The type of the underlying iterator
 * \tparam FuncT The type of the transformation function
 * \param iterator The underlying iterator
 * \param function The transformation function
 * \return A TransformIterator
 */
template <typename IteratorT, typename FuncT>
constexpr auto makeTransformIterator(IteratorT iterator, FuncT function)
    -> TransformIterator<IteratorT, FuncT> {
    return TransformIterator<IteratorT, FuncT>(std::move(iterator),
                                               std::move(function));
}

/*!
 * \brief An iterator that filters elements based on a predicate
 * \tparam IteratorT The type of the underlying iterator
 * \tparam PredicateT The type of the predicate
 */
template <typename IteratorT, typename PredicateT>
class FilterIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename std::iterator_traits<IteratorT>::value_type;
    using difference_type =
        typename std::iterator_traits<IteratorT>::difference_type;
    using pointer = typename std::iterator_traits<IteratorT>::pointer;
    using reference = typename std::iterator_traits<IteratorT>::reference;

private:
    IteratorT iter_;
    IteratorT end_;
    PredicateT pred_;

    constexpr void satisfyPredicate() {
        while (iter_ != end_ && !pred_(*iter_)) {
            ++iter_;
        }
    }

public:
    constexpr FilterIterator() = default;

    /*!
     * \brief Constructs a FilterIterator
     * \param iter The underlying iterator
     * \param end The end of the range
     * \param pred The predicate
     */
    constexpr FilterIterator(IteratorT iter, IteratorT end, PredicateT pred)
        : iter_(std::move(iter)), end_(std::move(end)), pred_(std::move(pred)) {
        satisfyPredicate();
    }

    constexpr auto operator*() const noexcept -> reference { return *iter_; }

    constexpr auto operator->() const noexcept -> pointer {
        return std::addressof(*iter_);
    }

    /*!
     * \brief Pre-increment operator
     * \return Reference to the incremented iterator
     */
    constexpr auto operator++() -> FilterIterator& {
        ++iter_;
        satisfyPredicate();
        return *this;
    }

    /*!
     * \brief Post-increment operator
     * \return A copy of the iterator before incrementing
     */
    constexpr auto operator++(int) -> FilterIterator {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    /*!
     * \brief Equality comparison operator
     * \param other The other iterator to compare with
     * \return True if the iterators are equal, false otherwise
     */
    constexpr auto operator==(const FilterIterator& other) const noexcept
        -> bool {
        return iter_ == other.iter_;
    }
};

/*!
 * \brief Creates a FilterIterator from an underlying iterator and a predicate
 * \tparam IteratorT The type of the underlying iterator
 * \tparam PredicateT The type of the predicate
 * \param iter The underlying iterator
 * \param end The end of the range
 * \param pred The predicate
 * \return A FilterIterator
 */
template <typename IteratorT, typename PredicateT>
constexpr auto makeFilterIterator(IteratorT iter, IteratorT end,
                                  PredicateT pred)
    -> FilterIterator<IteratorT, PredicateT> {
    return FilterIterator<IteratorT, PredicateT>(
        std::move(iter), std::move(end), std::move(pred));
}

/*!
 * \brief An iterator that reverses the direction of another iterator
 * \tparam IteratorT The type of the underlying iterator
 */
template <typename IteratorT>
class ReverseIterator {
public:
    using iterator_category =
        typename std::iterator_traits<IteratorT>::iterator_category;
    using value_type = typename std::iterator_traits<IteratorT>::value_type;
    using difference_type =
        typename std::iterator_traits<IteratorT>::difference_type;
    using pointer = typename std::iterator_traits<IteratorT>::pointer;
    using reference = typename std::iterator_traits<IteratorT>::reference;

private:
    IteratorT current_;

public:
    constexpr ReverseIterator() = default;

    /*!
     * \brief Constructs a ReverseIterator from an underlying iterator
     * \param iterator The underlying iterator
     */
    constexpr explicit ReverseIterator(IteratorT iterator) noexcept(
        std::is_nothrow_move_constructible_v<IteratorT>)
        : current_(std::move(iterator)) {}

    /*!
     * \brief Gets the base iterator
     * \return The underlying iterator
     */
    constexpr auto base() const noexcept -> IteratorT { return current_; }

    constexpr auto operator*() const noexcept -> reference {
        auto tmp = current_;
        return *--tmp;
    }

    constexpr auto operator->() const noexcept -> pointer {
        return std::addressof(operator*());
    }

    constexpr auto operator++() noexcept -> ReverseIterator& {
        --current_;
        return *this;
    }

    constexpr auto operator++(int) noexcept -> ReverseIterator {
        auto tmp = *this;
        --current_;
        return tmp;
    }

    constexpr auto operator--() noexcept -> ReverseIterator& {
        ++current_;
        return *this;
    }

    constexpr auto operator--(int) noexcept -> ReverseIterator {
        auto tmp = *this;
        ++current_;
        return tmp;
    }

    constexpr auto operator==(const ReverseIterator& iter) const noexcept
        -> bool {
        return current_ == iter.current_;
    }
};

/*!
 * \brief An iterator that zips multiple iterators together
 * \tparam Iterators The types of the underlying iterators
 */
template <typename... Iterators>
class ZipIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type =
        std::tuple<typename std::iterator_traits<Iterators>::value_type...>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type;

private:
    std::tuple<Iterators...> iterators_;

    template <std::size_t... Indices>
    constexpr auto dereference(std::index_sequence<Indices...>) const noexcept
        -> value_type {
        return std::make_tuple(*std::get<Indices>(iterators_)...);
    }

    template <std::size_t... Indices>
    constexpr void increment(std::index_sequence<Indices...>) noexcept {
        (++std::get<Indices>(iterators_), ...);
    }

public:
    constexpr ZipIterator() = default;

    /*!
     * \brief Constructs a ZipIterator from multiple iterators
     * \param its The underlying iterators
     */
    constexpr explicit ZipIterator(Iterators... its) noexcept(
        (std::is_nothrow_move_constructible_v<Iterators> && ...))
        : iterators_(std::move(its)...) {}

    /*!
     * \brief Dereferences all iterators and returns a tuple of their values
     * \return A tuple containing the values from all iterators
     */
    constexpr auto operator*() const noexcept -> value_type {
        return dereference(std::index_sequence_for<Iterators...>{});
    }

    /*!
     * \brief Pre-increment operator
     * \return Reference to the incremented iterator
     */
    constexpr auto operator++() noexcept -> ZipIterator& {
        increment(std::index_sequence_for<Iterators...>{});
        return *this;
    }

    /*!
     * \brief Post-increment operator
     * \return A copy of the iterator before incrementing
     */
    constexpr auto operator++(int) noexcept -> ZipIterator {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    /*!
     * \brief Equality comparison operator
     * \param other The other iterator to compare with
     * \return True if the iterators are equal, false otherwise
     */
    constexpr auto operator==(const ZipIterator& other) const noexcept -> bool {
        return iterators_ == other.iterators_;
    }
};

/*!
 * \brief Creates a ZipIterator from multiple iterators
 * \tparam Iterators The types of the underlying iterators
 * \param its The underlying iterators
 * \return A ZipIterator
 */
template <typename... Iterators>
constexpr auto makeZipIterator(Iterators... its) noexcept
    -> ZipIterator<Iterators...> {
    return ZipIterator<Iterators...>(std::move(its)...);
}

#endif  // ATOM_EXPERIMENTAL_ITERATOR_HPP
