-- filepath: d:\msys64\home\qwdma\Atom\atom\algorithm\xmake.lua
-- xmake configuration for Atom-Algorithm module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-algorithm")
set_version("1.0.0")
set_license("GPL3")

-- Define source files (adjust these based on your actual files)
local sources = {}  -- Add algorithm source files here

-- Define header files (adjust these based on your actual files)
local headers = {
    "*.hpp"  -- This will include all hpp files in the directory
}

-- Object Library
target("atom-algorithm_object")
    set_kind("object")
    
    -- Add files
    if #sources > 0 then
        add_files(table.unpack(sources))
    end
    add_headerfiles(table.unpack(headers))
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-algorithm")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-algorithm_object")
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/algorithm"))
    end)
target_end()

-- Remove previous package definition which seems to be unused
-- Uncomment if you actually need this package
--[[
package("foo")
    add_deps("cmake")
    set_sourcedir(path.join(os.scriptdir(), "foo"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs)
    end)
    on_test(function (package)
        assert(package:has_cfuncs("add", {includes = "foo.h"}))
    end)
package_end()
]]
