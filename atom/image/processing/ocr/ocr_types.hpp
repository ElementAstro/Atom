/**
 * @file ocr_types.hpp
 * @brief Common type definitions and feature detection for OCR module
 */

#pragma once

// Feature detection macros
#ifndef ATOM_OCR_HAS_SUPERRES
#if __has_include(<opencv2/dnn_superres.hpp>)
#define ATOM_OCR_HAS_SUPERRES 1
#else
#define ATOM_OCR_HAS_SUPERRES 0
#endif
#endif

#ifndef ATOM_OCR_HAS_TEXT
#if __has_include(<opencv2/text.hpp>)
#define ATOM_OCR_HAS_TEXT 1
#else
#define ATOM_OCR_HAS_TEXT 0
#endif
#endif

#ifndef ATOM_OCR_HAS_JSON
#if __has_include(<nlohmann/json.hpp>)
#define ATOM_OCR_HAS_JSON 1
#else
#define ATOM_OCR_HAS_JSON 0
#endif
#endif

// Check for C++20 format support
#ifndef ATOM_OCR_HAS_FORMAT
#if __cpp_lib_format >= 201907L
#define ATOM_OCR_HAS_FORMAT 1
#else
#define ATOM_OCR_HAS_FORMAT 0
#endif
#endif
