-- xmake configuration for Atom-Containers module
-- Author: Max Qian
-- License: GPL3

-- Add standard build modes
add_rules("mode.debug", "mode.release")

-- Project configuration
set_project("atom-containers")
set_version("1.0.0")
set_license("GPL3")

-- Set C++ standard
set_languages("c++20")

-- Define header files
local headers = {
    "boost_containers.hpp",
    "graph.hpp",
    "high_performance.hpp",
    "intrusive.hpp",
    "lockfree.hpp"
}

-- Header-only library target
target("atom-containers")
    -- Set as header-only library
    set_kind("headeronly")

    -- Add header files
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Add include directories
    add_includedirs(".", {public = true})
    add_includedirs("..", {public = true})

    -- Set C++ standard
    set_languages("c++20")

    -- Installation rules
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local headerdir = path.join(installdir, "include", "atom", "containers")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
