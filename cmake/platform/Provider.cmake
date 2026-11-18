# Validate the selected provider before optional consumer firmware is declared.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/TargetPolicy.cmake")

function(nexus_validate_selected_provider target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "Selected provider target is missing: ${target}")
    endif()
    get_target_property(aliased "${target}" ALIASED_TARGET)
    if(aliased)
        set(target "${aliased}")
    endif()
    get_target_property(imported "${target}" IMPORTED)
    get_target_property(kind "${target}" TYPE)
    if(imported OR NOT kind MATCHES "^(STATIC|OBJECT|INTERFACE)_LIBRARY$")
        message(
            FATAL_ERROR
            "Selected provider target must be a local static, object or "
            "interface library: ${target}"
        )
    endif()
    foreach(property NEXUS_STARTUP_SOURCES NEXUS_STARTUP_USAGE_TARGET)
        get_property(${property} TARGET "${target}" PROPERTY "${property}")
        if(NOT NEXUS_CPU_ARCH STREQUAL "native" AND NOT ${property})
            message(FATAL_ERROR "Selected provider is missing ${property}")
        endif()
    endforeach()
    foreach(source IN LISTS NEXUS_STARTUP_SOURCES)
        if(
            NOT IS_ABSOLUTE "${source}"
            OR NOT EXISTS "${source}"
            OR IS_DIRECTORY "${source}"
        )
            message(
                FATAL_ERROR
                "Selected provider NEXUS_STARTUP_SOURCES requires an existing "
                "absolute file: ${source}"
            )
        endif()
    endforeach()
    set(private_includes)
    if(NEXUS_STARTUP_USAGE_TARGET)
        if(NOT TARGET "${NEXUS_STARTUP_USAGE_TARGET}")
            message(
                FATAL_ERROR
                "Selected provider startup usage target is missing"
            )
        endif()
        get_target_property(usage_kind "${NEXUS_STARTUP_USAGE_TARGET}" TYPE)
        get_target_property(
            usage_imported
            "${NEXUS_STARTUP_USAGE_TARGET}"
            IMPORTED
        )
        if(usage_imported OR NOT usage_kind STREQUAL "INTERFACE_LIBRARY")
            message(
                FATAL_ERROR
                "Selected provider startup usage target must be a local interface library"
            )
        endif()
        get_property(
            private_includes
            TARGET "${NEXUS_STARTUP_USAGE_TARGET}"
            PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        )
        foreach(directory IN LISTS private_includes)
            if(
                NOT IS_ABSOLUTE "${directory}"
                OR NOT IS_DIRECTORY "${directory}"
            )
                message(
                    FATAL_ERROR
                    "Selected provider startup include requires an existing "
                    "absolute directory: ${directory}"
                )
            endif()
        endforeach()
    endif()
    # Check direct exports and ordinary BUILD_INTERFACE lists. Transitive and
    # nested generator expressions remain covered by actual consumer builds.
    get_property(
        public_includes
        TARGET "${target}"
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    )
    string(
        REGEX REPLACE "\\$<BUILD_INTERFACE:([^<>]*)>"
        "\\1"
        public_includes
        "${public_includes}"
    )
    foreach(public_include IN LISTS public_includes)
        if(public_include MATCHES "\\$<" OR NOT IS_ABSOLUTE "${public_include}")
            continue()
        endif()
        file(REAL_PATH "${public_include}" public_directory)
        foreach(directory IN LISTS private_includes)
            file(REAL_PATH "${directory}" private_directory)
            if(public_directory STREQUAL private_directory)
                message(
                    FATAL_ERROR
                    "Selected provider publicly exports startup include: ${directory}"
                )
            endif()
        endforeach()
    endforeach()
    if(NOT kind STREQUAL "INTERFACE_LIBRARY")
        nexus_apply_target_policy("${target}")
    endif()
endfunction()
