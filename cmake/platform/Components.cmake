# Shared data names the leaf entry; each component owns its software targets.
# Author: Nexus Team
include_guard(GLOBAL)

function(_nexus_component_catalog output)
    get_filename_component(
        root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.."
        ABSOLUTE
    )
    set(catalog "${root}/components/catalog.json")
    file(READ "${catalog}" document)
    string(JSON version GET "${document}" schema_version)
    string(JSON kind TYPE "${document}" components)
    if(NOT version STREQUAL "1" OR NOT kind STREQUAL "ARRAY")
        message(FATAL_ERROR "Invalid component catalogue")
    endif()
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${catalog}"
        )
    endif()
    set(${output} "${document}" PARENT_SCOPE)
endfunction()

function(_nexus_component_names output document)
    string(JSON count LENGTH "${document}" components)
    set(choices)
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON name GET "${document}" components ${index} id)
            list(FIND choices "${name}" duplicate)
            if(NOT duplicate LESS 0)
                message(FATAL_ERROR "Duplicate component catalogue ID: ${name}")
            endif()
            list(APPEND choices "${name}")
        endforeach()
    endif()
    set(${output} "${choices}" PARENT_SCOPE)
endfunction()

function(nexus_component_choices output)
    _nexus_component_catalog(document)
    _nexus_component_names(choices "${document}")
    set(${output} "${choices}" PARENT_SCOPE)
endfunction()

# Generated public headers and private binding implementations have distinct
# usage scopes. Leaf targets declare only the contracts they actually require.
function(_nexus_component_usage component target property aggregate)
    get_property(usage TARGET "${target}" PROPERTY "${property}")
    if(NOT usage)
        return()
    endif()
    if(NOT TARGET "${usage}" OR NOT TARGET "${aggregate}")
        message(
            FATAL_ERROR
            "Selected component binding usage is unavailable: ${component}"
        )
    endif()
    get_property(
        existing
        TARGET "${aggregate}"
        PROPERTY INTERFACE_LINK_LIBRARIES
    )
    if(NOT usage IN_LIST existing)
        target_link_libraries("${aggregate}" INTERFACE "${usage}")
    endif()
endfunction()

function(nexus_resolve_components output)
    _nexus_component_catalog(document)
    _nexus_component_names(choices "${document}")
    get_filename_component(
        root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.."
        ABSOLUTE
    )
    set(selected)
    foreach(component IN LISTS ARGN)
        list(FIND choices "${component}" index)
        if(index LESS 0)
            message(FATAL_ERROR "No owned target for component ${component}")
        endif()
        string(JSON target GET "${document}" components ${index} target)
        string(JSON entry GET "${document}" components ${index} entry)
        if(entry AND NOT TARGET "${target}")
            if(
                NOT entry
                    MATCHES
                    "^components/([A-Za-z0-9_]+/)+[A-Za-z0-9_]+\\.cmake$"
            )
                message(FATAL_ERROR "Invalid component catalogue leaf path")
            endif()
            include("${root}/${entry}")
        endif()
        if(NOT TARGET "${target}")
            message(
                FATAL_ERROR
                "Selected component target is unavailable: ${component}"
            )
        endif()
        _nexus_component_usage("${component}" "${target}"
            NEXUS_BINDINGS_USAGE_TARGET nexus_component_bindings
        )
        _nexus_component_usage("${component}" "${target}"
            NEXUS_BINDINGS_API_TARGET nexus_component_api
        )
        list(APPEND selected "${target}")
    endforeach()
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()
