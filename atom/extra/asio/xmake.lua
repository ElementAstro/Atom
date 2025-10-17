-- xmake configuration for ASIO networking library extensions
-- Author: Max Qian
-- License: GPL3

add_rules("mode.debug", "mode.release")

set_project("atom-extra-asio")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")  -- C++23 required for std::expected

-- Add required packages
add_requires("asio", {optional = true})
add_requires("boost", {optional = true, configs = {system = true}})

-- MQTT client source files
local mqtt_sources = {
    "mqtt/client.cpp",
    "mqtt/packet.cpp"
}

-- MQTT client header files
local mqtt_headers = {
    "mqtt/client.hpp",
    "mqtt/packet.hpp",
    "mqtt/protocol.hpp",
    "mqtt/test_client.hpp",
    "mqtt/test_packet.hpp",
    "mqtt/test_protocol.hpp",
    "mqtt/test_types.hpp",
    "mqtt/types.hpp"
}

-- SSE (Server-Sent Events) source files
local sse_sources = {
    "sse/event.cpp",
    "sse/event_store.cpp"
}

-- SSE header files
local sse_headers = {
    "sse/event.hpp",
    "sse/event_store.hpp",
    "sse/sse.hpp"
}

-- Main header files
local asio_headers = {
    "asio_compatibility.hpp"
}

target("atom-extra-asio")
    set_kind("static")

    -- Add MQTT sources
    for _, src in ipairs(mqtt_sources) do
        add_files(src)
    end

    -- Add SSE sources
    for _, src in ipairs(sse_sources) do
        add_files(src)
    end

    -- Add all headers
    for _, hdr in ipairs(mqtt_headers) do
        add_headerfiles(hdr)
    end
    for _, hdr in ipairs(sse_headers) do
        add_headerfiles(hdr)
    end
    for _, hdr in ipairs(asio_headers) do
        add_headerfiles(hdr)
    end

    -- Include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("asio", "boost")

    -- Add system libraries
    add_syslinks("pthread")

    -- Set C++ standard
    set_languages("c++23")

    -- Installation
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        local headerdir = path.join(installdir, "include", "atom", "extra", "asio")
        os.mkdir(headerdir)
        for _, hdr in ipairs(asio_headers) do
            os.cp(hdr, headerdir)
        end
        os.mkdir(path.join(headerdir, "mqtt"))
        for _, hdr in ipairs(mqtt_headers) do
            os.cp(hdr, path.join(headerdir, "mqtt"))
        end
        os.mkdir(path.join(headerdir, "sse"))
        for _, hdr in ipairs(sse_headers) do
            os.cp(hdr, path.join(headerdir, "sse"))
        end
    end)
