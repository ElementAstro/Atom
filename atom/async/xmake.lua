-- filepath: d:\msys64\home\qwdma\Atom\atom\async\xmake.lua
-- xmake configuration for Atom-Async module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-async")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local sources = {
    "lock.cpp", 
    "timer.cpp"
}

-- Define header files
local headers = {
    "*.hpp", 
    "*.inl"
}

-- Object Library
target("atom-async-object")
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
target_end()

-- Library target
target("atom-async")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-async-object")
    add_packages("loguru")
    
    -- Add include directories
    add_includedirs(".", {public = true})
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/async"))
        os.cp("*.inl", path.join(target:installdir(), "include/atom/async"))
    end)
target_end()
