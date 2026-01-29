/*
 * stacktrace_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of stacktrace utility functions

**************************************************/

#include "stacktrace_utils.hpp"

#include <array>
#include <cstdlib>
#include <format>
#include <regex>
#include <string_view>

#ifndef _WIN32
#include <cxxabi.h>
#endif

namespace atom::error {
namespace stacktrace_utils {

std::string demangle(const std::string& mangled) {
    if (mangled.empty()) [[unlikely]] {
        return mangled;
    }

#if defined(__GNUC__) || defined(__clang__)
#ifndef _WIN32
    // Use abi::__cxa_demangle for GCC/Clang on non-Windows
    int status = 0;
    char* demangled =
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
#endif
#endif

    return mangled;
}

std::string prettify(const std::string& input) {
    std::string output = input;

    // Pre-compiled static regex patterns for better performance
    static const std::array REPLACEMENTS = {
        std::pair{std::regex("std::__1::"), "std::"},
        std::pair{std::regex("std::__cxx11::"), "std::"},
        std::pair{std::regex("__thiscall "), ""},
        std::pair{std::regex("__cdecl "), ""},
        std::pair{std::regex(", std::allocator<[^<>]+>"), ""},
        std::pair{std::regex("class "), ""},
        std::pair{std::regex("struct "), ""}};

    for (const auto& [pattern, replacement] : REPLACEMENTS) {
        try {
            output = std::regex_replace(output, pattern, replacement);
        } catch (const std::regex_error&) {
            // Skip problematic regex patterns
            continue;
        }
    }

    try {
        static const std::regex spaceInAngles(R"(<\s*([^<> ]+)\s*>)");
        static const std::regex nestedAngles(
            R"(<([^<>]*)<([^<>]*)>\s*([^<>]*)>)");
        output = std::regex_replace(output, spaceInAngles, "<$1>");
        output = std::regex_replace(output, nestedAngles, "<$1<$2>$3>");
    } catch (const std::regex_error&) {
        // Return partially processed output if regex fails
    }

    return output;
}

std::string formatAddress(uintptr_t address) {
    return std::format("0x{:X}", address);
}

std::string getBaseName(const std::string& path) {
    if (path.empty()) [[unlikely]] {
        return path;
    }

    const std::string_view pathView = path;
    if (const auto lastSlash = pathView.find_last_of("/\\");
        lastSlash != std::string_view::npos) {
        return std::string(pathView.substr(lastSlash + 1));
    }
    return path;
}

bool containsMangledNames(const std::string& str) {
    const std::string_view view = str;
    // Look for common C++ mangling patterns
    return view.find("_Z") != std::string_view::npos ||
           view.find("__Z") != std::string_view::npos ||
           view.starts_with('?');  // MSVC mangling
}

}  // namespace stacktrace_utils
}  // namespace atom::error
