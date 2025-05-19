-- filepath: d:\msys64\home\qwdma\Atom\atom\error\xmake.lua
-- xmake configuration for Atom-Error module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-error")
set_version("1.0.0")
set_license("GPL3")

-- Define module dependencies
local atom_error_deps = {
    "atom-utils"
}

-- Define source files
local source_files = {
    "error_stack.cpp",
    "exception.cpp",
    "stacktrace.cpp"
}

-- Define header files
local header_files = {
    "error_code.hpp",
    "error_stack.hpp",
    "exception.hpp",
    "stacktrace.hpp"
}

-- Object Library
target("atom-error-object")
    set_kind("object")
    
    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))
    
    -- Add dependencies
    add_deps(table.unpack(atom_error_deps))
    add_packages("loguru")
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("dl")
    end
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-error")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-error-object")
    add_deps(table.unpack(atom_error_deps))
    add_packages("loguru")
    
    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("dl")
    end
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Set version with build timestamp
    set_version("1.0.0", {build = "%Y%m%d%H%M"})
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/error"))
    end)
target_end()
