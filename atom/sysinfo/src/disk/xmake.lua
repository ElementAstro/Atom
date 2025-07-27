-- xmake configuration for Disk module
-- Author: Max Qian
-- License: GPL3

target("atom-sysinfo-disk")
    set_kind("static")
    set_languages("c++20")

    -- Main source files
    add_files("disk.cpp")
    
    -- Common functionality
    add_files("common/disk_util.cpp")
    
    -- Component sources
    add_files("components/disk_device.cpp")
    add_files("components/disk_info.cpp")
    add_files("components/disk_monitor.cpp")
    add_files("components/disk_security.cpp")
    add_files("components/disk_analytics.cpp")
    add_files("components/disk_performance.cpp")

    -- Header files
    add_headerfiles("disk.hpp")
    add_headerfiles("common/disk_types.hpp", "common/disk_util.hpp")
    add_headerfiles("components/*.hpp")

    -- Dependencies
    add_packages("spdlog")

    -- Include directories
    add_includedirs(".", {public = true})
    add_includedirs("common", {public = true})
    add_includedirs("components", {public = true})

    -- Platform-specific libraries
    if is_plat("windows") then
        add_syslinks("setupapi")
    elseif is_plat("linux", "freebsd") then
        add_syslinks("pthread")
    end

    -- Set output directory
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Installation
    on_install(function (target)
        os.cp(target:targetfile(), path.join(target:installdir(), "lib"))
        os.cp("*.hpp", path.join(target:installdir(), "include/atom/sysinfo/disk"))
        os.cp("common/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/disk/common"))
        os.cp("components/*.hpp", path.join(target:installdir(), "include/atom/sysinfo/disk/components"))
    end)
target_end()
