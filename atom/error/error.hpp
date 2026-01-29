/*
 * error.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Main header file for the error handling module

**************************************************/

#ifndef ATOM_ERROR_HPP
#define ATOM_ERROR_HPP

// Core error types and codes
#include "core/error_codes.hpp"
#include "core/error_metadata.hpp"
#include "core/error_types.hpp"

// Exception classes
#include "exception/argument_exceptions.hpp"
#include "exception/common_exceptions.hpp"
#include "exception/exception_base.hpp"
#include "exception/file_exceptions.hpp"
#include "exception/object_exceptions.hpp"
#include "exception/system_exceptions.hpp"

// Stack trace
#include "stacktrace/stacktrace.hpp"

// Error context
#include "context/context_manager.hpp"
#include "context/error_context.hpp"
#include "context/scoped_context.hpp"

// Error handler
#include "handler/error_aggregator.hpp"
#include "handler/error_reporter.hpp"
#include "handler/global_handler.hpp"

#endif  // ATOM_ERROR_HPP
