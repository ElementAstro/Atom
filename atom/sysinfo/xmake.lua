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

-- Define compatibility header files (all implementation is in src/ subdirectories)
local header_files = {
    "battery.hpp",
    "bios.hpp",
    "cpu.hpp",
    "disk.hpp",
    "gpu.hpp",
    "locale.hpp",
    "memory.hpp",
    "os.hpp",
    "sn.hpp",
    "virtual.hpp",
    "wifi.hpp",
    "wm.hpp",
    "sysinfo_printer.hpp"
}

-- No source files at root level - everything is in src/ subdirectories
local source_files = {}

-- Add subdirectory targets for all modules
includes("src/*/xmake.lua")

-- Main library target (interface library that depends on all modules)
target("atom-sysinfo")
    set_kind("headeronly")

    -- Add compatibility headers
    add_headerfiles(table.unpack(header_files))

    -- Add dependencies on all module libraries
    add_deps(
        "atom-sysinfo-battery",
        "atom-sysinfo-bios",
        "atom-sysinfo-cpu",
        "atom-sysinfo-disk",
        "atom-sysinfo-gpu",
        "atom-sysinfo-locale",
        "atom-sysinfo-memory",
        "atom-sysinfo-os",
        "atom-sysinfo-serial",
        "atom-sysinfo-virtual",
        "atom-sysinfo-wifi",
        "atom-sysinfo-wm",
        "atom-sysinfo-printer"
    )

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set version
    set_version("1.0.0", {build = "%Y%m%d%H%M"})

    -- Install configuration
    on_install(function (target)
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/sysinfo"))
    end)
target_end()
