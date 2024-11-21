/*
 * noncopyable.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: A simple implementation of noncopyable.

**************************************************/

#ifndef ATOM_TYPE_NONCOPYABLE_HPP
#define ATOM_TYPE_NONCOPYABLE_HPP

#ifdef ATOM_USE_BOOST
#include <boost/core/noncopyable.hpp>
#endif

/**
 * @brief A class that prevents copying and moving.
 *
 * This class provides a simple implementation of a non-copyable and non-movable
 * class. It can be used as a base class to prevent derived classes from being
 * copied or moved.
 */
class NonCopyable
#ifdef ATOM_USE_BOOST
    : private boost::noncopyable
#endif
{
public:
    NonCopyable() = default;
    virtual ~NonCopyable() = default;

#ifndef ATOM_USE_BOOST
    NonCopyable(const NonCopyable&) = delete;
    auto operator=(const NonCopyable&) -> NonCopyable& = delete;

    NonCopyable(NonCopyable&&) = delete;
    auto operator=(NonCopyable&&) -> NonCopyable& = delete;
#endif
};

#endif  // ATOM_TYPE_NONCOPYABLE_HPP