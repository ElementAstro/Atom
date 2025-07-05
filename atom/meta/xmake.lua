-- filepath: d:\msys64\home\qwdma\Atom\atom\meta\xmake.lua
-- xmake configuration for Atom-Meta module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-meta")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local source_files = {
    "global_ptr.cpp"
}

-- Define header files
local header_files = {
    "global_ptr.hpp"
}

-- Object Library
target("atom-meta-object")
    set_kind("object")

    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-meta")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-meta-object")

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Set version with build timestamp
    set_version("1.0.0", {build = "%Y%m%d%H%M"})

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/meta"))
    end)
target_end()
