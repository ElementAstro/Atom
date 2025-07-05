-- filepath: d:\msys64\home\qwdma\Atom\atom\log\xmake.lua
-- xmake configuration for Atom-Log module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-log")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local sources = {
    "logger.cpp",
    "syslog.cpp"
}

-- Define header files
local headers = {
    "logger.hpp",
    "syslog.hpp"
}

-- Object Library
target("atom-log-object")
    set_kind("object")

    -- Add files
    add_files(table.unpack(sources))
    add_headerfiles(table.unpack(headers))

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")

    -- Configure loguru options
    if is_plat("windows") then
        add_defines("LOGURU_STACKTRACES=1", {public = true})
    else
        add_defines("LOGURU_STACKTRACES=1", {public = true})
    end

    add_defines("LOGURU_WITH_STREAMS=1", {public = true})
    add_defines("LOGURU_RTTI=1", {public = true})
target_end()

-- Library target
target("atom-log")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-log-object")
    add_packages("loguru")

    -- Platform-specific settings
    if is_plat("windows") then
        add_packages("dlfcn-win32")
        add_syslinks("dbghelp")
    else
        add_syslinks("dl", "pthread")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/log"))
    end)
target_end()
