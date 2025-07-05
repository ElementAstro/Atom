-- filepath: d:\msys64\home\qwdma\Atom\atom\type\xmake.lua
-- xmake configuration for Atom-Type module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-type")
set_version("1.0.0")
set_license("GPL3")

-- Define source files
local sources = {
    "args.cpp",
    "ini.cpp",
    "message.cpp"
}

-- Define header files
local headers = {
    "abi.hpp",
    "args.hpp",
    "enum_flag.hpp",
    "enum_flag.inl",
    "flatset.hpp",
    "ini_impl.hpp",
    "ini.hpp",
    "json.hpp",
    "message.hpp",
    "pointer.hpp",
    "small_vector.hpp"
}

-- Object Library
target("atom-type-object")
    set_kind("object")

    -- Add files
    add_headerfiles(table.unpack(headers))
    add_files(table.unpack(sources))

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-type")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-type-object", "atom-utils")

    -- Set include directories
    add_includedirs(".", {public = true})

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/type"))
        os.cp("*.inl", path.join(target:installdir(), "include/atom/type"))
    end)
target_end()

    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    after_build(function (target)
        os.cp("$(buildir)/lib", "$(projectdir)/lib")
        os.cp("$(projectdir)/*.hpp", "$(projectdir)/include")
    end)
