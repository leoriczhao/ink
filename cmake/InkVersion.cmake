function(ink_validate_version version source)
    if(NOT "${version}" MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR
            "ink: ${source} produced '${version}', expected X.Y.Z")
    endif()
endfunction()

function(ink_detect_git_version out_version out_source)
    set(${out_version} "" PARENT_SCOPE)
    set(${out_source} "" PARENT_SCOPE)

    find_program(INK_GIT_EXECUTABLE git)
    if(NOT INK_GIT_EXECUTABLE OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        return()
    endif()

    execute_process(
        COMMAND "${INK_GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                describe --tags --exact-match --match "v[0-9]*.[0-9]*.[0-9]*"
        OUTPUT_VARIABLE exact_tag
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE exact_result
    )

    if(exact_result EQUAL 0)
        string(REGEX REPLACE "^v" "" version "${exact_tag}")
        set(${out_version} "${version}" PARENT_SCOPE)
        set(${out_source} "git exact tag ${exact_tag}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${INK_GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
                describe --tags --abbrev=0 --match "v[0-9]*.[0-9]*.[0-9]*"
        OUTPUT_VARIABLE nearest_tag
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE nearest_result
    )

    if(nearest_result EQUAL 0)
        string(REGEX REPLACE "^v" "" version "${nearest_tag}")
        set(${out_version} "${version}" PARENT_SCOPE)
        set(${out_source} "git nearest tag ${nearest_tag}" PARENT_SCOPE)
    endif()
endfunction()

function(ink_resolve_version)
    if(DEFINED INK_VERSION AND NOT "${INK_VERSION}" STREQUAL "")
        set(resolved_version "${INK_VERSION}")
        set(version_source "-DINK_VERSION")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" resolved_version)
        string(STRIP "${resolved_version}" resolved_version)
        set(version_source "VERSION file")
    else()
        ink_detect_git_version(resolved_version version_source)
    endif()

    if("${resolved_version}" STREQUAL "")
        set(resolved_version "0.0.0")
        set(version_source "fallback")
    endif()

    ink_validate_version("${resolved_version}" "${version_source}")

    string(REPLACE "." ";" version_parts "${resolved_version}")
    list(GET version_parts 0 version_major)
    list(GET version_parts 1 version_minor)
    list(GET version_parts 2 version_patch)

    set(INK_VERSION "${resolved_version}" PARENT_SCOPE)
    set(INK_VERSION_FULL "${resolved_version}" PARENT_SCOPE)
    set(INK_VERSION_SOURCE "${version_source}" PARENT_SCOPE)
    set(INK_VERSION_MAJOR "${version_major}" PARENT_SCOPE)
    set(INK_VERSION_MINOR "${version_minor}" PARENT_SCOPE)
    set(INK_VERSION_PATCH "${version_patch}" PARENT_SCOPE)

    message(STATUS "ink: version ${resolved_version} (${version_source})")
endfunction()
