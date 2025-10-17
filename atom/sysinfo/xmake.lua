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

-- Define source files by category
local hardware_sources = {
    "hardware/battery.cpp",
    "hardware/bios.cpp",
    "hardware/gpu.cpp",
    "hardware/cpu/common.cpp",
    "hardware/cpu/windows.cpp",
    "hardware/cpu/linux.cpp",
    "hardware/cpu/macos.cpp",
    "hardware/cpu/freebsd.cpp"
}

local storage_sources = {
    "storage/disk/disk_device.cpp",
    "storage/disk/disk_info.cpp",
    "storage/disk/disk_monitor.cpp",
    "storage/disk/disk_security.cpp",
    "storage/disk/disk_util.cpp"
}

local info_sources = {
    "info/locale.cpp",
    "info/os.cpp",
    "info/wm.cpp",
    "info/sn.cpp",
    "info/virtual.cpp"
}

local utils_sources = {
    "utils/sysinfo_printer.cpp"
}

-- Combine all source files
local source_files = {}
for _, file in ipairs(hardware_sources) do
    table.insert(source_files, file)
end
for _, file in ipairs(storage_sources) do
    table.insert(source_files, file)
end
for _, file in ipairs(info_sources) do
    table.insert(source_files, file)
end
for _, file in ipairs(utils_sources) do
    table.insert(source_files, file)
end

-- Define header files by category
local hardware_headers = {
    "hardware/battery.hpp",
    "hardware/bios.hpp",
    "hardware/cpu.hpp",
    "hardware/gpu.hpp",
    "hardware/memory.hpp"
}

local storage_headers = {
    "storage/disk.hpp"
}

local network_headers = {
    "network/wifi.hpp"
}

local info_headers = {
    "info/locale.hpp",
    "info/os.hpp",
    "info/wm.hpp",
    "info/sn.hpp",
    "info/virtual.hpp"
}

local utils_headers = {
    "utils/sysinfo_printer.hpp"
}

-- Combine all header files
local header_files = {}
for _, file in ipairs(hardware_headers) do
    table.insert(header_files, file)
end
for _, file in ipairs(storage_headers) do
    table.insert(header_files, file)
end
for _, file in ipairs(network_headers) do
    table.insert(header_files, file)
end
for _, file in ipairs(info_headers) do
    table.insert(header_files, file)
end
for _, file in ipairs(utils_headers) do
    table.insert(header_files, file)
end

-- Define compatibility header files
local compat_header_files = {
    "battery.hpp",
    "bios.hpp",
    "cpu.hpp",
    "disk.hpp",
    "gpu.hpp",
    "locale.hpp",
    "memory.hpp",
    "os.hpp",
    "wifi.hpp",
    "wm.hpp",
    "sn.hpp",
    "virtual.hpp",
    "sysinfo_printer.hpp"
}

-- Object Library
target("atom-sysinfo-object")
    set_kind("object")

    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))
    add_headerfiles(table.unpack(compat_header_files))

    -- Add dependencies
    add_packages("loguru")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    add_includedirs("hardware", {public = true})
    add_includedirs("storage", {public = true})
    add_includedirs("network", {public = true})
    add_includedirs("info", {public = true})
    add_includedirs("utils", {public = true})

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
        os.cp("hardware/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/hardware"))
        os.cp("storage/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/storage"))
        os.cp("network/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/network"))
        os.cp("info/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/info"))
        os.cp("utils/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/utils"))
    end)
target_end()
