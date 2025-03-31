// serastro.h
#pragma once

// Library version
#define SERASTRO_VERSION_MAJOR 1
#define SERASTRO_VERSION_MINOR 0
#define SERASTRO_VERSION_PATCH 0

// Core components
#include "exception.h"
#include "frame_processor.h"
#include "quality.h"
#include "registration.h"
#include "ser_format.h"
#include "ser_reader.h"
#include "ser_writer.h"
#include "stacking.h"
#include "utils.h"


namespace serastro {

// Library information
struct LibraryInfo {
    static constexpr const char* name = "SERastro";
    static constexpr const char* version = "1.0.0";
    static constexpr const char* author = "C++20 Astrophotography Team";
    static constexpr const char* license = "MIT";

    // Check if OpenCV support is available
    static bool hasOpenCVSupport() { return true; }

    // Check if CUDA support is available
    static bool hasCUDASupport() {
#ifdef SERASTRO_WITH_CUDA
        return true;
#else
        return false;
#endif
    }
};

}  // namespace serastro