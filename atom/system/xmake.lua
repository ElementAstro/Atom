-- filepath: d:\msys64\home\qwdma\Atom\atom\system\xmake.lua
-- xmake configuration for Atom-System module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes (including minsizerel for size optimization)
add_rules("mode.debug", "mode.release", "mode.minsizerel")

-- Set languages (match CMake C++20)
set_languages("c11", "cxx20")

-- Add required packages (use spdlog instead of loguru to match CMake)
local use_system_packages = has_config("use_system_packages")
add_requires("spdlog", {system = use_system_packages, configs = {fmt_external = true}})
add_requires("fmt", {system = use_system_packages})

-- Project configuration
set_project("atom-system")
set_version("1.0.0")
set_description("A collection of useful system functions")
set_license("GPL3")

-- Object Library
target("atom-system-object")
    set_kind("object")

    -- Add source files from new structure
    add_files("core/*.cpp")
    add_files("process/*.cpp")
    add_files("hardware/*.cpp")
    add_files("power/*.cpp")
    add_files("info/*.cpp")
    add_files("registry/*.cpp")
    add_files("network/*.cpp")
    add_files("storage/*.cpp")
    add_files("clipboard/*.cpp")
    add_files("signals/*.cpp")
    add_files("debug/*.cpp")
    add_files("scheduling/*.cpp")

    -- Add header files from new structure
    add_headerfiles("*.hpp")  -- Backwards compatibility headers
    add_headerfiles("core/*.hpp")
    add_headerfiles("process/*.hpp")
    add_headerfiles("hardware/*.hpp")
    add_headerfiles("power/*.hpp")
    add_headerfiles("info/*.hpp")
    add_headerfiles("registry/*.hpp")
    add_headerfiles("network/*.hpp")
    add_headerfiles("storage/*.hpp")
    add_headerfiles("clipboard/*.hpp")
    add_headerfiles("clipboard/*.ipp")
    add_headerfiles("signals/*.hpp")
    add_headerfiles("debug/*.hpp")
    add_headerfiles("scheduling/*.hpp")

    -- Add dependencies
    add_packages("spdlog", "fmt")

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows", "mingw") then
        add_syslinks("pdh", "wlanapi", "userenv", "version", "advapi32", "hid", "setupapi")
    end

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-system")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-system-object")
    add_packages("spdlog", "fmt")

    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows", "mingw") then
        add_syslinks("pdh", "wlanapi", "userenv", "version", "advapi32", "hid", "setupapi")
    end

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        -- Install backwards compatibility headers
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/system"))
        -- Install actual implementation headers
        os.cp("core/*.hpp", path.join(target:installdir(), "include/atom/system/core"))
        os.cp("process/*.hpp", path.join(target:installdir(), "include/atom/system/process"))
        os.cp("hardware/*.hpp", path.join(target:installdir(), "include/atom/system/hardware"))
        os.cp("power/*.hpp", path.join(target:installdir(), "include/atom/system/power"))
        os.cp("info/*.hpp", path.join(target:installdir(), "include/atom/system/info"))
        os.cp("registry/*.hpp", path.join(target:installdir(), "include/atom/system/registry"))
        os.cp("network/*.hpp", path.join(target:installdir(), "include/atom/system/network"))
        os.cp("storage/*.hpp", path.join(target:installdir(), "include/atom/system/storage"))
        os.cp("clipboard/*.hpp", path.join(target:installdir(), "include/atom/system/clipboard"))
        os.cp("clipboard/*.ipp", path.join(target:installdir(), "include/atom/system/clipboard"))
        os.cp("signals/*.hpp", path.join(target:installdir(), "include/atom/system/signals"))
        os.cp("debug/*.hpp", path.join(target:installdir(), "include/atom/system/debug"))
        os.cp("scheduling/*.hpp", path.join(target:installdir(), "include/atom/system/scheduling"))
        -- Keep shortcut subdirectory as is
        os.cp("shortcut/*.hpp", path.join(target:installdir(), "include/atom/system/shortcut"))
        os.cp("shortcut/*.h", path.join(target:installdir(), "include/atom/system/shortcut"))
    end)
target_end()
