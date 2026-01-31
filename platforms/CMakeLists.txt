#-----------------------------------------------------------------------------
# Platform Support
#-----------------------------------------------------------------------------
# This file selects and configures the target platform based on Kconfig.
# Supported platforms: STM32, Native, ESP32, nRF52, GD32
#
# Author: Nexus Team
#-----------------------------------------------------------------------------

message(STATUS "=== Platform Configuration ===")

#-----------------------------------------------------------------------------
# Platform Selection from Kconfig
#-----------------------------------------------------------------------------

# Determine platform from Kconfig variables
if(CONFIG_PLATFORM_STM32)
    set(NEXUS_PLATFORM "stm32")
elseif(CONFIG_PLATFORM_NATIVE)
    set(NEXUS_PLATFORM "native")
elseif(CONFIG_PLATFORM_ESP32)
    set(NEXUS_PLATFORM "esp32")
elseif(CONFIG_PLATFORM_NRF52)
    set(NEXUS_PLATFORM "nrf52")
elseif(CONFIG_PLATFORM_GD32)
    set(NEXUS_PLATFORM "gd32")
else()
    message(FATAL_ERROR "No platform selected in Kconfig. Please run 'menuconfig' to configure.")
endif()

message(STATUS "Selected platform: ${NEXUS_PLATFORM}")

#-----------------------------------------------------------------------------
# Add Platform Subdirectory
#-----------------------------------------------------------------------------

if(NEXUS_PLATFORM STREQUAL "stm32")
    # STM32 platform
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32)
        add_subdirectory(stm32)
        message(STATUS "STM32 platform configured successfully")
    else()
        message(FATAL_ERROR "STM32 platform directory not found")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "native")
    # Native platform (PC simulation)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/native)
        add_subdirectory(native)
        message(STATUS "Native platform configured successfully")
    else()
        message(FATAL_ERROR "Native platform directory not found")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "esp32")
    # ESP32 platform
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/esp32)
        add_subdirectory(esp32)
        message(STATUS "ESP32 platform configured successfully")
    else()
        message(WARNING "ESP32 platform not yet implemented")
        message(FATAL_ERROR "ESP32 platform directory not found")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "nrf52")
    # nRF52 platform
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nrf52)
        add_subdirectory(nrf52)
        message(STATUS "nRF52 platform configured successfully")
    else()
        message(WARNING "nRF52 platform not yet implemented")
        message(FATAL_ERROR "nRF52 platform directory not found")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "gd32")
    # GD32 platform
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gd32)
        add_subdirectory(gd32)
        message(STATUS "GD32 platform configured successfully")
    else()
        message(WARNING "GD32 platform not yet implemented")
        message(FATAL_ERROR "GD32 platform directory not found")
    endif()

else()
    message(FATAL_ERROR "Unsupported platform: ${NEXUS_PLATFORM}")
endif()

#-----------------------------------------------------------------------------
# Platform Configuration Summary
#-----------------------------------------------------------------------------

message(STATUS "Platform: ${CONFIG_PLATFORM_NAME}")
message(STATUS "=== Platform Configuration Complete ===")

#-----------------------------------------------------------------------------
# End of Platform Support
#-----------------------------------------------------------------------------
