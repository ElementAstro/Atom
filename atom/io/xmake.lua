-- filepath: d:\msys64\home\qwdma\Atom\atom\io\xmake.lua
-- xmake configuration for Atom-IO module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-io")
set_version("1.0.0")
set_license("GPL3")

-- Define dependencies
local atom_io_libs = {
    "loguru",
    "zlib",
    "asio"
}

-- Define source files
local atom_io_sources = {
    "compress.cpp",
    "file.cpp",
    "io.cpp",
    "pushd.cpp"  -- Add the implementation file for DirectoryStack
}

-- Define header files
local atom_io_headers = {
    "compress.hpp",
    "file.hpp",
    "glob.hpp",
    "io.hpp",
    "pushd.hpp"
}

-- Object Library
target("atom-io_object")
    set_kind("object")
    
    -- Add files
    add_files(table.unpack(atom_io_sources))
    add_headerfiles(table.unpack(atom_io_headers))
    
    -- Add dependencies
    add_packages(table.unpack(atom_io_libs))
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-io")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-io_object")
    
    -- Add packages
    add_packages(table.unpack(atom_io_libs))
    
    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows") then
        -- Add Windows-specific system libraries if needed
    end
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/io"))
    end)
target_end()
