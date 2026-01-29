-- xmake configuration for Modern XML library (pugixml wrapper)
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-pugixml")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Add required packages
local use_system_packages = has_config("use_system_packages")
add_requires("pugixml", {optional = true, system = use_system_packages})

-- Header files
local headers = {
    "modern_xml.hpp",
    "xml_builder.hpp",
    "xml_document.hpp",
    "xml_node_wrapper.hpp",
    "xml_query.hpp"
}

target("atom-extra-pugixml")
    set_kind("headeronly")

    -- Add headers
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("pugixml")

    -- Set C++ standard
    set_languages("c++20")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local headerdir = path.join(installdir, "include", "atom", "extra", "pugixml")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
