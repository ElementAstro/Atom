-- filepath: d:\msys64\home\qwdma\Atom\atom\secret\xmake.lua
-- xmake configuration for Atom-Secret module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-secret")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local source_files = {
    "encryption.cpp",
    "storage.cpp"
}

-- Define header files
local header_files = {
    "common.hpp",
    "encryption.hpp",
    "password_entry.hpp",
    "result.hpp",
    "storage.hpp"
}

-- Object Library
target("atom-secret-object")
    set_kind("object")

    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))

    -- Add dependencies
    add_packages("loguru")
    add_deps("atom-utils")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("crypt32", "advapi32")
    elseif is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("macosx") then
        add_frameworks("Security")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-secret")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-secret-object", "atom-utils")
    add_packages("loguru")

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("crypt32", "advapi32")
    elseif is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("macosx") then
        add_frameworks("Security")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/secret"))
    end)
target_end()
