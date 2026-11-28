# The selected F1 checkout or installed manifest must prove its device pairing.
# Author: Nexus Team
function(nexus_stm32f1_official)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/maintenance/stm32f1_hal_lock.py" --source
            "${f1_hal}" --lock
            "${NEXUS_SOURCE_DIR}/dependencies/stm32f1-hal.lock.json"
            --cmsis-source "${f1_device}" --device
            "${NEXUS_VENDOR_DEVICE_HEADER}" "${NEXUS_VENDOR_DEVICE_STARTUP}"
        RESULT_VARIABLE result
        ERROR_VARIABLE error
        OUTPUT_VARIABLE identity
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "STM32F1 official SDK identity failed: ${error}")
    endif()
    string(JSON count LENGTH "${identity}" configure_inputs)
    if(count LESS 1)
        message(FATAL_ERROR "STM32F1 official configure inputs are empty")
    endif()
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${identity}" configure_inputs ${index})
        if(
            NOT IS_ABSOLUTE "${path}"
            OR NOT EXISTS "${path}"
            OR IS_DIRECTORY "${path}"
            OR IS_SYMLINK "${path}"
        )
            message(FATAL_ERROR "STM32F1 configure input is not a regular file")
        endif()
        set_property(
            DIRECTORY "${CMAKE_SOURCE_DIR}"
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}"
        )
    endforeach()
endfunction()
