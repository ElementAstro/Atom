-- filepath: d:\msys64\home\qwdma\Atom\atom\utils\xmake.lua
-- xmake configuration for Atom-Utils module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes (including minsizerel for size optimization)
add_rules("mode.debug", "mode.release", "mode.minsizerel")

-- Set languages (match CMake C++20)
set_languages("c11", "cxx20")

-- Add required packages (use spdlog instead of loguru to match CMake)
local use_system_packages = has_config("use_system_packages")
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("fmt", {system = use_system_packages})
add_requires("tinyxml2", {system = use_system_packages})

-- Project configuration
set_project("atom-utils")
set_version("1.0.0")
set_license("GPL3")

-- Define source files from new structure
local sources = {
    -- Text processing
    "text/string.cpp",
    "text/utf.cpp",
    "text/valid_string.cpp",

    -- Time utilities
    "time/time.cpp",
    "time/stopwatcher.cpp",
    "time/qdatetime.cpp",
    "time/qtimer.cpp",
    "time/qtimezone.cpp",

    -- Process utilities
    "process/qprocess.cpp",

    -- Conversion utilities
    "conversion/convert.cpp",
    "conversion/to_any.cpp",

    -- Cryptographic utilities
    "crypto/aes.cpp",

    -- Random number generation
    "random/random.cpp",
    "random/uuid.cpp",
    "random/lcg.cpp",

    -- Debug utilities
    "debug/error_stack.cpp",
    "debug/print.cpp",

    -- Format utilities
    "format/xml.cpp",
    "format/difflib.cpp"
}

-- Define header files from new structure
local headers = {
    -- Backwards compatibility headers
    "*.hpp",  -- All root level compatibility headers

    -- Implementation headers
    "text/*.hpp",
    "time/*.hpp",
    "process/*.hpp",
    "conversion/*.hpp",
    "crypto/*.hpp",
    "random/*.hpp",
    "container/*.hpp",
    "memory/*.hpp",
    "debug/*.hpp",
    "format/*.hpp",
    "core/*.hpp"
}

-- Object Library
target("atom-utils-object")
    set_kind("object")

    -- Add source files from new structure
    for _, src in ipairs(sources) do
        add_files(src)
    end

    -- Add header files from new structure
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Add dependencies
    add_packages("spdlog", "fmt", "tinyxml2")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-utils")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-utils-object")
    add_packages("spdlog", "fmt", "tinyxml2")

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/utils"))
    end)
target_end()
