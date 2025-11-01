-- xmake configuration for Boost utility headers
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-boost")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Add required packages
add_requires("boost", {optional = true})

-- Header files
local headers = {
    "charconv.hpp",
    "locale.hpp",
    "math.hpp",
    "regex.hpp",
    "system.hpp",
    "uuid.hpp"
}

target("atom-extra-boost")
    set_kind("headeronly")

    -- Add headers
    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("boost")

    -- Set C++ standard
    set_languages("c++20")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        local headerdir = path.join(installdir, "include", "atom", "extra", "boost")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
