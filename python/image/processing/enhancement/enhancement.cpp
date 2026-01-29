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

    // ColorSpace enum
    py::enum_<ColorSpace>(enhance_module, "ColorSpace", "Color space types")
        .value("RGB", ColorSpace::RGB, "RGB color space")
        .value("HSV", ColorSpace::HSV, "HSV color space")
        .value("HSL", ColorSpace::HSL, "HSL color space")
        .value("LAB", ColorSpace::LAB, "LAB color space")
        .value("YUV", ColorSpace::YUV, "YUV color space")
        .value("XYZ", ColorSpace::XYZ, "XYZ color space")
        .value("GRAY", ColorSpace::GRAY, "Grayscale")
        .value("YCbCr", ColorSpace::YCbCr, "YCbCr color space")
        .value("LUV", ColorSpace::LUV, "LUV color space")
        .value("HLS", ColorSpace::HLS, "HLS color space")
        .export_values();

    // HistogramMethod enum
    py::enum_<HistogramMethod>(enhance_module, "HistogramMethod",
                               "Histogram equalization methods")
        .value("GLOBAL", HistogramMethod::GLOBAL,
               "Global histogram equalization")
        .value("ADAPTIVE", HistogramMethod::ADAPTIVE,
               "Adaptive histogram equalization")
        .value("CLAHE", HistogramMethod::CLAHE,
               "Contrast Limited Adaptive Histogram Equalization")
        .value("LOCAL", HistogramMethod::LOCAL, "Local histogram equalization")
        .value("MULTI_SCALE", HistogramMethod::MULTI_SCALE,
               "Multi-scale histogram equalization")
        .export_values();

    // ToneMappingOperator enum
    py::enum_<ToneMappingOperator>(enhance_module, "ToneMappingOperator",
                                   "Tone mapping operators for HDR images")
        .value("REINHARD", ToneMappingOperator::REINHARD,
               "Reinhard tone mapping")
        .value("DRAGO", ToneMappingOperator::DRAGO, "Drago tone mapping")
        .value("MANTIUK", ToneMappingOperator::MANTIUK, "Mantiuk tone mapping")
        .value("FATTAL", ToneMappingOperator::FATTAL, "Fattal gradient domain")
        .value("DURAND", ToneMappingOperator::DURAND, "Durand bilateral")
        .value("GAMMA", ToneMappingOperator::GAMMA, "Simple gamma correction")
        .value("LINEAR", ToneMappingOperator::LINEAR, "Linear mapping")
        .value("LOGARITHMIC", ToneMappingOperator::LOGARITHMIC,
               "Logarithmic mapping")
        .export_values();

    // ColorCorrectionMethod enum
    py::enum_<ColorCorrectionMethod>(enhance_module, "ColorCorrectionMethod",
                                     "Color correction methods")
        .value("WHITE_BALANCE", ColorCorrectionMethod::WHITE_BALANCE,
               "White balance")
        .value("COLOR_CAST", ColorCorrectionMethod::COLOR_CAST,
               "Color cast removal")
        .value("GAMMA_CORRECTION", ColorCorrectionMethod::GAMMA_CORRECTION,
               "Gamma correction")
        .value("CURVES", ColorCorrectionMethod::CURVES, "Curves adjustment")
        .value("LEVELS", ColorCorrectionMethod::LEVELS, "Levels adjustment")
        .value("COLOR_GRADING", ColorCorrectionMethod::COLOR_GRADING,
               "Color grading")
        .value("AUTO_LEVELS", ColorCorrectionMethod::AUTO_LEVELS, "Auto levels")
        .value("AUTO_COLOR", ColorCorrectionMethod::AUTO_COLOR, "Auto color")
        .export_values();

    // EnhancementParams struct
    py::class_<EnhancementParams>(enhance_module, "EnhancementParams",
                                  "Parameters for image enhancement operations")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("brightness", &EnhancementParams::brightness,
                       "Brightness adjustment (-100 to 100)")
        .def_readwrite("contrast", &EnhancementParams::contrast,
                       "Contrast adjustment (0.0 to 3.0)")
        .def_readwrite("saturation", &EnhancementParams::saturation,
                       "Saturation adjustment (0.0 to 3.0)")
        .def_readwrite("hue", &EnhancementParams::hue,
                       "Hue shift (-180 to 180)")
        .def_readwrite("gamma", &EnhancementParams::gamma,
                       "Gamma value (0.1 to 3.0)")
        .def_readwrite("exposure", &EnhancementParams::exposure,
                       "Exposure adjustment in stops")
        .def_readwrite("shadows", &EnhancementParams::shadows,
                       "Shadow recovery (0 to 100)")
        .def_readwrite("highlights", &EnhancementParams::highlights,
                       "Highlight recovery (0 to 100)")
        .def_readwrite("clarity", &EnhancementParams::clarity,
                       "Clarity/local contrast (0 to 100)")
        .def_readwrite("vibrance", &EnhancementParams::vibrance,
                       "Vibrance (smart saturation)")
        .def_readwrite("sharpness", &EnhancementParams::sharpness,
                       "Sharpness amount")
        .def_readwrite("denoiseStrength", &EnhancementParams::denoiseStrength,
                       "Denoising strength")
        .def_readwrite("temperature", &EnhancementParams::temperature,
                       "Color temperature (2000-10000K)")
        .def_readwrite("tint", &EnhancementParams::tint, "Green-magenta tint")
        .def("__repr__", [](const EnhancementParams& self) {
            return "<EnhancementParams brightness=" +
                   std::to_string(self.brightness) +
                   " contrast=" + std::to_string(self.contrast) + ">";
        });

    // ImageEnhancement class
    py::class_<ImageEnhancement>(enhance_module, "ImageEnhancement",
                                 R"pbdoc(
        Comprehensive image enhancement processor.

        Provides various image enhancement operations including color adjustment,
        histogram manipulation, tone mapping, denoising, and sharpening.

        Example:
            >>> enhancer = ImageEnhancement()
            >>> params = EnhancementParams()
            >>> params.brightness = 20
            >>> params.contrast = 1.2
            >>> enhanced = enhancer.enhance(image, params)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("enhance", &ImageEnhancement::enhance, py::arg("input"),
             py::arg("params"),
             R"pbdoc(
            Apply multiple enhancements using parameters.

            Args:
                input: Input image
                params: Enhancement parameters

            Returns:
                Enhanced image
            )pbdoc")
        .def("adjustBrightness", &ImageEnhancement::adjustBrightness,
             py::arg("input"), py::arg("value"),
             "Adjust brightness (-100 to 100)")
        .def("adjustContrast", &ImageEnhancement::adjustContrast,
             py::arg("input"), py::arg("value"), "Adjust contrast (0.0 to 3.0)")
        .def("adjustSaturation", &ImageEnhancement::adjustSaturation,
             py::arg("input"), py::arg("value"),
             "Adjust saturation (0.0 to 3.0)")
        .def("adjustHue", &ImageEnhancement::adjustHue, py::arg("input"),
             py::arg("value"), "Adjust hue (-180 to 180)")
        .def("adjustGamma", &ImageEnhancement::adjustGamma, py::arg("input"),
             py::arg("gamma"), "Apply gamma correction (0.1 to 3.0)")
        .def("adjustExposure", &ImageEnhancement::adjustExposure,
             py::arg("input"), py::arg("stops"), "Adjust exposure in stops")
        .def("adjustTemperature", &ImageEnhancement::adjustTemperature,
             py::arg("input"), py::arg("temperature"),
             "Adjust color temperature (2000-10000K)")
        .def("adjustTint", &ImageEnhancement::adjustTint, py::arg("input"),
             py::arg("tint"), "Adjust green-magenta tint")
        .def("equalizeHistogram", &ImageEnhancement::equalizeHistogram,
             py::arg("input"), py::arg("method") = HistogramMethod::CLAHE,
             py::arg("clipLimit") = 2.0, py::arg("tileSize") = 8,
             R"pbdoc(
            Equalize image histogram.

            Args:
                input: Input image
                method: Histogram equalization method
                clipLimit: CLAHE clip limit
                tileSize: CLAHE tile size

            Returns:
                Equalized image
            )pbdoc")
        .def("matchHistogram", &ImageEnhancement::matchHistogram,
             py::arg("input"), py::arg("reference"),
             "Match histogram to reference image")
        .def("toneMap", &ImageEnhancement::toneMap, py::arg("input"),
             py::arg("tmo") = ToneMappingOperator::REINHARD,
             py::arg("gamma") = 2.2, py::arg("saturation") = 1.0,
             R"pbdoc(
            Apply tone mapping for HDR images.

            Args:
                input: Input HDR image
                tmo: Tone mapping operator
                gamma: Gamma correction value
                saturation: Output saturation

            Returns:
                Tone-mapped LDR image
            )pbdoc")
        .def("correctColor", &ImageEnhancement::correctColor, py::arg("input"),
             py::arg("method") = ColorCorrectionMethod::AUTO_COLOR,
             "Apply color correction")
        .def("autoWhiteBalance", &ImageEnhancement::autoWhiteBalance,
             py::arg("input"), py::arg("method") = "gray_world",
             "Automatically adjust white balance")
        .def("sharpen", &ImageEnhancement::sharpen, py::arg("input"),
             py::arg("amount") = 1.0, py::arg("radius") = 1.0,
             py::arg("threshold") = 0.0,
             R"pbdoc(
            Sharpen image using unsharp mask.

            Args:
                input: Input image
                amount: Sharpening amount (0.0 to 2.0)
                radius: Sharpening radius
                threshold: Edge threshold

            Returns:
                Sharpened image
            )pbdoc")
        .def("denoise", &ImageEnhancement::denoise, py::arg("input"),
             py::arg("strength") = 10.0, py::arg("method") = "bilateral",
             "Remove noise from image")
        .def("adjustShadowsHighlights",
             &ImageEnhancement::adjustShadowsHighlights, py::arg("input"),
             py::arg("shadows") = 0.0, py::arg("highlights") = 0.0,
             "Adjust shadows and highlights independently")
        .def("adjustClarity", &ImageEnhancement::adjustClarity,
             py::arg("input"), py::arg("amount") = 50.0,
             "Adjust clarity (local contrast)")
        .def("adjustVibrance", &ImageEnhancement::adjustVibrance,
             py::arg("input"), py::arg("amount") = 50.0,
             "Adjust vibrance (smart saturation)")
        .def("applyLUT", &ImageEnhancement::applyLUT, py::arg("input"),
             py::arg("lutPath"), "Apply color lookup table from file")
        .def("convertColorSpace", &ImageEnhancement::convertColorSpace,
             py::arg("input"), py::arg("from"), py::arg("to"),
             "Convert between color spaces")
        .def("autoEnhance", &ImageEnhancement::autoEnhance, py::arg("input"),
             py::arg("strength") = 1.0,
             R"pbdoc(
            Automatically enhance image quality.

            Analyzes image and applies appropriate enhancements.

            Args:
                input: Input image
                strength: Enhancement strength (0.0 to 2.0)

            Returns:
                Enhanced image
            )pbdoc")
        .def("analyzeImage", &ImageEnhancement::analyzeImage, py::arg("input"),
             R"pbdoc(
            Analyze image characteristics.

            Returns dictionary with:
            - brightness: Average brightness
            - contrast: Contrast level
            - saturation: Average saturation
            - sharpness: Sharpness metric
            - noise: Estimated noise level
            - histogram: Histogram data
            )pbdoc")
        .def("suggestEnhancements", &ImageEnhancement::suggestEnhancements,
             py::arg("input"),
             "Suggest enhancement parameters based on image analysis");

    // Factory function
    enhance_module.def(
        "createOptimalEnhancement", &createOptimalEnhancement,
        py::arg("useGPU") = false,
        "Create optimal enhancement processor for current hardware");
}
