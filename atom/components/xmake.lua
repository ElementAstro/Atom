-- filepath: d:\msys64\home\qwdma\Atom\atom\components\xmake.lua
-- xmake configuration for Atom-Component module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-component")
set_version("1.0.0")
set_license("GPL3")

-- Define module dependencies
local atom_component_deps = {
    "atom-error",
    "atom-type",
    "atom-utils"
}

-- Define package dependencies
local atom_component_packages = {
    "loguru"
}

-- Define source files
local source_files = {
    "registry.cpp"
}

-- Define header files
local header_files = {
    "component.hpp",
    "dispatch.hpp",
    "types.hpp",
    "var.hpp"
}

-- Object Library
target("atom-component-object")
    set_kind("object")
    
    -- Add files
    add_files(table.unpack(source_files))
    add_headerfiles(table.unpack(header_files))
    
    -- Add dependencies
    add_deps(table.unpack(atom_component_deps))
    add_packages(table.unpack(atom_component_packages))
    
    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})
    
    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-component")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")
    
    -- Add dependencies
    add_deps("atom-component-object")
    add_deps(table.unpack(atom_component_deps))
    add_packages(table.unpack(atom_component_packages))
    
    -- Platform-specific settings
    if is_plat("linux") then
        add_syslinks("pthread")
    end
    
    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")
    
    -- Set version with build timestamp
    set_version("1.0.0", {build = "%Y%m%d%H%M"})
    
    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/components"))
    end)
target_end()
