/**
 * @file transforms.cpp
 * @brief Python bindings for geometric transformations
 */

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/transforms.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_transforms(py::module& m) {
    auto transform_module =
        m.def_submodule("transforms", "Geometric transformation operations");

    // Interpolation methods
    py::enum_<InterpolationMethod>(transform_module, "InterpolationMethod",
                                   "Interpolation methods for transformations")
        .value("NEAREST", InterpolationMethod::NEAREST, "Nearest neighbor")
        .value("LINEAR", InterpolationMethod::LINEAR, "Bilinear interpolation")
        .value("CUBIC", InterpolationMethod::CUBIC, "Bicubic interpolation")
        .value("LANCZOS", InterpolationMethod::LANCZOS, "Lanczos interpolation")
        .value("AREA", InterpolationMethod::AREA, "Area-based interpolation")
        .export_values();

    // Border modes
    py::enum_<BorderMode>(transform_module, "BorderMode",
                          "Border handling modes for transformations")
        .value("CONSTANT", BorderMode::CONSTANT, "Constant border")
        .value("REPLICATE", BorderMode::REPLICATE, "Replicate edge pixels")
        .value("REFLECT", BorderMode::REFLECT, "Reflect border")
        .value("WRAP", BorderMode::WRAP, "Wrap around")
        .value("REFLECT_101", BorderMode::REFLECT_101, "Reflect 101 border")
        .export_values();

    // Transformation matrix
    py::class_<TransformMatrix>(transform_module, "Matrix",
                                "2D transformation matrix")
        .def(py::init<>(), "Default constructor (identity)")
        .def(py::init<const std::vector<double>&>(), py::arg("values"),
             "Construct from 6 or 9 values")
        .def_static("identity", &TransformMatrix::identity,
                    "Create identity matrix")
        .def_static("translation", &TransformMatrix::translation, py::arg("tx"),
                    py::arg("ty"), "Create translation matrix")
        .def_static("rotation", &TransformMatrix::rotation, py::arg("angle"),
                    py::arg("center_x") = 0.0, py::arg("center_y") = 0.0,
                    "Create rotation matrix")
        .def_static("scaling", &TransformMatrix::scaling, py::arg("sx"),
                    py::arg("sy") = -1.0, "Create scaling matrix")
        .def_static("shearing", &TransformMatrix::shearing, py::arg("shx"),
                    py::arg("shy"), "Create shearing matrix")
        .def("invert", &TransformMatrix::invert, "Get inverse matrix")
        .def("compose", &TransformMatrix::compose, py::arg("other"),
             "Compose with another matrix")
        .def("transform_point", &TransformMatrix::transformPoint, py::arg("x"),
             py::arg("y"), "Transform a point")
        .def("get_values", &TransformMatrix::getValues,
             "Get matrix values as list")
        .def("__mul__", &TransformMatrix::compose, "Compose matrices")
        .def("__repr__", [](const TransformMatrix& self) {
            auto vals = self.getValues();
            return "<TransformMatrix [" + std::to_string(vals[0]) + ", " +
                   std::to_string(vals[1]) + ", " + std::to_string(vals[2]) +
                   "]>";
        });

    // Basic transformations
    transform_module.def("resize", &resize, py::arg("input"), py::arg("width"),
                         py::arg("height"),
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         R"pbdoc(
        Resize image to specified dimensions.

        Args:
            input: Input image
            width: Target width
            height: Target height
            interpolation: Interpolation method

        Returns:
            Resized image
        )pbdoc");

    transform_module.def("scale", &scale, py::arg("input"), py::arg("scale_x"),
                         py::arg("scale_y") = -1.0,
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         "Scale image by factors");

    transform_module.def("rotate", &rotate, py::arg("input"), py::arg("angle"),
                         py::arg("center_x") = -1.0, py::arg("center_y") = -1.0,
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         py::arg("expand") = false,
                         R"pbdoc(
        Rotate image by angle in degrees.

        Args:
            input: Input image
            angle: Rotation angle in degrees (positive = counter-clockwise)
            center_x: X coordinate of rotation center (-1 = image center)
            center_y: Y coordinate of rotation center (-1 = image center)
            interpolation: Interpolation method
            expand: Expand canvas to fit rotated image

        Returns:
            Rotated image
        )pbdoc");

    transform_module.def("translate", &translate, py::arg("input"),
                         py::arg("tx"), py::arg("ty"),
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         "Translate image by offset");

    transform_module.def("shear", &shear, py::arg("input"), py::arg("shear_x"),
                         py::arg("shear_y"),
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         "Apply shear transformation");

    transform_module.def("flip_horizontal", &flipHorizontal, py::arg("input"),
                         "Flip image horizontally");

    transform_module.def("flip_vertical", &flipVertical, py::arg("input"),
                         "Flip image vertically");

    transform_module.def("transpose", &transpose, py::arg("input"),
                         "Transpose image (swap rows and columns)");

    transform_module.def("rotate_90", &rotate90, py::arg("input"),
                         py::arg("clockwise") = true,
                         "Rotate image by 90 degrees");

    transform_module.def("rotate_180", &rotate180, py::arg("input"),
                         "Rotate image by 180 degrees");

    transform_module.def("rotate_270", &rotate270, py::arg("input"),
                         "Rotate image by 270 degrees");

    // Advanced transformations
    transform_module.def("warp_affine", &warpAffine, py::arg("input"),
                         py::arg("matrix"), py::arg("output_width"),
                         py::arg("output_height"),
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         R"pbdoc(
        Apply affine transformation.

        Args:
            input: Input image
            matrix: 2x3 transformation matrix
            output_width: Output image width
            output_height: Output image height
            interpolation: Interpolation method
            border_mode: Border handling mode

        Returns:
            Transformed image
        )pbdoc");

    transform_module.def("warp_perspective", &warpPerspective, py::arg("input"),
                         py::arg("matrix"), py::arg("output_width"),
                         py::arg("output_height"),
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         "Apply perspective transformation");

    transform_module.def("remap", &remap, py::arg("input"), py::arg("map_x"),
                         py::arg("map_y"),
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         "Remap image using coordinate maps");

    // Geometric corrections
    transform_module.def("correct_lens_distortion", &correctLensDistortion,
                         py::arg("input"), py::arg("k1"), py::arg("k2") = 0.0,
                         py::arg("k3") = 0.0, py::arg("p1") = 0.0,
                         py::arg("p2") = 0.0,
                         R"pbdoc(
        Correct lens distortion.

        Args:
            input: Input image
            k1, k2, k3: Radial distortion coefficients
            p1, p2: Tangential distortion coefficients

        Returns:
            Corrected image
        )pbdoc");

    transform_module.def(
        "correct_perspective", &correctPerspective, py::arg("input"),
        py::arg("src_points"), py::arg("dst_points"),
        "Correct perspective distortion using point correspondences");

    transform_module.def("deskew", &deskew, py::arg("input"),
                         py::arg("auto_detect") = true, py::arg("angle") = 0.0,
                         "Deskew image (correct rotation)");

    // Cropping and padding
    transform_module.def("crop", &crop, py::arg("input"), py::arg("x"),
                         py::arg("y"), py::arg("width"), py::arg("height"),
                         "Crop image to rectangle");

    transform_module.def("crop_center", &cropCenter, py::arg("input"),
                         py::arg("width"), py::arg("height"),
                         "Crop centered rectangle");

    transform_module.def("pad", &pad, py::arg("input"), py::arg("top"),
                         py::arg("bottom"), py::arg("left"), py::arg("right"),
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         py::arg("value") = 0, "Add padding to image");

    transform_module.def("pad_to_size", &padToSize, py::arg("input"),
                         py::arg("width"), py::arg("height"),
                         py::arg("border_mode") = BorderMode::CONSTANT,
                         "Pad image to specific size");

    // Utility functions
    transform_module.def("fit_to_size", &fitToSize, py::arg("input"),
                         py::arg("width"), py::arg("height"),
                         py::arg("preserve_aspect") = true,
                         py::arg("interpolation") = InterpolationMethod::LINEAR,
                         "Fit image to size (resize and/or pad)");

    transform_module.def("thumbnail", &thumbnail, py::arg("input"),
                         py::arg("max_size"), py::arg("preserve_aspect") = true,
                         "Create thumbnail with maximum dimension");

    transform_module.def("aspect_fill", &aspectFill, py::arg("input"),
                         py::arg("width"), py::arg("height"),
                         "Resize to fill dimensions (may crop)");

    transform_module.def("aspect_fit", &aspectFit, py::arg("input"),
                         py::arg("width"), py::arg("height"),
                         "Resize to fit within dimensions (may pad)");
}
