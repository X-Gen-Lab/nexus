# Resolver catalogs carry logical names and exact physical file paths.
function(_nexus_read_configuration_catalog catalog paths_output labels_output)
    file(READ "${catalog}" inputs)
    string(JSON kind TYPE "${inputs}")
    if(NOT kind STREQUAL "OBJECT")
        message(
            FATAL_ERROR
            "Configuration input catalog must be an object: ${catalog}"
        )
    endif()
    string(JSON count LENGTH "${inputs}")
    if(count EQUAL 0)
        message(
            FATAL_ERROR
            "Configuration input catalog must be nonempty: ${catalog}"
        )
    endif()
    set(paths)
    set(labels)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON label MEMBER "${inputs}" ${index})
        string(JSON kind TYPE "${inputs}" "${label}")
        if(NOT kind STREQUAL "STRING")
            message(
                FATAL_ERROR
                "Configuration input path must be a string: ${label}"
            )
        endif()
        string(JSON path GET "${inputs}" "${label}")
        if(
            NOT IS_ABSOLUTE "${path}"
            OR NOT EXISTS "${path}"
            OR IS_DIRECTORY "${path}"
        )
            message(
                FATAL_ERROR
                "Configuration input must be an existing absolute file: ${path}"
            )
        endif()
        list(APPEND paths "${path}")
        list(APPEND labels "${label}")
    endforeach()
    set(${paths_output} "${paths}" PARENT_SCOPE)
    set(${labels_output} "${labels}" PARENT_SCOPE)
endfunction()

function(_nexus_watch_configuration_inputs bundle)
    foreach(catalog IN LISTS ARGN)
        _nexus_read_configuration_catalog("${bundle}/${catalog}.json" paths labels)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${paths})
    endforeach()
endfunction()

# A failing configuration removes the previous owned generated bundle.
function(nexus_resolve_assembly)
    get_cmake_property(assembly_cache_variables CACHE_VARIABLES)
    foreach(override IN LISTS assembly_cache_variables)
        if(override MATCHES "^NEXUS_(CPU|ARCH|IRQ|OS)_")
            message(
                FATAL_ERROR
                "${override} is not a platform override; "
                "use NEXUS_ASSEMBLY_FILE"
            )
        endif()
    endforeach()
    get_filename_component(
        assembly
        "${NEXUS_ASSEMBLY_FILE}"
        ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}"
    )
    set(bundle "${NEXUS_BINARY_DIR}/generated")
    set(test_support_arguments)
    if(NEXUS_TEST_SUPPORT)
        list(APPEND test_support_arguments --test-support)
    endif()
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --source-root
            "${NEXUS_SOURCE_DIR}" --assembly "${assembly}" --output "${bundle}"
            ${test_support_arguments}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Configuration rejected: ${output}${error}")
    endif()
    include("${bundle}/selection.cmake")
    # Register the resolver's exact configuration-code contract. This tracks
    # These are generation inputs; CMake owns production source targets.
    _nexus_watch_configuration_inputs("${bundle}" configuration_code_paths)
    # The resolver owns this list. Package-private selections do not add
    # MCU or peripheral names to this common build function.
    if(NOT NEXUS_SELECTION_VARIABLES)
        message(FATAL_ERROR "Missing resolver selection contract")
    endif()
    set(selection_variables ${NEXUS_SELECTION_VARIABLES})
    list(LENGTH selection_variables selected_count)
    list(REMOVE_DUPLICATES selection_variables)
    list(LENGTH selection_variables unique_count)
    if(NOT selected_count EQUAL unique_count)
        message(FATAL_ERROR "Duplicate variable in resolver selection contract")
    endif()
    foreach(value IN LISTS selection_variables)
        if(NOT value MATCHES "^NEXUS_[A-Za-z0-9_]+$" OR NOT DEFINED "${value}")
            message(FATAL_ERROR "Invalid resolver selection contract: ${value}")
        endif()
        set(${value} "${${value}}" PARENT_SCOPE)
    endforeach()
    set(NEXUS_SELECTION_VARIABLES "${selection_variables}" PARENT_SCOPE)
    set(NEXUS_CONFIG_DIR "${bundle}" PARENT_SCOPE)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS "${assembly}"
    )
    # Every authored and fact input comes from the same resolver snapshot.
    # CMake consumes identities; it never parses or normalizes authored TOML.
    _nexus_watch_configuration_inputs("${bundle}" input_paths)
endfunction()
