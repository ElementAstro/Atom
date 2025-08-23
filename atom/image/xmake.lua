-- filepath: d:\msys64\home\qwdma\Atom\atom\image\xmake.lua
-- xmake configuration for Atom-Image module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-image")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local source_files = {
    -- Metadata sources
    "metadata/exif.cpp",

    -- Format sources
    "formats/fits_data.cpp",
    "formats/fits_file.cpp",
    "formats/fits_header.cpp",
    "formats/fits_utils.cpp",
    "formats/hdu.cpp",

    -- Processing sources
    "processing/image_processor.cpp"
}

-- Define header files
local header_files = {
    -- Main header
    "image.hpp",

    -- Core headers
    "core/image_blob.hpp",

    -- Metadata headers
    "metadata/exif.hpp",

    -- Format headers
    "formats/fits_data.hpp",
    "formats/fits_file.hpp",
    "formats/fits_header.hpp",
    "formats/fits_utils.hpp",
    "formats/hdu.hpp",

    -- Processing headers
    "processing/image_processor.hpp"
}

-- Add required packages
add_requires("cfitsio", {optional = true})

-- Object Library
target("atom-image-object")
    set_kind("object")
    
    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))
    
    -- Add dependencies
    add_packages("loguru")
    
    -- Add optional dependency on cfitsio if available
    if has_package("cfitsio") then
        add_packages("cfitsio")
        add_defines("HAS_CFITSIO")
    end
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-image")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-image-object")
    add_packages("loguru")
    
    -- Add optional dependency on cfitsio if available
    if has_package("cfitsio") then
        add_packages("cfitsio")
    end
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/image"))
    end)
target_end()
