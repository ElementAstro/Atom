/**
 * @file realtime.cpp
 * @brief Python bindings for real-time image processing
 */

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/image/processing/realtime.hpp"

namespace py = pybind11;
using namespace atom::image;

void bind_realtime(py::module& m) {
    auto rt_module = m.def_submodule(
        "realtime", "Real-time image processing for video streams");

    // CaptureSource enum
    py::enum_<CaptureSource>(rt_module, "CaptureSource",
                             "Video capture source types")
        .value("CAMERA", CaptureSource::CAMERA, "Camera device")
        .value("FILE", CaptureSource::FILE, "Video file")
        .value("STREAM", CaptureSource::STREAM, "Network stream")
        .value("SCREEN", CaptureSource::SCREEN, "Screen capture")
        .value("SYNTHETIC", CaptureSource::SYNTHETIC, "Synthetic frames")
        .value("CUSTOM", CaptureSource::CUSTOM, "Custom source")
        .export_values();

    // ProcessingMode enum
    py::enum_<ProcessingMode>(rt_module, "ProcessingMode",
                              "Real-time processing modes")
        .value("PASSTHROUGH", ProcessingMode::PASSTHROUGH, "No processing")
        .value("FILTER", ProcessingMode::FILTER, "Apply filters")
        .value("ENHANCE", ProcessingMode::ENHANCE, "Image enhancement")
        .value("DETECT", ProcessingMode::DETECT, "Object detection")
        .value("TRACK", ProcessingMode::TRACK, "Object tracking")
        .value("ANALYZE", ProcessingMode::ANALYZE, "Image analysis")
        .value("CUSTOM", ProcessingMode::CUSTOM, "Custom pipeline")
        .export_values();

    // FrameInfo struct
    py::class_<FrameInfo>(rt_module, "FrameInfo",
                          "Information about a video frame")
        .def(py::init<>())
        .def_readwrite("timestamp", &FrameInfo::timestamp,
                       "Frame timestamp in microseconds")
        .def_readwrite("frame_number", &FrameInfo::frameNumber,
                       "Frame sequence number")
        .def_readwrite("fps", &FrameInfo::fps, "Current frames per second")
        .def_readwrite("width", &FrameInfo::width, "Frame width")
        .def_readwrite("height", &FrameInfo::height, "Frame height")
        .def_readwrite("channels", &FrameInfo::channels,
                       "Number of color channels")
        .def_readwrite("format", &FrameInfo::format, "Pixel format")
        .def_readwrite("metadata", &FrameInfo::metadata, "Additional metadata")
        .def("__repr__", [](const FrameInfo& self) {
            return "<FrameInfo frame=" + std::to_string(self.frameNumber) +
                   " size=" + std::to_string(self.width) + "x" +
                   std::to_string(self.height) +
                   " fps=" + std::to_string(self.fps) + ">";
        });

    // ProcessingStats struct
    py::class_<ProcessingStats>(rt_module, "ProcessingStats",
                                "Real-time processing statistics")
        .def(py::init<>())
        .def_readwrite("average_fps", &ProcessingStats::averageFPS,
                       "Average processing FPS")
        .def_readwrite("current_fps", &ProcessingStats::currentFPS,
                       "Current processing FPS")
        .def_readwrite("average_latency", &ProcessingStats::averageLatency,
                       "Average latency in milliseconds")
        .def_readwrite("current_latency", &ProcessingStats::currentLatency,
                       "Current latency in milliseconds")
        .def_readwrite("frames_processed", &ProcessingStats::framesProcessed,
                       "Total frames processed")
        .def_readwrite("frames_dropped", &ProcessingStats::framesDropped,
                       "Total frames dropped")
        .def_readwrite("cpu_usage", &ProcessingStats::cpuUsage,
                       "CPU usage percentage")
        .def_readwrite("memory_usage", &ProcessingStats::memoryUsage,
                       "Memory usage in MB")
        .def_readwrite("gpu_usage", &ProcessingStats::gpuUsage,
                       "GPU usage percentage")
        .def_readwrite("status", &ProcessingStats::status, "Current status")
        .def("__repr__", [](const ProcessingStats& self) {
            return "<ProcessingStats fps=" + std::to_string(self.currentFPS) +
                   " latency=" + std::to_string(self.currentLatency) + "ms" +
                   " processed=" + std::to_string(self.framesProcessed) + ">";
        });

    // RealtimeParams struct
    py::class_<RealtimeParams>(rt_module, "Params",
                               "Parameters for real-time processing")
        .def(py::init<>())
        .def_readwrite("max_buffer_size", &RealtimeParams::maxBufferSize,
                       "Maximum frame buffer size")
        .def_readwrite("num_threads", &RealtimeParams::numThreads,
                       "Number of processing threads (0=auto)")
        .def_readwrite("use_gpu", &RealtimeParams::useGPU,
                       "Use GPU acceleration")
        .def_readwrite("drop_frames", &RealtimeParams::dropFrames,
                       "Drop frames if processing is slow")
        .def_readwrite("target_fps", &RealtimeParams::targetFPS,
                       "Target processing FPS")
        .def_readwrite("max_width", &RealtimeParams::maxWidth,
                       "Maximum frame width")
        .def_readwrite("max_height", &RealtimeParams::maxHeight,
                       "Maximum frame height")
        .def_readwrite("maintain_aspect_ratio",
                       &RealtimeParams::maintainAspectRatio,
                       "Maintain aspect ratio when resizing")
        .def_readwrite("pixel_format", &RealtimeParams::pixelFormat,
                       "Preferred pixel format");

    // RealtimeProcessor class
    py::class_<RealtimeProcessor>(rt_module, "Processor",
                                  R"pbdoc(
        Real-time image processor for video streams.

        Provides low-latency processing of video frames with support for
        various capture sources, processing pipelines, and output modes.

        Example:
            >>> processor = realtime.Processor()
            >>> processor.initialize(realtime.CaptureSource.CAMERA, device_id=0)
            >>> processor.setProcessingMode(realtime.ProcessingMode.ENHANCE)
            >>> processor.startCapture()
            >>>
            >>> while processor.isRunning():
            >>>     frame = processor.getFrame()
            >>>     if frame:
            >>>         # Display or process frame
            >>>         pass
            >>>
            >>> processor.stopCapture()
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<const RealtimeParams&>(), py::arg("params"),
             "Construct with parameters")
        .def("initialize", &RealtimeProcessor::initialize, py::arg("source"),
             py::arg("sourceId") = "",
             R"pbdoc(
            Initialize with capture source.

            Args:
                source: Capture source type
                sourceId: Source identifier (device ID, file path, URL)
            )pbdoc")
        .def("startCapture", &RealtimeProcessor::startCapture,
             "Start frame capture")
        .def("stopCapture", &RealtimeProcessor::stopCapture,
             "Stop frame capture")
        .def("processFrame", &RealtimeProcessor::processFrame, py::arg("frame"),
             "Process a single frame")
        .def("getFrame", &RealtimeProcessor::getFrame, py::arg("timeoutMs") = 0,
             R"pbdoc(
            Get next processed frame.

            Args:
                timeoutMs: Timeout in milliseconds (0=no wait, -1=infinite)

            Returns:
                Processed frame or None if timeout
            )pbdoc")
        .def("setProcessingMode", &RealtimeProcessor::setProcessingMode,
             py::arg("mode"), "Set processing mode")
        .def("addFilter", &RealtimeProcessor::addFilter, py::arg("filterType"),
             py::arg("params") = std::map<std::string, double>{},
             "Add filter to processing pipeline")
        .def("clearFilters", &RealtimeProcessor::clearFilters,
             "Clear all filters from pipeline")
        .def("setFrameCallback", &RealtimeProcessor::setFrameCallback,
             py::arg("callback"),
             R"pbdoc(
            Set callback for each processed frame.

            Args:
                callback: Function(blob, FrameInfo) -> void
            )pbdoc")
        .def("setAnalysisCallback", &RealtimeProcessor::setAnalysisCallback,
             py::arg("callback"), "Set callback for frame analysis results")
        .def("getStatistics", &RealtimeProcessor::getStatistics,
             "Get processing statistics")
        .def("pause", &RealtimeProcessor::pause, "Pause processing")
        .def("resume", &RealtimeProcessor::resume, "Resume processing")
        .def("isRunning", &RealtimeProcessor::isRunning,
             "Check if processor is running")
        .def("isPaused", &RealtimeProcessor::isPaused,
             "Check if processor is paused")
        .def("startRecording", &RealtimeProcessor::startRecording,
             py::arg("outputPath"), py::arg("codec") = "H264",
             py::arg("fps") = 30.0, "Start recording to video file")
        .def("stopRecording", &RealtimeProcessor::stopRecording,
             "Stop recording")
        .def("isRecording", &RealtimeProcessor::isRecording,
             "Check if recording is active")
        .def("takeSnapshot", &RealtimeProcessor::takeSnapshot,
             py::arg("outputPath"), "Save current frame to file")
        .def("listCaptureDevices", &RealtimeProcessor::listCaptureDevices,
             "List available capture devices")
        .def("getCaptureSettings", &RealtimeProcessor::getCaptureSettings,
             "Get current capture settings")
        .def("setCaptureSettings", &RealtimeProcessor::setCaptureSettings,
             py::arg("settings"), "Set capture settings")
        .def("__enter__",
             [](RealtimeProcessor& self) -> RealtimeProcessor& {
                 self.startCapture();
                 return self;
             })
        .def("__exit__", [](RealtimeProcessor& self, py::object, py::object,
                            py::object) { self.stopCapture(); });

    // Convenience functions
    rt_module.def("process_video", &processVideo, py::arg("input_file"),
                  py::arg("output_file"), py::arg("processing_callback"),
                  py::arg("params") = RealtimeParams{},
                  R"pbdoc(
        Process video file with custom callback.

        Args:
            input_file: Input video file path
            output_file: Output video file path
            processing_callback: Processing function
            params: Processing parameters
        )pbdoc");

    rt_module.def("capture_frames", &captureFrames, py::arg("source"),
                  py::arg("num_frames"), py::arg("source_id") = "",
                  py::arg("params") = RealtimeParams{},
                  R"pbdoc(
        Capture specified number of frames.

        Args:
            source: Capture source
            num_frames: Number of frames to capture
            source_id: Source identifier
            params: Capture parameters

        Returns:
            List of captured frames
        )pbdoc");

    rt_module.def("stream_to_callback", &streamToCallback, py::arg("source"),
                  py::arg("callback"), py::arg("source_id") = "",
                  py::arg("params") = RealtimeParams{},
                  "Stream frames to callback function");
}
