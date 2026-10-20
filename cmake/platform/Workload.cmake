# Workload selection observes generated public identities. It does not change
# the platform assembly, routes, selected modes or provider construction.
include_guard(GLOBAL)

function(nexus_workload_identity generated kind selected output)
    if(NOT kind MATCHES "^(UART|GPIO)$")
        message(FATAL_ERROR "Unsupported workload identity class ${kind}")
    endif()
    file(STRINGS "${generated}/nexus_factory.h" rows
        REGEX "^    NX_${kind}_ID_[A-Z0-9_]+ = [0-9]+,?$")
    set(identities)
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^    (NX_${kind}_ID_[A-Z0-9_]+) = .*" "\\1" id "${row}")
        if(NOT id STREQUAL "NX_${kind}_ID_COUNT")
            list(APPEND identities "${id}")
        endif()
    endforeach()
    if(selected STREQUAL "")
        list(LENGTH identities count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "${kind} workload requires exactly one selected "
                "instance, or an explicit NEXUS_WORKLOAD_${kind}_ID; "
                "available identities: ${identities}")
        endif()
        list(GET identities 0 selected)
    endif()
    list(FIND identities "${selected}" identity_index)
    if(NOT selected MATCHES "^NX_${kind}_ID_[A-Z0-9_]+$" OR identity_index LESS 0)
        message(FATAL_ERROR "Workload ${kind} identity ${selected} is absent "
            "from this assembly; available identities: ${identities}")
    endif()
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()
