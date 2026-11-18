# A passing recheck of the existing ELF leaves the link input unchanged.
# Ninja can then prune the link while still enforcing the new budget.
if(EXISTS "${stamp}" AND EXISTS "${image}")
    execute_process(COMMAND ${command} RESULT_VARIABLE status)
    if(status STREQUAL "0")
        return()
    endif()
endif()

# New objects or a new memory layout can fix an obsolete image. The actual
# link and its post-build budget check decide the result in that case.
file(TOUCH "${stamp}")
