#-----------------------------------------------------------------------------
# Nexus Example Applications
#-----------------------------------------------------------------------------
# This file manages the build of example applications based on Kconfig
# configuration. Applications are only built for embedded platforms.
#
# Configuration options are defined in applications/Kconfig
#-----------------------------------------------------------------------------

# Only build embedded examples for non-native platforms
if(NOT NEXUS_PLATFORM STREQUAL "native")

    message(STATUS "Configuring example applications...")

    #-------------------------------------------------------------------------
    # Application Build Control
    #-------------------------------------------------------------------------
    # Applications are built based on Kconfig configuration options:
    #   - CONFIG_BUILD_APP_BLINKY
    #   - CONFIG_BUILD_APP_SHELL_DEMO
    #   - CONFIG_BUILD_APP_CONFIG_DEMO
    #   - CONFIG_BUILD_APP_STM32_BLINKY
    #   - CONFIG_BUILD_APP_FREERTOS_DEMO
    #-------------------------------------------------------------------------

    # Basic blinky example
    if(CONFIG_BUILD_APP_BLINKY AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/blinky)
        add_subdirectory(blinky)
        message(STATUS "  [+] blinky")
    endif()

    # Shell demo example
    if(CONFIG_BUILD_APP_SHELL_DEMO AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shell_demo)
        add_subdirectory(shell_demo)
        message(STATUS "  [+] shell_demo")
    endif()

    # Config demo example
    if(CONFIG_BUILD_APP_CONFIG_DEMO AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config_demo)
        add_subdirectory(config_demo)
        message(STATUS "  [+] config_demo")
    endif()

    # STM32 minimal blinky example
    if(CONFIG_BUILD_APP_STM32_BLINKY AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32_blinky)
        add_subdirectory(stm32_blinky)
        message(STATUS "  [+] stm32_blinky")
    endif()

    # FreeRTOS demo example
    if(CONFIG_BUILD_APP_FREERTOS_DEMO AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/freertos_demo)
        add_subdirectory(freertos_demo)
        message(STATUS "  [+] freertos_demo")
    endif()

    message(STATUS "Example applications configured")

else()
    message(STATUS "Native platform: Skipping embedded applications")
endif()

#-----------------------------------------------------------------------------
# End of Example Applications
#-----------------------------------------------------------------------------
