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
        .value("CONSTANT", BorderMode::CONSTANT, "Constant border value")
        .value("REPLICATE", BorderMode::REPLICATE, "Replicate edge pixels")
        .value("REFLECT", BorderMode::REFLECT, "Reflect border")
        .value("WRAP", BorderMode::WRAP, "Wrap around")
        .value("TRANSPARENT", BorderMode::TRANSPARENT, "Transparent border")
        .export_values();

    // TransformMatrix type alias (3x3 matrix as nested array)
    transform_module.attr("TransformMatrix") =
        py::module::import("typing").attr("List");

    // Point2D struct
    py::class_<Point2D>(transform_module, "Point2D", "2D point")
        .def(py::init<>(), "Default constructor")
        .def(py::init<double, double>(), py::arg("x"), py::arg("y"),
             "Construct with coordinates")
        .def_readwrite("x", &Point2D::x, "X coordinate")
        .def_readwrite("y", &Point2D::y, "Y coordinate")
        .def("__repr__", [](const Point2D& self) {
            return "<Point2D x=" + std::to_string(self.x) +
                   " y=" + std::to_string(self.y) + ">";
        });

    // Rectangle struct
    py::class_<Rectangle>(transform_module, "Rectangle", "Rectangle region")
        .def(py::init<>(), "Default constructor")
        .def(py::init<double, double, double, double>(), py::arg("x"),
             py::arg("y"), py::arg("width"), py::arg("height"),
             "Construct with position and size")
        .def_readwrite("x", &Rectangle::x, "X position")
        .def_readwrite("y", &Rectangle::y, "Y position")
        .def_readwrite("width", &Rectangle::width, "Width")
        .def_readwrite("height", &Rectangle::height, "Height")
        .def("area", &Rectangle::area, "Calculate area")
        .def("center", &Rectangle::center, "Get center point")
        .def("contains", &Rectangle::contains, py::arg("point"),
             "Check if point is inside")
        .def("intersects", &Rectangle::intersects, py::arg("other"),
             "Check if intersects with another rectangle")
        .def("__repr__", [](const Rectangle& self) {
            return "<Rectangle x=" + std::to_string(self.x) +
                   " y=" + std::to_string(self.y) +
                   " w=" + std::to_string(self.width) +
                   " h=" + std::to_string(self.height) + ">";
        });

    // ImageTransform class
    py::class_<ImageTransform>(transform_module, "ImageTransform",
                               R"pbdoc(
        Comprehensive image transformation processor.

        Provides geometric transformation operations including resize, rotate,
        affine and perspective transforms, distortion correction, and more.

        Example:
            >>> transform = ImageTransform()
            >>> resized = transform.resize(image, 800, 600)
            >>> rotated = transform.rotate(image, 45.0)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("resize", &ImageTransform::resize, py::arg("input"),
             py::arg("newWidth"), py::arg("newHeight"),
             py::arg("interpolation") = InterpolationMethod::LINEAR,
             R"pbdoc(
            Resize image to specified dimensions.

            Args:
                input: Input image
                newWidth: Target width
                newHeight: Target height
                interpolation: Interpolation method

            Returns:
                Resized image
            )pbdoc")
        .def("rotate", &ImageTransform::rotate, py::arg("input"),
             py::arg("angleDegrees"), py::arg("center") = Point2D{-1, -1},
             py::arg("scale") = 1.0,
             py::arg("interpolation") = InterpolationMethod::LINEAR,
             py::arg("borderMode") = BorderMode::CONSTANT,
             R"pbdoc(
            Rotate image by angle in degrees.

            Args:
                input: Input image
                angleDegrees: Rotation angle in degrees
                center: Rotation center (default: image center)
                scale: Scale factor
                interpolation: Interpolation method
                borderMode: Border handling mode

            Returns:
                Rotated image
            )pbdoc")
        .def("affineTransform", &ImageTransform::affineTransform,
             py::arg("input"), py::arg("matrix"),
             py::arg("outputSize") = std::pair<int, int>{0, 0},
             py::arg("interpolation") = InterpolationMethod::LINEAR,
             py::arg("borderMode") = BorderMode::CONSTANT,
             R"pbdoc(
            Apply affine transformation.

            Args:
                input: Input image
                matrix: 2x3 affine transformation matrix
                outputSize: Output size (0,0 = same as input)
                interpolation: Interpolation method
                borderMode: Border handling mode

            Returns:
                Transformed image
            )pbdoc")
        .def("perspectiveTransform", &ImageTransform::perspectiveTransform,
             py::arg("input"), py::arg("matrix"),
             py::arg("outputSize") = std::pair<int, int>{0, 0},
             py::arg("interpolation") = InterpolationMethod::LINEAR,
             py::arg("borderMode") = BorderMode::CONSTANT,
             "Apply perspective transformation with 3x3 matrix")
        .def("correctPerspective", &ImageTransform::correctPerspective,
             py::arg("input"), py::arg("sourcePoints"), py::arg("targetPoints"),
             py::arg("outputSize") = std::pair<int, int>{0, 0},
             "Correct perspective using point correspondences")
        .def("correctDistortion", &ImageTransform::correctDistortion,
             py::arg("input"), py::arg("k1"), py::arg("k2") = 0.0,
             py::arg("k3") = 0.0, py::arg("p1") = 0.0, py::arg("p2") = 0.0,
             R"pbdoc(
            Correct lens distortion.

            Args:
                input: Input image
                k1, k2, k3: Radial distortion coefficients
                p1, p2: Tangential distortion coefficients

            Returns:
                Corrected image
            )pbdoc")
        .def("elasticDeform", &ImageTransform::elasticDeform, py::arg("input"),
             py::arg("alpha"), py::arg("sigma"),
             py::arg("interpolation") = InterpolationMethod::LINEAR,
             "Apply elastic deformation for data augmentation")
        .def("toPolar", &ImageTransform::toPolar, py::arg("input"),
             py::arg("center") = Point2D{-1, -1}, py::arg("maxRadius") = -1.0,
             "Convert image to polar coordinates")
        .def("fromPolar", &ImageTransform::fromPolar, py::arg("input"),
             py::arg("center") = Point2D{-1, -1},
             py::arg("outputSize") = std::pair<int, int>{0, 0},
             "Convert image from polar coordinates")
        .def("registerImages", &ImageTransform::registerImages,
             py::arg("reference"), py::arg("target"),
             py::arg("method") = "affine",
             R"pbdoc(
            Register target image to reference image.

            Args:
                reference: Reference image
                target: Image to align
                method: Registration method ("rigid", "affine", "perspective")

            Returns:
                Tuple of (aligned image, transformation matrix)
            )pbdoc")
        .def("warpControlPoints", &ImageTransform::warpControlPoints,
             py::arg("input"), py::arg("srcPoints"), py::arg("dstPoints"),
             py::arg("method") = "tps", "Warp image using control point pairs")
        .def("stitchPanorama", &ImageTransform::stitchPanorama,
             py::arg("images"), py::arg("blendMode") = "linear",
             "Stitch multiple images into panorama")
        .def_static("createIdentityMatrix",
                    &ImageTransform::createIdentityMatrix,
                    "Create 3x3 identity transformation matrix")
        .def_static("createTranslationMatrix",
                    &ImageTransform::createTranslationMatrix, py::arg("tx"),
                    py::arg("ty"), "Create translation matrix")
        .def_static("createRotationMatrix",
                    &ImageTransform::createRotationMatrix, py::arg("angle"),
                    py::arg("centerX") = 0.0, py::arg("centerY") = 0.0,
                    "Create rotation matrix")
        .def_static("createScaleMatrix", &ImageTransform::createScaleMatrix,
                    py::arg("sx"), py::arg("sy"), "Create scale matrix")
        .def_static("createShearMatrix", &ImageTransform::createShearMatrix,
                    py::arg("shx"), py::arg("shy"), "Create shear matrix")
        .def_static("composeMatrices", &ImageTransform::composeMatrices,
                    py::arg("m1"), py::arg("m2"),
                    "Compose two transformation matrices")
        .def_static("invertMatrix", &ImageTransform::invertMatrix,
                    py::arg("matrix"), "Invert transformation matrix")
        .def_static("transformPoint", &ImageTransform::transformPoint,
                    py::arg("matrix"), py::arg("point"),
                    "Transform a point using matrix");

    // Factory function
    transform_module.def(
        "createOptimalTransform", &createOptimalTransform,
        py::arg("useGPU") = false,
        "Create optimal transform processor for current hardware");
}
