-- xmake configuration for Beast HTTP/WebSocket library
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-beast")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++20")

-- Add required packages
local use_system_packages = has_config("use_system_packages")
add_requires("boost", {configs = {system = use_system_packages}})
add_requires("nlohmann_json", {system = use_system_packages})
add_requires("fmt", {system = use_system_packages})
add_requires("spdlog", {system = use_system_packages})

-- Source files
local sources = {
    "http.cpp",
    "ws.cpp"
}

-- Header files
local headers = {
    "http.hpp",
    "http_utils.hpp",
    "ws.hpp"
}

target("atom-extra-beast")
    set_kind("static")

    -- Add files
    for _, src in ipairs(sources) do
        add_files(src)
    end

    for _, hdr in ipairs(headers) do
        add_headerfiles(hdr)
    end

    -- Include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("boost", "nlohmann_json", "fmt", "spdlog")

    -- Set C++ standard
    set_languages("c++20")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "beast")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
