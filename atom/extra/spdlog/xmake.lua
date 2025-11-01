-- xmake configuration for modern logging library (spdlog wrapper)
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-spdlog")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")  -- C++23 required

-- Add required packages
add_requires("spdlog", {optional = true})
add_requires("fmt", {optional = true})

-- Source files
local sources = {
    "core/context.cpp",
    "filters/filter.cpp",
    "filters/builtin_filters.cpp",
    "sampling/sampler.cpp",
    "events/event_system.cpp",
    "utils/structured_data.cpp",
    "utils/timer.cpp",
    "utils/archiver.cpp",
    "logger/logger.cpp",
    "logger/manager.cpp"
}

-- Header files
local headers = {
    "modern_log.h"
}

target("atom-extra-spdlog")
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
    add_packages("spdlog", "fmt")

    -- Set C++ standard
    set_languages("c++23")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "spdlog")
        os.mkdir(headerdir)
        for _, hdr in ipairs(headers) do
            os.cp(hdr, headerdir)
        end
    end)
