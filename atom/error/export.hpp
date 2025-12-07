/*
 * export.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-06

Description: DLL export/import macros for atom-error module

**************************************************/

#ifndef ATOM_ERROR_EXPORT_HPP
#define ATOM_ERROR_EXPORT_HPP

#ifdef _WIN32
#ifdef atom_error_EXPORTS
#define ATOM_ERROR_API __declspec(dllexport)
#else
#define ATOM_ERROR_API __declspec(dllimport)
#endif
#else
#define ATOM_ERROR_API __attribute__((visibility("default")))
#endif

#endif  // ATOM_ERROR_EXPORT_HPP
