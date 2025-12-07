-- xmake.lua for atom-error module
--
-- Copyright (C) 2023-2024 Max Qian <lightapt.com>
--
-- Refactored modular structure with subdirectories

-- Set project info
set_project("atom-error")
set_version("1.0.0")
set_xmakever("2.8.0")

-- Set languages
set_languages("c++20")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
add_requires("loguru")

-- Define sources by module
local core_sources = {
    "core/error_metadata.cpp"
}

local stacktrace_sources = {
    "stacktrace/stack_frame.cpp",
    "stacktrace/stacktrace_utils.cpp",
    "stacktrace/builtin_backend.cpp",
    "stacktrace/external_backends.cpp",
    "stacktrace/backend_factory.cpp",
    "stacktrace/stacktrace.cpp"
}

local exception_sources = {
    "exception/exception_base.cpp"
}

local context_sources = {
    "context/error_context.cpp",
    "context/context_manager.cpp",
    "context/scoped_context.cpp"
}

local handler_sources = {
    "handler/error_reporter.cpp",
    "handler/error_aggregator.cpp",
    "handler/global_handler.cpp"
}

-- Aggregate all sources
local sources = {}
for _, src in ipairs(core_sources) do table.insert(sources, src) end
for _, src in ipairs(stacktrace_sources) do table.insert(sources, src) end
for _, src in ipairs(exception_sources) do table.insert(sources, src) end
for _, src in ipairs(context_sources) do table.insert(sources, src) end
for _, src in ipairs(handler_sources) do table.insert(sources, src) end

-- Define headers by module
local core_headers = {
    "core/error_types.hpp",
    "core/error_codes.hpp",
    "core/error_metadata.hpp"
}

local stacktrace_headers = {
    "stacktrace/stack_frame.hpp",
    "stacktrace/stacktrace_utils.hpp",
    "stacktrace/backend_interface.hpp",
    "stacktrace/builtin_backend.hpp",
    "stacktrace/external_backends.hpp",
    "stacktrace/stacktrace.hpp"
}

local exception_headers = {
    "exception/exception_base.hpp",
    "exception/common_exceptions.hpp",
    "exception/object_exceptions.hpp",
    "exception/argument_exceptions.hpp",
    "exception/file_exceptions.hpp",
    "exception/system_exceptions.hpp"
}

local context_headers = {
    "context/error_context.hpp",
    "context/context_manager.hpp",
    "context/scoped_context.hpp"
}

local handler_headers = {
    "handler/error_reporter.hpp",
    "handler/error_aggregator.hpp",
    "handler/global_handler.hpp"
}

-- Aggregate all headers
local headers = {"error.hpp"}
for _, hdr in ipairs(core_headers) do table.insert(headers, hdr) end
for _, hdr in ipairs(stacktrace_headers) do table.insert(headers, hdr) end
for _, hdr in ipairs(exception_headers) do table.insert(headers, hdr) end
for _, hdr in ipairs(context_headers) do table.insert(headers, hdr) end
for _, hdr in ipairs(handler_headers) do table.insert(headers, hdr) end

-- Main shared library target
target("atom-error")
    set_kind("shared")
    add_files(sources)
    add_headerfiles(headers)
    add_packages("loguru")
    add_includedirs(".", "..", {public = true})

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("dbghelp", "psapi")
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
    elseif is_plat("linux") then
        add_syslinks("dl")
    end

    -- ========================================================================
    -- Optional external stacktrace libraries
    -- ========================================================================

    -- High-quality external backends
    if has_config("use_cpptrace") then
        add_defines("ATOM_USE_CPPTRACE")
        add_packages("cpptrace")
    end

    if has_config("use_backward") then
        add_defines("ATOM_USE_BACKWARD_CPP")
        add_packages("backward-cpp")
    end

    if has_config("use_boost_stacktrace") then
        add_defines("ATOM_USE_BOOST_STACKTRACE")
        add_packages("boost")
    end

    -- System-level backends
    if has_config("use_libunwind") then
        add_defines("ATOM_USE_LIBUNWIND")
        if is_plat("linux", "macosx", "bsd") then
            add_syslinks("unwind")
        end
    end

    if has_config("use_execinfo") then
        add_defines("ATOM_USE_EXECINFO")
        if is_plat("bsd") then
            add_syslinks("execinfo")
        end
    end

    if has_config("use_libbacktrace") then
        add_defines("ATOM_USE_LIBBACKTRACE")
        add_syslinks("backtrace")
    end

    if has_config("use_std_stacktrace") then
        add_defines("ATOM_USE_STD_STACKTRACE")
        -- GCC needs stdc++_libbacktrace for std::stacktrace
        if is_plat("linux") and is_kind("shared") then
            add_syslinks("stdc++_libbacktrace")
        end
    end

    if has_config("use_abseil") then
        add_defines("ATOM_USE_ABSEIL_STACKTRACE")
        add_packages("abseil")
    end

    -- Installation rules
    on_install(function(target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))

        -- Install headers preserving structure
        os.cp("error.hpp", path.join(target:installdir(), "include/atom/error"))
        os.cp("core/*.hpp", path.join(target:installdir(), "include/atom/error/core"))
        os.cp("stacktrace/*.hpp", path.join(target:installdir(), "include/atom/error/stacktrace"))
        os.cp("exception/*.hpp", path.join(target:installdir(), "include/atom/error/exception"))
        os.cp("context/*.hpp", path.join(target:installdir(), "include/atom/error/context"))
        os.cp("handler/*.hpp", path.join(target:installdir(), "include/atom/error/handler"))
    end)
target_end()

-- Configuration options
option("use_cpptrace")
    set_default(false)
    set_showmenu(true)
    set_description("Use cpptrace for stack traces")
option_end()

option("use_backward")
    set_default(false)
    set_showmenu(true)
    set_description("Use backward-cpp for stack traces")
option_end()

option("use_boost_stacktrace")
    set_default(false)
    set_showmenu(true)
    set_description("Use Boost.Stacktrace for stack traces")
option_end()

-- System-level backend options
option("use_libunwind")
    set_default(false)
    set_showmenu(true)
    set_description("Use libunwind for stack traces")
option_end()

option("use_execinfo")
    set_default(false)
    set_showmenu(true)
    set_description("Use execinfo/backtrace for stack traces (POSIX)")
option_end()

option("use_libbacktrace")
    set_default(false)
    set_showmenu(true)
    set_description("Use libbacktrace for stack traces (GCC)")
option_end()

option("use_std_stacktrace")
    set_default(false)
    set_showmenu(true)
    set_description("Use std::stacktrace (C++23)")
option_end()

option("use_abseil")
    set_default(false)
    set_showmenu(true)
    set_description("Use Abseil for stack traces")
option_end()
