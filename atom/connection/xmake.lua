-- filepath: d:\msys64\home\qwdma\Atom\atom\connection\xmake.lua
-- xmake configuration for Atom-Connection module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-connection")
set_version("1.0.0")
set_description("Connection Between Lithium Drivers, TCP and IPC")
set_license("GPL3")

-- Build options - making these options consistent with parent options
option("enable_ssh")
    set_default(false)
    set_showmenu(true)
    set_description("Enable SSH support")
option_end()

option("enable_libssh")
    set_default(false)
    set_showmenu(true)
    set_description("Enable LibSSH support")
option_end()

-- Object Library
target("atom-connection-object")
    set_kind("object")
    
    -- Add base files
    add_files("*.cpp|sshclient.cpp|_pybind.cpp")  -- Exclude conditional files
    add_headerfiles("*.hpp|sshclient.hpp")         -- Exclude conditional headers
    
    -- Add conditional files
    if has_config("enable_ssh") or has_config("enable_libssh") then
        add_files("sshclient.cpp")
        add_headerfiles("sshclient.hpp")
        add_packages("libssh")
    end
    
    -- Add dependencies
    add_packages("loguru")
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-connection")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-connection-object")
    add_packages("loguru")
    
    -- Python support
    if has_config("build_python") then
        add_rules("python.pybind11_module")
        add_files("_pybind.cpp")
        add_packages("python", "pybind11")
    end
    
    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/connection"))
    end)
target_end()

-- Remove old targets that are now handled by parent xmake.lua
-- The install, build, and clean targets should be managed by the parent project
