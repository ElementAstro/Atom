function(update_vcpkg_baseline)
    # GitHub API URL to get the latest commit SHA for the master branch of vcpkg
    set(VCPKG_COMMIT_API_URL "https://api.github.com/repos/microsoft/vcpkg/commits/master")
    set(TEMP_FILE "${CMAKE_BINARY_DIR}/vcpkg_latest_commit.json")
    # Define a default baseline to use if fetching the latest fails
    set(DEFAULT_VCPKG_BASELINE "YOUR_DEFAULT_BASELINE_SHA_HERE") # Replace with a known good commit SHA

    message(STATUS "Attempting to download latest vcpkg commit info from ${VCPKG_COMMIT_API_URL}")

    # Download the latest commit information
    file(DOWNLOAD "${VCPKG_COMMIT_API_URL}" "${TEMP_FILE}"
         TIMEOUT 10 # seconds
         STATUS DOWNLOAD_STATUS
         LOG DOWNLOAD_LOG)

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_EC)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_MSG)

    set(USE_DEFAULT_BASELINE FALSE)
    if(NOT DOWNLOAD_EC EQUAL 0)
        message(WARNING "Failed to download vcpkg commit info: ${DOWNLOAD_MSG}. Log: ${DOWNLOAD_LOG}")
        set(USE_DEFAULT_BASELINE TRUE)
    else()
        # Read the downloaded content
        file(READ "${TEMP_FILE}" GITHUB_API_RESPONSE)
        file(REMOVE "${TEMP_FILE}") # Clean up the temporary file

        # Extract the SHA (baseline) using regex
        # The response is a JSON array, we're interested in the "sha" of the first commit object.
        # A more robust solution might involve a JSON parsing library if available/desired.
        # This regex looks for the first "sha": "commit_hash"
        string(REGEX MATCH "\"sha\"[ \t\n\r]*:[ \t\n\r]*\"([a-f0-9]+)\""
               LATEST_BASELINE_MATCH "${GITHUB_API_RESPONSE}")

        if(NOT LATEST_BASELINE_MATCH)
            message(WARNING "Could not parse latest baseline SHA from GitHub API response.")
            message(STATUS "GitHub API Response was: ${GITHUB_API_RESPONSE}")
            set(USE_DEFAULT_BASELINE TRUE)
        else()
            set(LATEST_BASELINE "${CMAKE_MATCH_1}")
            message(STATUS "Successfully fetched latest vcpkg baseline (commit SHA): ${LATEST_BASELINE}")
        endif()
    endif()

    if(USE_DEFAULT_BASELINE)
        message(WARNING "Using default vcpkg baseline: ${DEFAULT_VCPKG_BASELINE}")
        set(LATEST_BASELINE "${DEFAULT_VCPKG_BASELINE}")
    endif()

    # Read the existing vcpkg.json
    set(VCPKG_JSON_PATH "${CMAKE_SOURCE_DIR}/vcpkg.json")
    if(NOT EXISTS "${VCPKG_JSON_PATH}")
        message(WARNING "vcpkg.json not found at ${VCPKG_JSON_PATH}. Skipping update.")
        return()
    endif()
    file(READ "${VCPKG_JSON_PATH}" VCPKG_JSON_CONTENT)

    # Check if "builtin-baseline" already exists
    if(VCPKG_JSON_CONTENT MATCHES "\"builtin-baseline\"")
        string(REGEX REPLACE "\"builtin-baseline\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\""
                             "\"builtin-baseline\": \"${LATEST_BASELINE}\""
                             VCPKG_JSON_UPDATED "${VCPKG_JSON_CONTENT}")
    else()
        # Add "builtin-baseline" after the first opening curly brace
        # This assumes a simple JSON structure; for complex JSON, a proper parser would be better.
        string(REGEX REPLACE "\\{"
                             "{\n  \"builtin-baseline\": \"${LATEST_BASELINE}\","
                             VCPKG_JSON_UPDATED "${VCPKG_JSON_CONTENT}")
    endif()

    # Write the updated content back to vcpkg.json
    file(WRITE "${VCPKG_JSON_PATH}" "${VCPKG_JSON_UPDATED}")
    if(USE_DEFAULT_BASELINE)
        message(STATUS "Updated ${VCPKG_JSON_PATH} with default baseline: ${LATEST_BASELINE}")
    else()
        message(STATUS "Updated ${VCPKG_JSON_PATH} with latest baseline: ${LATEST_BASELINE}")
    endif()
endfunction()

# If the UPDATE_VCPKG_BASELINE option is specified, execute the update
if(UPDATE_VCPKG_BASELINE)
    update_vcpkg_baseline()
endif()
