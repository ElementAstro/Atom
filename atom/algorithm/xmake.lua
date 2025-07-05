-- Set minimum xmake version
set_xmakever("2.8.0")

-- Set project info
set_project("atom-algorithm")
set_version("1.0.0", {build = "%Y%m%d%H%M"})

-- Set languages
set_languages("c11", "cxx17")

-- Add build modes
add_rules("mode.debug", "mode.release")

-- Define dependencies
local atom_algorithm_depends = {"atom-error"}

-- Add required packages
add_requires("openssl", "tbb", "loguru")

-- Define the main target
target("atom-algorithm")
    -- Set target kind
    set_kind("static")

    -- Add source files (automatically collect .cpp files)
    add_files("*.cpp")

    -- Add header files (automatically collect .hpp files)
    add_headerfiles("*.hpp")

    -- Add include directories
    add_includedirs(".", {public = true})

    -- Add packages
    add_packages("openssl", "tbb", "loguru")

    -- Add system libraries
    add_syslinks("pthread")

    -- Add dependencies (assuming they are other xmake targets or libraries)
    for _, dep in ipairs(atom_algorithm_depends) do
        add_deps(dep)
    end

    -- Set properties
    set_targetdir("$(buildir)/lib")
    set_objectdir("$(buildir)/obj")

    -- Enable position independent code for static library
    add_cxflags("-fPIC", {tools = {"gcc", "clang"}})
    add_cflags("-fPIC", {tools = {"gcc", "clang"}})

    -- Set version info
    set_version("1.0.0")

    -- Add compile features
    set_policy("build.optimization.lto", true)

    -- Installation rules
    after_build(function (target)
        -- Custom post-build actions if needed
    end)

    -- Install target
    on_install(function (target)
        local installdir = target:installdir() or "$(prefix)"
        os.cp(target:targetfile(), path.join(installdir, "lib"))
        os.cp("*.hpp", path.join(installdir, "include", "atom-algorithm"))
    end)

-- Optional: Add option to control dependency building
option("enable-deps-check")
    set_default(true)
    set_description("Enable dependency checking")
option_end()

-- Dependency verification (equivalent to your foreach loop)
if has_config("enable-deps-check") then
    for _, dep in ipairs(atom_algorithm_depends) do
        -- Convert atom-error to ATOM_BUILD_ERROR format
        local dep_var = dep:upper():gsub("ATOM%-", "ATOM_BUILD_")
        if not has_config(dep_var:lower()) then
            print("Warning: Module atom-algorithm depends on " .. dep ..
                  ", but that module is not enabled for building")
        end
    end
end

-- Add custom configuration for module dependencies
for _, dep in ipairs(atom_algorithm_depends) do
    local config_name = dep:gsub("atom%-", "atom_build_")
    option(config_name)
        set_default(true)
        set_description("Enable building " .. dep .. " module")
    option_end()
end
