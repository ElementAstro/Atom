-- filepath: d:\msys64\home\qwdma\Atom\atom\system\xmake.lua
-- xmake configuration for Atom-System module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-system")
set_version("1.0.0")
set_description("A collection of useful system functions")
set_license("GPL3")

-- Object Library
target("atom-system-object")
    set_kind("object")
    
    -- Add source files
    add_files("*.cpp")
    add_files("module/*.cpp")
    
    -- Add header files
    add_headerfiles("*.hpp")
    add_headerfiles("module/*.hpp")
    
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
target("atom-system")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-system-object")
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
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/system"))
        os.cp("module/*.hpp", path.join(target:installdir(), "include/atom/system/module"))
    end)
target_end()
