-- xmake script for Atom-IO
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom-IO
-- Description: IO Components for Element Astro Project
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-io")
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
add_requires("loguru", "minizip", "zlib", "tbb")

-- Define sources and headers from new structure
local sources = {
    -- Async operations
    "async/async_compress.cpp",
    "async/async_glob.cpp",
    "async/async_io.cpp",

    -- Compression functionality
    "compression/compress.cpp",

    -- Filesystem operations
    "filesystem/file_info.cpp",
    "filesystem/file_permission.cpp",
    "filesystem/pushd.cpp",

    -- Core I/O
    "core/io.cpp"
}

local headers = {
    -- Backwards compatibility headers
    "async_compress.hpp",
    "async_glob.hpp",
    "async_io.hpp",
    "compress.hpp",
    "file_info.hpp",
    "file_permission.hpp",
    "glob.hpp",
    "io.hpp",
    "pushd.hpp",

    -- Implementation headers
    "async/async_compress.hpp",
    "async/async_glob.hpp",
    "async/async_io.hpp",

    "compression/compress.hpp",

    "filesystem/file_info.hpp",
    "filesystem/file_permission.hpp",
    "filesystem/pushd.hpp",

    "core/glob.hpp",
    "core/io.hpp"
}

-- Main static library target
target("atom-io")
    -- Set target kind to static library
    set_kind("static")
    
    -- Add source files
    add_files(sources)
    
    -- Add header files
    add_headerfiles(headers)
    
    -- Add include directories
    add_includedirs(".", {public = true})
    
    -- Add packages
    add_packages("loguru", "minizip", "zlib", "tbb")
    
    -- Add system libraries
    add_syslinks("pthread")
    
    -- Windows-specific libraries
    if is_plat("windows") then
        add_syslinks("ws2_32", "wsock32")
    end
    
    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
    
    -- Set version info
    set_version("1.0.0")
    
    -- Set output name
    set_basename("atom-io")
    
    -- Set target and object directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Installation rules
    after_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        -- Install static library
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        -- Install headers
        local headerdir = path.join(installdir, "include", "atom", "io")
        os.mkdir(headerdir)
        -- Install compatibility headers
        os.cp("*.hpp", headerdir)
        -- Install implementation headers
        os.cp("async/*.hpp", path.join(headerdir, "async"))
        os.cp("compression/*.hpp", path.join(headerdir, "compression"))
        os.cp("filesystem/*.hpp", path.join(headerdir, "filesystem"))
        os.cp("core/*.hpp", path.join(headerdir, "core"))
    end)

-- Optional: Create object library target (equivalent to CMake's object library)
target("atom-io-object")
    set_kind("object")
    
    -- Add the same source files
    add_files(sources)
    add_headerfiles(headers)
    
    -- Configuration
    add_includedirs(".")
    add_packages("loguru", "minizip", "zlib", "tbb")
    add_syslinks("pthread")
    
    -- Windows-specific libraries
    if is_plat("windows") then
        add_syslinks("ws2_32", "wsock32")
    end
    
    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
