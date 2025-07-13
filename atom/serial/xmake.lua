-- filepath: d:\msys64\home\qwdma\Atom\atom\serial\xmake.lua
-- xmake configuration for Atom-Serial module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-serial")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local source_files = {
    "bluetooth_serial.cpp",
    "scanner.cpp",
    "serial_port.cpp",
    "usb.cpp"
}

-- Define header files
local header_files = {
    "bluetooth_serial.hpp",
    "bluetooth_serial_mac.hpp",
    "bluetooth_serial_unix.hpp",
    "bluetooth_serial_win.hpp",
    "scanner.hpp",
    "serial_port.hpp",
    "serial_port_unix.hpp",
    "serial_port_win.hpp",
    "usb.hpp"
}

-- Object Library
target("atom-serial-object")
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
    if is_plat("windows") then
        add_defines("WIN32_LEAN_AND_MEAN")
        add_syslinks("setupapi")
    elseif is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("macosx") then
        add_frameworks("IOKit", "CoreFoundation")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-serial")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-serial-object")
    add_packages("loguru")

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("setupapi")
    elseif is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("macosx") then
        add_frameworks("IOKit", "CoreFoundation")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/serial"))
    end)
target_end()
