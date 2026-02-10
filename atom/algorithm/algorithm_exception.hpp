/*
 * algorithm_exception.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: Unified exception hierarchy for algorithm module

**************************************************/

#ifndef ATOM_ALGORITHM_ALGORITHM_EXCEPTION_HPP
#define ATOM_ALGORITHM_ALGORITHM_EXCEPTION_HPP

#include "atom/error/exception.hpp"

namespace atom::algorithm {

/**
 * @brief Base exception class for all algorithm-related errors
 */
class AlgorithmException : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception class for math-related errors
 */
class MathException : public AlgorithmException {
public:
    using AlgorithmException::AlgorithmException;
};

/**
 * @brief Exception class for optimization-related errors
 */
class OptimizationException : public AlgorithmException {
public:
    using AlgorithmException::AlgorithmException;
};

/**
 * @brief Exception class for signal processing errors
 */
class SignalException : public AlgorithmException {
public:
    using AlgorithmException::AlgorithmException;
};

/**
 * @brief Exception class for utility function errors
 */
class UtilsException : public AlgorithmException {
public:
    using AlgorithmException::AlgorithmException;
};

// Convenience macros for throwing algorithm exceptions
#define THROW_ALGORITHM_ERROR(...)                                            \
    throw atom::algorithm::AlgorithmException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_MATH_ERROR(...)                                            \
    throw atom::algorithm::MathException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_OPTIMIZATION_ERROR(...)             \
    throw atom::algorithm::OptimizationException( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SIGNAL_ERROR(...)                                            \
    throw atom::algorithm::SignalException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                           ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_UTILS_ERROR(...)                                            \
    throw atom::algorithm::UtilsException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ALGORITHM_EXCEPTION_HPP
