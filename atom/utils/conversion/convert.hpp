/*
 * convert.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_UTILS_CONVERT_HPP
#define ATOM_UTILS_CONVERT_HPP

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <string_view>

namespace atom::utils {
/**
 * @brief Converts a string_view to LPWSTR (wide character string).
 * @param str The string_view to be converted.
 * @return LPWSTR representing the wide character version of the input string.
 */
[[nodiscard]] auto CharToLPWSTR(std::string_view str) -> LPWSTR;

/**
 * @brief Converts a WCHAR array to a std::string using UTF-8 encoding.
 * @param wCharArray A pointer to the null-terminated wide character array.
 * @return std::string The converted std::string in UTF-8 encoding.
 */
[[nodiscard]] auto WCharArrayToString(const WCHAR* wCharArray) -> std::string;

/**
 * @brief Converts a string to LPSTR (character string).
 * @param str The string to be converted.
 * @return LPSTR representing the character version of the input string.
 */
[[nodiscard]] auto StringToLPSTR(const std::string& str) -> LPSTR;

/**
 * @brief Converts a wstring to LPSTR (character string).
 * @param wstr The wstring to be converted.
 * @return LPSTR representing the character version of the input wstring.
 */
[[nodiscard]] auto WStringToLPSTR(const std::wstring& wstr) -> LPSTR;

/**
 * @brief Converts a string to LPWSTR (wide character string).
 * @param str The string to be converted.
 * @return LPWSTR representing the wide character version of the input string.
 */
[[nodiscard]] auto StringToLPWSTR(const std::string& str) -> LPWSTR;

/**
 * @brief Converts LPWSTR (wide character string) to a string.
 * @param lpwstr The LPWSTR to be converted.
 * @return std::string containing the converted data.
 */
[[nodiscard]] auto LPWSTRToString(LPWSTR lpwstr) -> std::string;

/**
 * @brief Converts LPCWSTR (const wide character string) to a string.
 * @param lpcwstr The LPCWSTR to be converted.
 * @return std::string containing the converted data.
 */
[[nodiscard]] auto LPCWSTRToString(LPCWSTR lpcwstr) -> std::string;

/**
 * @brief Converts a wstring to LPWSTR (wide character string).
 * @param wstr The wstring to be converted.
 * @return LPWSTR representing the converted wstring.
 */
[[nodiscard]] auto WStringToLPWSTR(const std::wstring& wstr) -> LPWSTR;

/**
 * @brief Converts LPWSTR (wide character string) to a wstring.
 * @param lpwstr The LPWSTR to be converted.
 * @return std::wstring containing the converted data.
 */
[[nodiscard]] auto LPWSTRToWString(LPWSTR lpwstr) -> std::wstring;

/**
 * @brief Converts LPCWSTR (const wide character string) to a wstring.
 * @param lpcwstr The LPCWSTR to be converted.
 * @return std::wstring containing the converted data.
 */
[[nodiscard]] auto LPCWSTRToWString(LPCWSTR lpcwstr) -> std::wstring;

}  // namespace atom::utils

#endif
#endif