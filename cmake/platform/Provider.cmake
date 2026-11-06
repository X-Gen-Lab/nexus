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
        message(FATAL_ERROR
            "Selected provider target must be a local static, object or "
            "interface library: ${target}")
    endif()
    # Native models have no mandatory device startup or system translation unit.
    # Every supplied path still belongs to the selected build input contract.
    if(NOT NEXUS_CPU_ARCH STREQUAL "native")
        foreach(required NEXUS_STARTUP_SOURCE NEXUS_SYSTEM_SOURCE)
            if(NOT ${required})
                message(FATAL_ERROR "Selected provider is missing ${required}")
            endif()
        endforeach()
    endif()
    foreach(property NEXUS_STARTUP_SOURCE NEXUS_SYSTEM_SOURCE)
        foreach(source IN LISTS ${property})
            if(NOT IS_ABSOLUTE "${source}" OR NOT EXISTS "${source}" OR
                    IS_DIRECTORY "${source}")
                message(FATAL_ERROR
                    "Selected provider ${property} requires an existing "
                    "absolute file: ${source}")
            endif()
        endforeach()
    endforeach()
    foreach(property NEXUS_SYSTEM_PRIVATE_INCLUDES NEXUS_SYSTEM_VENDOR_INCLUDES)
        foreach(directory IN LISTS ${property})
            if(NOT IS_ABSOLUTE "${directory}" OR
                    NOT IS_DIRECTORY "${directory}")
                message(FATAL_ERROR
                    "Selected provider ${property} requires an existing "
                    "absolute directory: ${directory}")
            endif()
        endforeach()
    endforeach()
    # Check direct include exports and ordinary BUILD_INTERFACE path lists.
    # Nested generator expressions and transitive dependency usage requirements
    # remain outside this declaration check; actual consumers test header scope.
    get_target_property(public_includes "${target}" INTERFACE_INCLUDE_DIRECTORIES)
    string(REGEX REPLACE "\\$<BUILD_INTERFACE:([^<>]*)>" "\\1"
        public_includes "${public_includes}")
    foreach(public_include IN LISTS public_includes)
        if(public_include MATCHES "\\$<" OR NOT IS_ABSOLUTE "${public_include}")
            continue()
        endif()
        file(REAL_PATH "${public_include}" public_directory)
        foreach(property NEXUS_SYSTEM_PRIVATE_INCLUDES NEXUS_SYSTEM_VENDOR_INCLUDES)
            foreach(directory IN LISTS ${property})
                file(REAL_PATH "${directory}" private_directory)
                if(public_directory STREQUAL private_directory)
                    message(FATAL_ERROR
                        "Selected provider publicly exports ${property}: "
                        "${directory}")
                endif()
            endforeach()
        endforeach()
    endforeach()
    if(NOT kind STREQUAL "INTERFACE_LIBRARY")
        nexus_apply_target_policy("${target}")
    endif()
endfunction()
