# Index the selection once on the target shared by providers and consumers.
include_guard(GLOBAL)
block()
    foreach(row IN LISTS NEXUS_SELECTED_CONTROLLERS)
        string(REPLACE "/" ";" fields "${row}")
        list(GET fields 0 kind)
        list(GET fields 1 controller)
        list(GET fields 2 mode)
        set_property(
            TARGET nexus_build_options
            APPEND
            PROPERTY "NEXUS_CONTROLLERS/${kind}" "${controller}"
        )
        set_property(
            TARGET nexus_build_options
            APPEND
            PROPERTY "NEXUS_MODES/${kind}" "${mode}"
        )
        set_property(
            TARGET nexus_build_options
            APPEND
            PROPERTY "NEXUS_MODES/${kind}/${controller}" "${mode}"
        )
    endforeach()
endblock()

function(nexus_controller_modes output kind)
    set(key "NEXUS_MODES/${kind}")
    if(NOT "${ARGV2}" STREQUAL "")
        string(APPEND key "/${ARGV2}")
    endif()
    get_property(values TARGET nexus_build_options PROPERTY "${key}")
    list(REMOVE_DUPLICATES values)
    set(${output} "${values}" PARENT_SCOPE)
endfunction()

function(nexus_controller_names output kind)
    get_property(
        values
        TARGET nexus_build_options
        PROPERTY "NEXUS_CONTROLLERS/${kind}"
    )
    list(REMOVE_DUPLICATES values)
    set(${output} "${values}" PARENT_SCOPE)
endfunction()
