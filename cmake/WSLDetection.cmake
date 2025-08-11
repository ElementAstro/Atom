# Detect if running in a WSL environment
function(detect_wsl RESULT_VAR)
    # Try multiple methods to detect WSL

    # Method 1: Check /proc/version
    if(EXISTS "/proc/version")
        file(READ "/proc/version" VERSION_CONTENT)
        if(VERSION_CONTENT MATCHES "Microsoft" OR VERSION_CONTENT MATCHES "WSL" OR VERSION_CONTENT MATCHES "microsoft-standard")
            set(${RESULT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    # Method 2: Check /proc/sys/kernel/osrelease
    if(EXISTS "/proc/sys/kernel/osrelease")
        file(READ "/proc/sys/kernel/osrelease" KERNEL_RELEASE)
        if(KERNEL_RELEASE MATCHES "Microsoft" OR KERNEL_RELEASE MATCHES "WSL" OR KERNEL_RELEASE MATCHES "microsoft-standard")
            set(${RESULT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    # Method 3: Check WSL environment variables
    if(DEFINED ENV{WSL_DISTRO_NAME} OR DEFINED ENV{WSL_INTEROP} OR DEFINED ENV{IS_WSL})
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Method 4: Check for /run/WSL
    if(EXISTS "/run/WSL")
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Method 5: Check uname -r or uname -a output
    execute_process(COMMAND uname -r OUTPUT_VARIABLE UNAME_R_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(UNAME_R_OUTPUT MATCHES "Microsoft" OR UNAME_R_OUTPUT MATCHES "WSL" OR UNAME_R_OUTPUT MATCHES "microsoft-standard")
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    execute_process(COMMAND uname -a OUTPUT_VARIABLE UNAME_A_OUTPUT ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(UNAME_A_OUTPUT MATCHES "Microsoft" OR UNAME_A_OUTPUT MATCHES "WSL" OR UNAME_A_OUTPUT MATCHES "microsoft-standard")
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Default to not WSL
    set(${RESULT_VAR} FALSE PARENT_SCOPE)
endfunction()
