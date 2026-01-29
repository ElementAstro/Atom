/**
 * @file ocr.hpp
 * @brief Enhanced Optical Character Recognition (OCR) processing with Tesseract
 * and OpenCV
 *
 * This is the main header file that includes all OCR module components.
 * For individual components, include the specific headers:
 * - ocr_types.hpp    - Feature detection macros
 * - ocr_config.hpp   - OCRConfig struct
 * - progress_reporter.hpp - ProgressReporter class
 * - ocr_cache.hpp    - OCRCache class
 * - spell_checker.hpp - SpellChecker class
 * - ocr_processor.hpp - OCRProcessor class
 * - model_manager.hpp - ModelManager class
 */

#pragma once

// Include all OCR module components
#include "model_manager.hpp"
#include "ocr_cache.hpp"
#include "ocr_config.hpp"
#include "ocr_processor.hpp"
#include "ocr_types.hpp"
#include "progress_reporter.hpp"
#include "spell_checker.hpp"

// Backward compatibility aliases
namespace atom::image {

// Re-export types from ocr namespace for backward compatibility
using OCRConfig = atom::image::ocr::OCRConfig;
using ProgressReporter = atom::image::ocr::ProgressReporter;
using OCRCache = atom::image::ocr::OCRCache;
using SpellChecker = atom::image::ocr::SpellChecker;
using EnhancedOCRProcessor = atom::image::ocr::OCRProcessor;
using OCRResult = atom::image::ocr::OCRResult;

}  // namespace atom::image
