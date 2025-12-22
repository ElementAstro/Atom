/**
 * @file ocr.cpp
 * @brief Python bindings for OCR (Optical Character Recognition)
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#ifdef ATOM_IMAGE_HAS_OCR
#include "atom/image/processing/ocr/ocr.hpp"
#endif

namespace py = pybind11;

void bind_ocr(py::module& m) {
#ifdef ATOM_IMAGE_HAS_OCR
    using namespace atom::image;

    auto ocr_module = m.def_submodule(
        "ocr", "Optical Character Recognition (OCR) capabilities");

    // OCR language enum
    py::enum_<OCRLanguage>(ocr_module, "Language", "Supported OCR languages")
        .value("ENGLISH", OCRLanguage::ENGLISH, "English")
        .value("CHINESE_SIMPLIFIED", OCRLanguage::CHINESE_SIMPLIFIED,
               "Simplified Chinese")
        .value("CHINESE_TRADITIONAL", OCRLanguage::CHINESE_TRADITIONAL,
               "Traditional Chinese")
        .value("JAPANESE", OCRLanguage::JAPANESE, "Japanese")
        .value("KOREAN", OCRLanguage::KOREAN, "Korean")
        .value("FRENCH", OCRLanguage::FRENCH, "French")
        .value("GERMAN", OCRLanguage::GERMAN, "German")
        .value("SPANISH", OCRLanguage::SPANISH, "Spanish")
        .value("ITALIAN", OCRLanguage::ITALIAN, "Italian")
        .value("PORTUGUESE", OCRLanguage::PORTUGUESE, "Portuguese")
        .value("RUSSIAN", OCRLanguage::RUSSIAN, "Russian")
        .value("ARABIC", OCRLanguage::ARABIC, "Arabic")
        .value("AUTO", OCRLanguage::AUTO, "Auto-detect language")
        .export_values();

    // OCR mode enum
    py::enum_<OCRMode>(ocr_module, "Mode", "OCR processing modes")
        .value("FAST", OCRMode::FAST, "Fast mode (less accurate)")
        .value("ACCURATE", OCRMode::ACCURATE, "Accurate mode (slower)")
        .value("LEGACY", OCRMode::LEGACY, "Legacy OCR engine")
        .value("LSTM", OCRMode::LSTM, "LSTM neural network engine")
        .export_values();

    // OCR options
    py::class_<OCROptions>(ocr_module, "Options", "Options for OCR processing")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("language", &OCROptions::language, "OCR language")
        .def_readwrite("mode", &OCROptions::mode, "OCR processing mode")
        .def_readwrite("page_segmentation_mode",
                       &OCROptions::pageSegmentationMode,
                       "Page segmentation mode")
        .def_readwrite("whitelist", &OCROptions::whitelist,
                       "Character whitelist")
        .def_readwrite("blacklist", &OCROptions::blacklist,
                       "Character blacklist")
        .def_readwrite("min_confidence", &OCROptions::minConfidence,
                       "Minimum confidence threshold")
        .def_readwrite("preprocess", &OCROptions::preprocess,
                       "Enable preprocessing")
        .def_readwrite("deskew", &OCROptions::deskew, "Enable deskewing")
        .def_readwrite("denoise", &OCROptions::denoise, "Enable denoising")
        .def_readwrite("scale_factor", &OCROptions::scaleFactor,
                       "Image scaling factor");

    // OCR result for a single word
    py::class_<OCRWord>(ocr_module, "Word", "OCR result for a single word")
        .def(py::init<>())
        .def_readwrite("text", &OCRWord::text, "Recognized text")
        .def_readwrite("confidence", &OCRWord::confidence, "Confidence score")
        .def_readwrite("bbox", &OCRWord::bbox,
                       "Bounding box (x, y, width, height)")
        .def("__str__", [](const OCRWord& self) { return self.text; })
        .def("__repr__", [](const OCRWord& self) {
            return "<OCRWord text='" + self.text +
                   "' confidence=" + std::to_string(self.confidence) + ">";
        });

    // OCR result for a line
    py::class_<OCRLine>(ocr_module, "Line", "OCR result for a line of text")
        .def(py::init<>())
        .def_readwrite("text", &OCRLine::text, "Full line text")
        .def_readwrite("words", &OCRLine::words, "Individual words")
        .def_readwrite("confidence", &OCRLine::confidence, "Average confidence")
        .def_readwrite("bbox", &OCRLine::bbox, "Bounding box")
        .def("__str__", [](const OCRLine& self) { return self.text; })
        .def("__repr__", [](const OCRLine& self) {
            return "<OCRLine text='" + self.text +
                   "' words=" + std::to_string(self.words.size()) + ">";
        });

    // OCR result for a paragraph
    py::class_<OCRParagraph>(ocr_module, "Paragraph",
                             "OCR result for a paragraph")
        .def(py::init<>())
        .def_readwrite("text", &OCRParagraph::text, "Full paragraph text")
        .def_readwrite("lines", &OCRParagraph::lines, "Individual lines")
        .def_readwrite("confidence", &OCRParagraph::confidence,
                       "Average confidence")
        .def_readwrite("bbox", &OCRParagraph::bbox, "Bounding box")
        .def("__str__", [](const OCRParagraph& self) { return self.text; });

    // Complete OCR result
    py::class_<OCRResult>(ocr_module, "Result", "Complete OCR result")
        .def(py::init<>())
        .def_readwrite("text", &OCRResult::text, "Full recognized text")
        .def_readwrite("paragraphs", &OCRResult::paragraphs, "Paragraphs")
        .def_readwrite("lines", &OCRResult::lines, "All lines")
        .def_readwrite("words", &OCRResult::words, "All words")
        .def_readwrite("confidence", &OCRResult::confidence,
                       "Overall confidence")
        .def_readwrite("language", &OCRResult::language, "Detected language")
        .def_readwrite("processing_time", &OCRResult::processingTime,
                       "Processing time in milliseconds")
        .def("get_text", &OCRResult::getText, "Get full text")
        .def("get_text_by_confidence", &OCRResult::getTextByConfidence,
             py::arg("min_confidence"), "Get text filtered by confidence")
        .def("get_words_in_region", &OCRResult::getWordsInRegion, py::arg("x"),
             py::arg("y"), py::arg("width"), py::arg("height"),
             "Get words in specified region")
        .def("to_json", &OCRResult::toJson, "Convert result to JSON")
        .def("to_hocr", &OCRResult::toHOCR, "Convert result to hOCR format")
        .def("__str__", [](const OCRResult& self) { return self.text; })
        .def("__repr__", [](const OCRResult& self) {
            return "<OCRResult words=" + std::to_string(self.words.size()) +
                   " confidence=" + std::to_string(self.confidence) + ">";
        });

    // OCR engine
    py::class_<OCREngine>(ocr_module, "Engine",
                          R"pbdoc(
        OCR engine for text recognition.

        The OCR engine provides text recognition capabilities using Tesseract.
        Supports multiple languages, preprocessing, and various output formats.

        Example:
            >>> engine = ocr.Engine()
            >>> engine.initialize()
            >>> result = engine.recognize(image)
            >>> print(result.text)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("initialize", &OCREngine::initialize, py::arg("data_path") = "",
             py::arg("language") = OCRLanguage::ENGLISH,
             R"pbdoc(
            Initialize OCR engine.

            Args:
                data_path: Path to tessdata directory
                language: Default language

            Returns:
                True if initialization succeeded
            )pbdoc")
        .def("is_initialized", &OCREngine::isInitialized,
             "Check if engine is initialized")
        .def("recognize", &OCREngine::recognize, py::arg("input"),
             py::arg("options") = OCROptions{},
             R"pbdoc(
            Recognize text in image.

            Args:
                input: Input image
                options: OCR options

            Returns:
                OCRResult with recognized text
            )pbdoc")
        .def("recognize_region", &OCREngine::recognizeRegion, py::arg("input"),
             py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
             py::arg("options") = OCROptions{},
             "Recognize text in specific region")
        .def("detect_text_regions", &OCREngine::detectTextRegions,
             py::arg("input"), "Detect regions containing text")
        .def("get_available_languages", &OCREngine::getAvailableLanguages,
             "Get list of available languages")
        .def("set_variable", &OCREngine::setVariable, py::arg("name"),
             py::arg("value"), "Set Tesseract configuration variable")
        .def("get_version", &OCREngine::getVersion, "Get Tesseract version")
        .def("shutdown", &OCREngine::shutdown, "Shutdown OCR engine");

    // Convenience functions
    ocr_module.def("recognize_text", &recognizeText, py::arg("input"),
                   py::arg("language") = OCRLanguage::ENGLISH,
                   py::arg("mode") = OCRMode::ACCURATE,
                   R"pbdoc(
        Recognize text in image (convenience function).

        Args:
            input: Input image
            language: OCR language
            mode: OCR mode

        Returns:
            Recognized text string
        )pbdoc");

    ocr_module.def("extract_text", &extractText, py::arg("input"),
                   py::arg("preprocess") = true,
                   "Extract text from image with automatic preprocessing");

    ocr_module.def("is_ocr_available", &isOCRAvailable,
                   "Check if OCR support is available");

#else
    // OCR not available - provide stub
    auto ocr_module = m.def_submodule(
        "ocr", "OCR support not available (Tesseract not found)");

    ocr_module.def(
        "is_ocr_available", []() { return false; },
        "Check if OCR support is available");
#endif
}
