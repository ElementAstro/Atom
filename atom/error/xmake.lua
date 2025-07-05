-- xmake script for Atom-Error
-- This project is licensed under the terms of the GPL3 license.
--
-- Project Name: Atom-Error
-- Description: Atom Error Library
-- Author: Max Qian
-- License: GPL3

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-error")
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_license("GPL-3.0")

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Add required packages
add_requires("loguru")

-- Define sources and headers
local sources = {
    "exception.cpp",
    "stacktrace.cpp"
}

local headers = {
    "error_code.hpp",
    "stacktrace.hpp"
}

-- Main shared library target
target("atom-error")
    -- Set target kind to shared library
    set_kind("shared")

    -- Add source files
    add_files(sources)

    -- Add header files
    add_headerfiles(headers)

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("loguru")

    -- Add platform-specific libraries
    if is_plat("linux") then
        add_syslinks("dl")
    end

    -- Enable position independent code (automatic for shared libraries)
    set_policy("build.optimization.lto", true)

    -- Set version info
    set_version("1.0.0")

    -- Set output name
    set_basename("atom-error")

    -- Set target and object directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Installation rules
    after_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        -- Install shared library
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        -- Install headers
        local headerdir = path.join(installdir, "include", "atom-error")
        os.mkdir(headerdir)
        for _, header in ipairs(headers) do
            os.cp(header, headerdir)
        end
    end)

-- Optional: Create object library target (equivalent to CMake's object library)
target("atom-error-object")
    set_kind("object")

    -- Add the same source files
    add_files(sources)
    add_headerfiles(headers)

    -- Configuration
    add_includedirs(".")
    add_packages("loguru")

    -- Platform-specific libraries
    if is_plat("linux") then
        add_syslinks("dl")
    end

    -- Enable position independent code
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})
