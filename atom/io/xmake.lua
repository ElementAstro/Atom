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

-- Set languages (match CMake C++20)
set_languages("c11", "cxx20")

-- Add build modes (including minsizerel for size optimization)
add_rules("mode.debug", "mode.release", "mode.minsizerel")

-- =============================================================================
-- Configuration Options
-- =============================================================================

option("use_asio")
    set_default(false)
    set_showmenu(true)
    set_description("Enable ASIO for async I/O features")
option_end()

option("use_minizip")
    set_default(false)
    set_showmenu(true)
    set_description("Enable minizip-ng for zip compression features")
option_end()

option("use_tbb")
    set_default(false)
    set_showmenu(true)
    set_description("Enable TBB for parallel algorithms")
option_end()

-- =============================================================================
-- Package Dependencies
-- =============================================================================

local use_system_packages = has_config("use_system_packages")
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("fmt", {system = use_system_packages})
add_requires("zlib", {system = use_system_packages, optional = true})

if has_config("use_minizip") then
    add_requires("minizip-ng", {system = use_system_packages, optional = true})
end
if has_config("use_tbb") then
    add_requires("tbb", {system = use_system_packages, optional = true})
end
if has_config("use_asio") then
    add_requires("asio", {system = use_system_packages, optional = true})
end

-- =============================================================================
-- Source and Header Definitions
-- =============================================================================

-- Core sources (always built)
local sources = {
    -- Compression functionality (split into sub-modules)
    "compression/gz_compress.cpp",
    "compression/zip_operations.cpp",
    "compression/slice_compress.cpp",
    "compression/data_compress.cpp",
    "compression/backup.cpp",

    -- Filesystem operations
    "filesystem/file_info.cpp",
    "filesystem/file_ops.cpp",
    "filesystem/file_permission.cpp",
    "filesystem/file_permission_change.cpp",
    "filesystem/directory_stack.cpp",
    "filesystem/directory_stack_navigation.cpp",
    "filesystem/directory_stack_persistence.cpp",

    -- Core I/O
    "core/io.cpp"
}

-- Async sources (conditional on ASIO)
local async_sources = {
    "async/async_file.cpp",
    "async/async_directory.cpp",
    "async/async_batch.cpp",
    "async/async_stream.cpp",
    "async/async_simd.cpp",
    "async/async_compressor.cpp",
    "async/async_decompressor.cpp",
    "async/async_zip.cpp",
    "async/async_glob.cpp"
}

-- All headers
local headers = {
    -- Barrel export header
    "index.hpp",

    -- Async headers
    "async/async_types.hpp",
    "async/async_file.hpp",
    "async/async_directory.hpp",
    "async/async_batch.hpp",
    "async/async_stream.hpp",
    "async/async_simd.hpp",
    "async/async_compressor.hpp",
    "async/async_decompressor.hpp",
    "async/async_zip.hpp",
    "async/async_compress.hpp",
    "async/async_glob.hpp",
    "async/async_io.hpp",

    -- Compression headers
    "compression/compress.hpp",
    "compression/types.hpp",
    "compression/utils.hpp",
    "compression/gz_compress.hpp",
    "compression/zip_operations.hpp",
    "compression/slice_compress.hpp",
    "compression/data_compress.hpp",
    "compression/backup.hpp",

    -- Filesystem headers
    "filesystem/file_info.hpp",
    "filesystem/file_ops.hpp",
    "filesystem/file_permission.hpp",
    "filesystem/file_permission_change.hpp",
    "filesystem/task.hpp",
    "filesystem/directory_stack.hpp",
    "filesystem/directory_stack_impl.hpp",
    "filesystem/pushd.hpp",

    -- Core headers
    "core/glob.hpp",
    "core/io.hpp",
    "core/types.hpp",
    "core/path_convert.hpp",
    "core/file_query.hpp",
    "core/file_ops.hpp",
    "core/directory_ops.hpp",
    "core/directory_walk.hpp",
    "core/file_split_merge.hpp",
    "core/path_utils.hpp"
}

-- =============================================================================
-- Main Static Library Target
-- =============================================================================

target("atom-io")
    set_kind("static")

    -- Add core source files
    add_files(sources)

    -- Add async sources if ASIO is enabled
    if has_config("use_asio") then
        add_files(async_sources)
    end

    -- Add header files
    add_headerfiles(headers)

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add required packages
    add_packages("spdlog", "fmt")
    add_packages("zlib")

    if has_config("use_minizip") then
        add_packages("minizip-ng")
    else
        add_defines("ATOM_IO_NO_MINIZIP")
    end
    if has_config("use_tbb") then
        add_packages("tbb")
    end
    if has_config("use_asio") then
        add_packages("asio")
        add_defines("ASIO_STANDALONE", "ATOM_USE_ASIO", {public = true})
    end

    -- Add module dependency
    add_deps("atom-async")

    -- Add system libraries
    if is_plat("linux") then
        add_syslinks("pthread")
    end

    -- Windows-specific libraries
    if is_plat("windows", "mingw") then
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
        -- Install headers preserving subdirectory structure
        local headerdir = path.join(installdir, "include", "atom", "io")
        os.mkdir(headerdir)
        os.cp("index.hpp", headerdir)
        os.cp("async/*.hpp", path.join(headerdir, "async"))
        os.cp("compression/*.hpp", path.join(headerdir, "compression"))
        os.cp("filesystem/*.hpp", path.join(headerdir, "filesystem"))
        os.cp("core/*.hpp", path.join(headerdir, "core"))
    end)
target_end()

-- =============================================================================
-- Object Library Target (equivalent to CMake's object library)
-- =============================================================================

target("atom-io-object")
    set_kind("object")

    -- Add core source files
    add_files(sources)

    -- Add async sources if ASIO is enabled
    if has_config("use_asio") then
        add_files(async_sources)
    end

    add_headerfiles(headers)

    -- Configuration
    add_includedirs(".")
    add_packages("spdlog", "fmt")
    add_packages("zlib")

    if has_config("use_minizip") then
        add_packages("minizip-ng")
    else
        add_defines("ATOM_IO_NO_MINIZIP")
    end
    if has_config("use_tbb") then
        add_packages("tbb")
    end
    if has_config("use_asio") then
        add_packages("asio")
        add_defines("ASIO_STANDALONE", "ATOM_USE_ASIO")
    end

    add_deps("atom-async")

    if is_plat("linux") then
        add_syslinks("pthread")
    end

    -- Windows-specific libraries
    if is_plat("windows", "mingw") then
        add_syslinks("ws2_32", "wsock32")
    end

    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
target_end()
