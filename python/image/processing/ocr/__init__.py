"""
Optical Character Recognition (OCR) capabilities.

This submodule provides:
- OCREngine (Engine): OCR engine for text recognition
- OCRLanguage (Language): Supported OCR languages
- OCRMode (Mode): OCR processing modes
- OCROptions (Options): OCR processing options
- OCRResult (Result), OCRWord (Word), OCRLine (Line), OCRParagraph (Paragraph): Result structures
- Convenience functions: recognize_text, extract_text, is_ocr_available

These classes are exported in the atom_image.ocr submodule by the C++ bindings.
OCR support requires Tesseract to be available at build time.
"""

__all__ = [
    "OCREngine",
    "OCRLanguage",
    "OCRMode",
    "OCROptions",
    "OCRResult",
    "OCRWord",
    "OCRLine",
    "OCRParagraph",
    "recognize_text",
    "extract_text",
    "is_ocr_available",
]

try:
    from atom_image.ocr import Engine as OCREngine
    from atom_image.ocr import Language as OCRLanguage
    from atom_image.ocr import Line as OCRLine
    from atom_image.ocr import Mode as OCRMode
    from atom_image.ocr import Options as OCROptions
    from atom_image.ocr import Paragraph as OCRParagraph
    from atom_image.ocr import Result as OCRResult
    from atom_image.ocr import Word as OCRWord
    from atom_image.ocr import extract_text, is_ocr_available, recognize_text
except ImportError:
    # OCR not available - provide stub
    def is_ocr_available():
        """Check if OCR support is available."""
        return False

    __all__ = ["is_ocr_available"]
