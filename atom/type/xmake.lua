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
    "rjson.cpp",
    "ryaml.cpp"
}

-- Define header files
local headers = {
    "args.hpp",
    "argsview.hpp",
    "auto_table.hpp",
    "compat.hpp",
    "concurrent_map.hpp",
    "concurrent_set.hpp",
    "concurrent_vector.hpp",
    "cstream.hpp",
    "expected.hpp",
    "flatmap.hpp",
    "flatset.hpp",
    "indestructible.hpp",
    "iter.hpp",
    "json-schema.hpp",
    "json.hpp",
    "json_fwd.hpp",
    "no_offset_ptr.hpp",
    "noncopyable.hpp",
    "optional.hpp",
    "pod_vector.hpp",
    "pointer.hpp",
    "qvariant.hpp",
    "rjson.hpp",
    "robin_hood.hpp",
    "rtype.hpp",
    "ryaml.hpp",
    "small_list.hpp",
    "small_vector.hpp",
    "static_string.hpp",
    "static_vector.hpp",
    "string.hpp",
    "trackable.hpp",
    "uint.hpp",
    "weak_ptr.hpp"
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

    -- Add dependencies
    add_deps("atom-error", "atom-meta")

    -- Set C++ standard
    set_languages("c++20")
target_end()

-- Library target
target("atom-type")
    -- Set library type based on parent project option
    set_kind(has_config("shared_libs") and "shared" or "static")

    -- Add dependencies
    add_deps("atom-type-object", "atom-error", "atom-meta")

    -- Set include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set output directories
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Install configuration
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/type"))
    end)
target_end()
