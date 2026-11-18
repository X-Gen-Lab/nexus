# Workload selection observes generated public identities. It does not change
# the platform assembly, routes, selected modes or provider construction.
include_guard(GLOBAL)

function(nexus_workload_identity generated kind selected output)
    if(NOT kind MATCHES "^(UART|GPIO)$")
        message(FATAL_ERROR "Unsupported workload identity class ${kind}")
    endif()
    file(
        STRINGS "${generated}/nexus_factory.h"
        rows
        REGEX "^    NX_${kind}_ID_[A-Z0-9_]+ = [0-9]+,?$"
    )
    set(identities)
    foreach(row IN LISTS rows)
        string(
            REGEX REPLACE "^    (NX_${kind}_ID_[A-Z0-9_]+) = .*"
            "\\1"
            id
            "${row}"
        )
        if(NOT id STREQUAL "NX_${kind}_ID_COUNT")
            list(APPEND identities "${id}")
        endif()
    endforeach()
    if(selected STREQUAL "")
        list(LENGTH identities count)
        if(NOT count EQUAL 1)
            message(
                FATAL_ERROR
                "${kind} workload requires exactly one selected "
                "instance, or an explicit NEXUS_WORKLOAD_${kind}_ID; "
                "available identities: ${identities}"
            )
        endif()
        list(GET identities 0 selected)
    endif()
    list(FIND identities "${selected}" identity_index)
    if(
        NOT selected MATCHES "^NX_${kind}_ID_[A-Z0-9_]+$"
        OR identity_index LESS 0
    )
        message(
            FATAL_ERROR
            "Workload ${kind} identity ${selected} is absent "
            "from this assembly; available identities: ${identities}"
        )
    endif()
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()

function(nexus_add_workload)
    set(NEXUS_WORKLOAD
        "empty"
        CACHE STRING
        "Minimal resource fixture: empty/gpio/uart/spi"
    )
    nexus_add_firmware(nexus_firmware
        SOURCES "${NEXUS_SOURCE_DIR}/tools/measurement/workload.c"
    )
    nexus_target_warnings(nexus_firmware STRICT)
    if(NOT NEXUS_WORKLOAD MATCHES "^(empty|gpio|uart|spi)$")
        message(FATAL_ERROR "Unknown minimal workload ${NEXUS_WORKLOAD}")
    endif()
    target_compile_definitions(
        nexus_firmware
        PRIVATE "NEXUS_WORKLOAD_${NEXUS_WORKLOAD}=1"
    )
    if(NEXUS_WORKLOAD MATCHES "^(gpio|uart)$")
        string(TOUPPER "${NEXUS_WORKLOAD}" workload_kind)
        set(NEXUS_WORKLOAD_${workload_kind}_ID
            ""
            CACHE STRING
            "Assembly-local typed factory ID used only by the workload"
        )
        nexus_workload_identity("${NEXUS_CONFIG_DIR}" "${workload_kind}"
            "${NEXUS_WORKLOAD_${workload_kind}_ID}" workload_identity
        )
        target_compile_definitions(
            nexus_firmware
            PRIVATE "NEXUS_WORKLOAD_${workload_kind}_ID=${workload_identity}"
        )
        if(NEXUS_WORKLOAD STREQUAL "gpio")
            string(
                REGEX REPLACE "^NX_GPIO_ID_"
                ""
                workload_gpio
                "${workload_identity}"
            )
            target_compile_definitions(
                nexus_firmware
                PRIVATE "NEXUS_WORKLOAD_GPIO_MASK=NEXUS_${workload_gpio}_MASK"
            )
        endif()
    endif()
endfunction()
