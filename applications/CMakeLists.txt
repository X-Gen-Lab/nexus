##############################################################################
# Nexus Example Applications
##############################################################################

# Only build embedded examples for non-native platforms
if(NOT NEXUS_PLATFORM STREQUAL "native")
    add_subdirectory(blinky)
    add_subdirectory(shell_demo)
    add_subdirectory(config_demo)

    # Build FreeRTOS demo only when FreeRTOS backend is selected
    if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
        add_subdirectory(freertos_demo)
    endif()
endif()
