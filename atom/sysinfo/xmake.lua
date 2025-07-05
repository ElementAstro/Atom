-- filepath: d:\msys64\home\qwdma\Atom\atom\sysinfo\xmake.lua
-- xmake configuration for Atom-Sysinfo module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-sysinfo")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local source_files = {
    "battery.cpp",
    "cpu.cpp",
    "disk.cpp",
    "gpu.cpp",
    "memory.cpp",
    "os.cpp",
    "wifi.cpp"
}

-- Define header files
local header_files = {
    "battery.hpp",
    "cpu.hpp",
    "disk.hpp",
    "gpu.hpp",
    "memory.hpp",
    "os.hpp",
    "wifi.hpp"
}

-- Object Library
target("atom-sysinfo-object")
    set_kind("object")

    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows") then
        add_syslinks("pdh", "wlanapi")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-sysinfo")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-sysinfo-object")
    add_packages("loguru")

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows") then
        add_syslinks("pdh", "wlanapi")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Set version with build timestamp
    set_version("1.0.0", {build = "%Y%m%d%H%M"})

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/sysinfo"))
    end)
target_end()
