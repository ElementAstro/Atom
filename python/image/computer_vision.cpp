/**
 * @file computer_vision.cpp
 * @brief Python bindings for computer vision operations
 */

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/computer_vision.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_computer_vision(py::module& m) {
    auto cv_module =
        m.def_submodule("cv", "Computer vision algorithms and operations");

    // Feature detection
    cv_module.def("detect_corners", &detectCorners, py::arg("input"),
                  py::arg("max_corners") = 100, py::arg("quality_level") = 0.01,
                  py::arg("min_distance") = 10.0,
                  R"pbdoc(
        Detect corners in image using Harris or Shi-Tomasi method.

        Args:
            input: Input image
            max_corners: Maximum number of corners to detect
            quality_level: Quality threshold for corner detection
            min_distance: Minimum distance between corners

        Returns:
            List of detected corner coordinates
        )pbdoc");

    cv_module.def("detect_features", &detectFeatures, py::arg("input"),
                  py::arg("method") = "SIFT",
                  R"pbdoc(
        Detect features using various methods (SIFT, SURF, ORB, etc.).

        Args:
            input: Input image
            method: Feature detection method

        Returns:
            List of detected keypoints
        )pbdoc");

    cv_module.def("match_features", &matchFeatures, py::arg("keypoints1"),
                  py::arg("keypoints2"), py::arg("descriptors1"),
                  py::arg("descriptors2"), py::arg("method") = "BF",
                  R"pbdoc(
        Match features between two images.

        Args:
            keypoints1: Keypoints from first image
            keypoints2: Keypoints from second image
            descriptors1: Descriptors from first image
            descriptors2: Descriptors from second image
            method: Matching method (BF, FLANN)

        Returns:
            List of matches
        )pbdoc");

    // Object detection
    cv_module.def("detect_faces", &detectFaces, py::arg("input"),
                  py::arg("scale_factor") = 1.1, py::arg("min_neighbors") = 3,
                  R"pbdoc(
        Detect faces in image using Haar cascades.

        Args:
            input: Input image
            scale_factor: Scale factor for detection
            min_neighbors: Minimum number of neighbors

        Returns:
            List of face bounding boxes
        )pbdoc");

    cv_module.def("detect_objects", &detectObjects, py::arg("input"),
                  py::arg("cascade_file"), py::arg("scale_factor") = 1.1,
                  py::arg("min_neighbors") = 3,
                  R"pbdoc(
        Detect objects using custom Haar cascade.

        Args:
            input: Input image
            cascade_file: Path to cascade XML file
            scale_factor: Scale factor for detection
            min_neighbors: Minimum number of neighbors

        Returns:
            List of object bounding boxes
        )pbdoc");

    // Segmentation
    cv_module.def("segment_watershed", &segmentWatershed, py::arg("input"),
                  py::arg("markers"),
                  R"pbdoc(
        Segment image using watershed algorithm.

        Args:
            input: Input image
            markers: Marker image for watershed

        Returns:
            Segmented image
        )pbdoc");

    cv_module.def("segment_grabcut", &segmentGrabCut, py::arg("input"),
                  py::arg("rect"), py::arg("iterations") = 5,
                  R"pbdoc(
        Segment foreground using GrabCut algorithm.

        Args:
            input: Input image
            rect: Initial rectangle containing foreground
            iterations: Number of iterations

        Returns:
            Segmentation mask
        )pbdoc");

    cv_module.def("segment_kmeans", &segmentKMeans, py::arg("input"),
                  py::arg("k") = 3, py::arg("iterations") = 10,
                  R"pbdoc(
        Segment image using K-means clustering.

        Args:
            input: Input image
            k: Number of clusters
            iterations: Maximum iterations

        Returns:
            Segmented image
        )pbdoc");

    // Optical flow
    cv_module.def("compute_optical_flow", &computeOpticalFlow,
                  py::arg("prev_frame"), py::arg("next_frame"),
                  py::arg("method") = "farneback",
                  R"pbdoc(
        Compute optical flow between frames.

        Args:
            prev_frame: Previous frame
            next_frame: Next frame
            method: Optical flow method (farneback, lucas_kanade)

        Returns:
            Flow field
        )pbdoc");

    // Template matching
    cv_module.def("match_template", &matchTemplate, py::arg("input"),
                  py::arg("template_img"),
                  py::arg("method") = "TM_CCOEFF_NORMED",
                  R"pbdoc(
        Find template in image.

        Args:
            input: Input image
            template_img: Template to find
            method: Matching method

        Returns:
            Match result image
        )pbdoc");

    cv_module.def("find_template_locations", &findTemplateLocations,
                  py::arg("input"), py::arg("template_img"),
                  py::arg("threshold") = 0.8,
                  R"pbdoc(
        Find all locations of template in image.

        Args:
            input: Input image
            template_img: Template to find
            threshold: Match threshold

        Returns:
            List of match locations
        )pbdoc");

    // Contour operations
    cv_module.def("find_contours", &findContours, py::arg("input"),
                  py::arg("mode") = "external", py::arg("method") = "simple",
                  R"pbdoc(
        Find contours in binary image.

        Args:
            input: Binary input image
            mode: Contour retrieval mode
            method: Contour approximation method

        Returns:
            List of contours
        )pbdoc");

    cv_module.def("draw_contours", &drawContours, py::arg("input"),
                  py::arg("contours"),
                  py::arg("color") = std::vector<int>{255, 0, 0},
                  py::arg("thickness") = 2,
                  R"pbdoc(
        Draw contours on image.

        Args:
            input: Input image
            contours: List of contours to draw
            color: Drawing color (RGB)
            thickness: Line thickness

        Returns:
            Image with drawn contours
        )pbdoc");

    cv_module.def("compute_contour_properties", &computeContourProperties,
                  py::arg("contour"),
                  R"pbdoc(
        Compute properties of a contour.

        Args:
            contour: Input contour

        Returns:
            Dictionary with area, perimeter, centroid, etc.
        )pbdoc");

    // Hough transforms
    cv_module.def("detect_lines_hough", &detectLinesHough, py::arg("input"),
                  py::arg("threshold") = 100, py::arg("min_line_length") = 50,
                  py::arg("max_line_gap") = 10,
                  R"pbdoc(
        Detect lines using Hough transform.

        Args:
            input: Binary edge image
            threshold: Accumulator threshold
            min_line_length: Minimum line length
            max_line_gap: Maximum gap between line segments

        Returns:
            List of detected lines
        )pbdoc");

    cv_module.def("detect_circles_hough", &detectCirclesHough, py::arg("input"),
                  py::arg("min_radius") = 0, py::arg("max_radius") = 0,
                  R"pbdoc(
        Detect circles using Hough transform.

        Args:
            input: Grayscale input image
            min_radius: Minimum circle radius
            max_radius: Maximum circle radius (0 = auto)

        Returns:
            List of detected circles (x, y, radius)
        )pbdoc");

    // Morphological operations
    cv_module.def("find_connected_components", &findConnectedComponents,
                  py::arg("input"), py::arg("connectivity") = 8,
                  R"pbdoc(
        Find connected components in binary image.

        Args:
            input: Binary input image
            connectivity: Connectivity (4 or 8)

        Returns:
            Labeled image and component statistics
        )pbdoc");

    cv_module.def("compute_distance_transform", &computeDistanceTransform,
                  py::arg("input"), py::arg("distance_type") = "L2",
                  R"pbdoc(
        Compute distance transform of binary image.

        Args:
            input: Binary input image
            distance_type: Distance metric (L1, L2, C)

        Returns:
            Distance transform image
        )pbdoc");

    // Image alignment
    cv_module.def("align_images", &alignImages, py::arg("reference"),
                  py::arg("target"), py::arg("method") = "ECC",
                  R"pbdoc(
        Align target image to reference image.

        Args:
            reference: Reference image
            target: Image to align
            method: Alignment method (ECC, ORB, SIFT)

        Returns:
            Aligned image and transformation matrix
        )pbdoc");

    cv_module.def("compute_homography", &computeHomography,
                  py::arg("src_points"), py::arg("dst_points"),
                  py::arg("method") = "RANSAC",
                  R"pbdoc(
        Compute homography between point sets.

        Args:
            src_points: Source points
            dst_points: Destination points
            method: Computation method (RANSAC, LMEDS)

        Returns:
            Homography matrix
        )pbdoc");

    // Stereo vision
    cv_module.def("compute_disparity", &computeDisparity, py::arg("left"),
                  py::arg("right"), py::arg("num_disparities") = 16,
                  py::arg("block_size") = 15,
                  R"pbdoc(
        Compute disparity map from stereo pair.

        Args:
            left: Left stereo image
            right: Right stereo image
            num_disparities: Number of disparities
            block_size: Block size for matching

        Returns:
            Disparity map
        )pbdoc");
}
