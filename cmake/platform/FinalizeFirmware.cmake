# The linker has written the executable before this rule starts. On rejection,
# withdraw that image so restoring a previous graph must link it again.
execute_process(COMMAND ${command} RESULT_VARIABLE status)
if(status STREQUAL "0")
    execute_process(
        COMMAND "${objcopy}" -O binary "${image}" "${binary}"
        RESULT_VARIABLE status
    )
    if(status STREQUAL "0")
        return()
    endif()
    file(REMOVE "${report}")
endif()
file(REMOVE "${image}" "${binary}" "${map}")
message(FATAL_ERROR "Firmware finalization failed: ${status}")
