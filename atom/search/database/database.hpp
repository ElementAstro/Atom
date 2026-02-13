/*
 * database.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file database.hpp
 * @brief Aggregate header for database components.
 */

#ifndef ATOM_SEARCH_DATABASE_HPP
#define ATOM_SEARCH_DATABASE_HPP

#include "base.hpp"
#include "connection_pool.hpp"
#include "query_builder.hpp"
#include "retry.hpp"
#include "statement_cache.hpp"
#include "types.hpp"

// Database implementations
#include "mysql.hpp"
#include "sqlite.hpp"

#endif  // ATOM_SEARCH_DATABASE_HPP
