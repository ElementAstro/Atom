/**
 * @file enhancement.cpp
 * @brief Python bindings for image enhancement operations
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/enhancement.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_enhancement(py::module& m) {
    auto enhance_module = m.def_submodule(
        "enhancement", "Image enhancement and quality improvement operations");

    // Color adjustment
    enhance_module.def("adjust_brightness", &adjustBrightness, py::arg("input"),
                       py::arg("value"),
                       R"pbdoc(
        Adjust image brightness.

        Args:
            input: Input image
            value: Brightness adjustment (-100 to 100)

        Returns:
            Adjusted image
        )pbdoc");

    enhance_module.def("adjust_contrast", &adjustContrast, py::arg("input"),
                       py::arg("value"), "Adjust image contrast (-100 to 100)");

    enhance_module.def("adjust_saturation", &adjustSaturation, py::arg("input"),
                       py::arg("value"),
                       "Adjust color saturation (-100 to 100)");

    enhance_module.def("adjust_hue", &adjustHue, py::arg("input"),
                       py::arg("value"),
                       "Adjust color hue (-180 to 180 degrees)");

    enhance_module.def("adjust_gamma", &adjustGamma, py::arg("input"),
                       py::arg("gamma"),
                       R"pbdoc(
        Apply gamma correction.

        Args:
            input: Input image
            gamma: Gamma value (typically 0.1 to 3.0)

        Returns:
            Gamma-corrected image
        )pbdoc");

    enhance_module.def("adjust_exposure", &adjustExposure, py::arg("input"),
                       py::arg("stops"), "Adjust exposure in stops");

    enhance_module.def("adjust_temperature", &adjustTemperature,
                       py::arg("input"), py::arg("temperature"),
                       "Adjust color temperature (2000-10000K)");

    enhance_module.def("adjust_tint", &adjustTint, py::arg("input"),
                       py::arg("tint"), "Adjust green-magenta tint");

    // Histogram operations
    enhance_module.def("equalize_histogram", &equalizeHistogram,
                       py::arg("input"), py::arg("adaptive") = false,
                       R"pbdoc(
        Equalize image histogram.

        Args:
            input: Input image
            adaptive: Use adaptive histogram equalization (CLAHE)

        Returns:
            Equalized image
        )pbdoc");

    enhance_module.def("match_histogram", &matchHistogram, py::arg("input"),
                       py::arg("reference"),
                       "Match histogram to reference image");

    enhance_module.def("stretch_histogram", &stretchHistogram, py::arg("input"),
                       py::arg("min_percentile") = 1.0,
                       py::arg("max_percentile") = 99.0,
                       "Stretch histogram to use full dynamic range");

    // Sharpening
    enhance_module.def("sharpen", &sharpen, py::arg("input"),
                       py::arg("amount") = 1.0, py::arg("radius") = 1.0,
                       py::arg("threshold") = 0.0,
                       R"pbdoc(
        Sharpen image.

        Args:
            input: Input image
            amount: Sharpening amount (0.0 to 2.0)
            radius: Sharpening radius
            threshold: Sharpening threshold

        Returns:
            Sharpened image
        )pbdoc");

    enhance_module.def("unsharp_mask", &unsharpMask, py::arg("input"),
                       py::arg("amount") = 1.0, py::arg("radius") = 1.0,
                       py::arg("threshold") = 0.0,
                       "Apply unsharp mask sharpening");

    enhance_module.def("high_pass_sharpen", &highPassSharpen, py::arg("input"),
                       py::arg("radius") = 3.0,
                       "Sharpen using high-pass filter");

    // Denoising
    enhance_module.def("denoise", &denoise, py::arg("input"),
                       py::arg("strength") = 10.0,
                       py::arg("method") = "nlmeans",
                       R"pbdoc(
        Remove noise from image.

        Args:
            input: Input image
            strength: Denoising strength
            method: Denoising method (nlmeans, bilateral, median)

        Returns:
            Denoised image
        )pbdoc");

    enhance_module.def("denoise_color", &denoiseColor, py::arg("input"),
                       py::arg("strength") = 10.0,
                       py::arg("color_strength") = 10.0, "Denoise color image");

    enhance_module.def("reduce_noise", &reduceNoise, py::arg("input"),
                       py::arg("strength") = 10.0, "General noise reduction");

    // White balance
    enhance_module.def("auto_white_balance", &autoWhiteBalance,
                       py::arg("input"), py::arg("method") = "gray_world",
                       R"pbdoc(
        Automatically adjust white balance.

        Args:
            input: Input image
            method: White balance method (gray_world, retinex, simple)

        Returns:
            White-balanced image
        )pbdoc");

    enhance_module.def("white_balance_gray_world", &whiteBalanceGrayWorld,
                       py::arg("input"),
                       "White balance using gray world assumption");

    enhance_module.def("white_balance_retinex", &whiteBalanceRetinex,
                       py::arg("input"),
                       "White balance using retinex algorithm");

    // Tone mapping
    enhance_module.def("tone_map", &toneMap, py::arg("input"),
                       py::arg("method") = "reinhard", py::arg("gamma") = 2.2,
                       R"pbdoc(
        Apply tone mapping for HDR images.

        Args:
            input: Input HDR image
            method: Tone mapping method (reinhard, drago, mantiuk)
            gamma: Gamma correction value

        Returns:
            Tone-mapped LDR image
        )pbdoc");

    enhance_module.def("local_tone_map", &localToneMap, py::arg("input"),
                       py::arg("radius") = 50, "Apply local tone mapping");

    // Shadow and highlight recovery
    enhance_module.def("recover_shadows", &recoverShadows, py::arg("input"),
                       py::arg("amount") = 50.0, "Recover details in shadows");

    enhance_module.def("recover_highlights", &recoverHighlights,
                       py::arg("input"), py::arg("amount") = 50.0,
                       "Recover details in highlights");

    enhance_module.def("adjust_shadows_highlights", &adjustShadowsHighlights,
                       py::arg("input"), py::arg("shadows") = 0.0,
                       py::arg("highlights") = 0.0,
                       "Adjust shadows and highlights independently");

    // Clarity and vibrance
    enhance_module.def("adjust_clarity", &adjustClarity, py::arg("input"),
                       py::arg("amount") = 50.0,
                       "Adjust image clarity (local contrast)");

    enhance_module.def("adjust_vibrance", &adjustVibrance, py::arg("input"),
                       py::arg("amount") = 50.0,
                       "Adjust vibrance (smart saturation)");

    enhance_module.def("adjust_structure", &adjustStructure, py::arg("input"),
                       py::arg("amount") = 50.0,
                       "Adjust image structure/texture");

    // Vignette
    enhance_module.def("add_vignette", &addVignette, py::arg("input"),
                       py::arg("amount") = 50.0, py::arg("radius") = 1.0,
                       "Add vignette effect");

    enhance_module.def("remove_vignette", &removeVignette, py::arg("input"),
                       "Remove vignetting");

    // Color grading
    enhance_module.def("apply_lut", &applyLUT, py::arg("input"),
                       py::arg("lut_file"), "Apply color lookup table (LUT)");

    enhance_module.def("apply_color_grade", &applyColorGrade, py::arg("input"),
                       py::arg("preset"), "Apply color grading preset");

    enhance_module.def("split_tone", &splitTone, py::arg("input"),
                       py::arg("shadow_hue"), py::arg("shadow_sat"),
                       py::arg("highlight_hue"), py::arg("highlight_sat"),
                       "Apply split toning");

    // Auto enhancement
    enhance_module.def("auto_enhance", &autoEnhance, py::arg("input"),
                       py::arg("aggressive") = false,
                       R"pbdoc(
        Automatically enhance image quality.

        Applies multiple enhancement operations automatically based on
        image analysis.

        Args:
            input: Input image
            aggressive: Use aggressive enhancement

        Returns:
            Enhanced image
        )pbdoc");

    enhance_module.def("auto_contrast", &autoContrast, py::arg("input"),
                       py::arg("clip_percent") = 1.0,
                       "Automatically adjust contrast");

    enhance_module.def("auto_levels", &autoLevels, py::arg("input"),
                       "Automatically adjust levels");

    enhance_module.def("auto_color", &autoColor, py::arg("input"),
                       "Automatically adjust colors");

    // Quality assessment
    enhance_module.def("assess_quality", &assessQuality, py::arg("input"),
                       R"pbdoc(
        Assess image quality.

        Returns dictionary with quality metrics including:
        - sharpness
        - noise_level
        - contrast
        - brightness
        - color_balance
        - overall_score
        )pbdoc");

    enhance_module.def("compute_sharpness", &computeSharpness, py::arg("input"),
                       "Compute image sharpness metric");

    enhance_module.def("compute_noise_level", &computeNoiseLevel,
                       py::arg("input"), "Estimate noise level in image");

    enhance_module.def("is_overexposed", &isOverexposed, py::arg("input"),
                       py::arg("threshold") = 0.05,
                       "Check if image is overexposed");

    enhance_module.def("is_underexposed", &isUnderexposed, py::arg("input"),
                       py::arg("threshold") = 0.05,
                       "Check if image is underexposed");
}
