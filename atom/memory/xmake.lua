-- filepath: d:\msys64\home\qwdma\Atom\atom\memory\xmake.lua
-- xmake configuration for Atom-Memory module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-memory")
set_version("1.0.0")
set_license("GPL3")

-- Define header files
local header_files = {
    "memory.hpp",
    "memory_pool.hpp",
    "object.hpp",
    "ring.hpp",
    "shared.hpp",
    "short_alloc.hpp",
    "tracker.hpp",
    "utils.hpp"
}

-- Object Library - header-only library
target("atom-memory-object")
    set_kind("object")
    
    -- Add header files
    add_headerfiles(table.unpack(header_files))
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target (header-only)
target("atom-memory")
    -- Set library type to header-only
    set_kind("headeronly")
    
    -- Add dependencies
    add_deps("atom-memory-object")
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    
    -- Install configuration
    on_install(function (target)
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/memory"))
    end)
target_end()
