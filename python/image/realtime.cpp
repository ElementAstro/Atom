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
            >>> processor.set_source(realtime.CaptureSource.CAMERA, device_id=0)
            >>> processor.set_processing_mode(realtime.ProcessingMode.ENHANCE)
            >>> processor.start()
            >>>
            >>> while processor.is_running():
            >>>     frame = processor.get_frame()
            >>>     if frame:
            >>>         # Display or process frame
            >>>         pass
            >>>
            >>> processor.stop()
        )pbdoc")
        .def(py::init<const RealtimeParams&>(),
             py::arg("params") = RealtimeParams{}, "Construct with parameters")
        .def("set_source", &RealtimeProcessor::setSource, py::arg("source"),
             py::arg("source_id") = "",
             R"pbdoc(
            Set capture source.

            Args:
                source: Capture source type
                source_id: Source identifier (device ID, file path, URL)
            )pbdoc")
        .def("set_processing_mode", &RealtimeProcessor::setProcessingMode,
             py::arg("mode"), "Set processing mode")
        .def("set_processing_callback",
             &RealtimeProcessor::setProcessingCallback, py::arg("callback"),
             R"pbdoc(
            Set custom processing callback.

            The callback receives a frame and FrameInfo and should return
            the processed frame.

            Args:
                callback: Function(blob, FrameInfo) -> blob
            )pbdoc")
        .def("set_frame_callback", &RealtimeProcessor::setFrameCallback,
             py::arg("callback"), "Set callback for each processed frame")
        .def("start", &RealtimeProcessor::start, "Start processing")
        .def("stop", &RealtimeProcessor::stop, "Stop processing")
        .def("pause", &RealtimeProcessor::pause, "Pause processing")
        .def("resume", &RealtimeProcessor::resume, "Resume processing")
        .def("is_running", &RealtimeProcessor::isRunning,
             "Check if processor is running")
        .def("is_paused", &RealtimeProcessor::isPaused,
             "Check if processor is paused")
        .def("get_frame", &RealtimeProcessor::getFrame,
             py::arg("timeout_ms") = 0,
             R"pbdoc(
            Get next processed frame.

            Args:
                timeout_ms: Timeout in milliseconds (0=no wait, -1=infinite)

            Returns:
                Processed frame or None if timeout
            )pbdoc")
        .def("get_frame_info", &RealtimeProcessor::getFrameInfo,
             "Get information about current frame")
        .def("get_stats", &RealtimeProcessor::getStats,
             "Get processing statistics")
        .def("reset_stats", &RealtimeProcessor::resetStats,
             "Reset statistics counters")
        .def("set_params", &RealtimeProcessor::setParams, py::arg("params"),
             "Update processing parameters")
        .def("get_params", &RealtimeProcessor::getParams,
             "Get current parameters")
        .def("get_source_info", &RealtimeProcessor::getSourceInfo,
             "Get information about capture source")
        .def("set_roi", &RealtimeProcessor::setROI, py::arg("x"), py::arg("y"),
             py::arg("width"), py::arg("height"), "Set region of interest")
        .def("clear_roi", &RealtimeProcessor::clearROI,
             "Clear region of interest")
        .def("save_frame", &RealtimeProcessor::saveFrame, py::arg("filename"),
             "Save current frame to file")
        .def("start_recording", &RealtimeProcessor::startRecording,
             py::arg("filename"), py::arg("codec") = "H264",
             "Start recording to video file")
        .def("stop_recording", &RealtimeProcessor::stopRecording,
             "Stop recording")
        .def("is_recording", &RealtimeProcessor::isRecording,
             "Check if recording is active")
        .def("__enter__",
             [](RealtimeProcessor& self) -> RealtimeProcessor& {
                 self.start();
                 return self;
             })
        .def("__exit__", [](RealtimeProcessor& self, py::object, py::object,
                            py::object) { self.stop(); });

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
