# Source admission for optional component selectors; no implicit supplier fetch.
# Author: Nexus Team
include_guard(GLOBAL)

function(nexus_component_source identifier output)
    get_filename_component(
        root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.."
        ABSOLUTE
    )
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    get_property(packaged GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
    set(scope)
    if(packaged AND packaged STREQUAL root)
        list(APPEND scope --source-sdk)
    endif()
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${root}/tools/maintenance/component_lock.py" --root "${root}"
            --dependency "${identifier}" ${scope}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE record
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${identifier} source rejected: ${error}")
    endif()
    string(JSON count LENGTH "${record}" configure_inputs)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON input GET "${record}" configure_inputs ${index})
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}"
        )
    endforeach()
    string(JSON path GET "${record}" path)
    set("${output}" "${path}" PARENT_SCOPE)
endfunction()
