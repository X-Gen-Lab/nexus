##############################################################################
# Nexus Example Applications
##############################################################################

message(STATUS "Configuring example applications...")

#-----------------------------------------------------------------------------
# Platform-Specific Applications
#-----------------------------------------------------------------------------

# Only build embedded examples for non-native platforms
if(NOT NEXUS_PLATFORM STREQUAL "native")

    # Basic blinky example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/blinky)
        add_subdirectory(blinky)
        message(STATUS "Application: blinky")
    endif()

    # Shell demo example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shell_demo)
        add_subdirectory(shell_demo)
        message(STATUS "Application: shell_demo")
    endif()

    # Config demo example
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config_demo)
        add_subdirectory(config_demo)
        message(STATUS "Application: config_demo")
    endif()

    # STM32-specific examples
    if(NEXUS_PLATFORM MATCHES "^stm32")
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32_blinky)
            add_subdirectory(stm32_blinky)
            message(STATUS "Application: stm32_blinky")
        endif()
    endif()

    # FreeRTOS demo (only when FreeRTOS backend is selected)
    if(NEXUS_OSAL_BACKEND STREQUAL "freertos")
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/freertos_demo)
            add_subdirectory(freertos_demo)
            message(STATUS "Application: freertos_demo")
        endif()
    endif()

else()
    message(STATUS "Native platform: Skipping embedded applications")
endif()

#-----------------------------------------------------------------------------
# End of Example Applications
#-----------------------------------------------------------------------------
