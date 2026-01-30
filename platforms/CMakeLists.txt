##############################################################################
# Platform Support
##############################################################################

#-----------------------------------------------------------------------------
# Platform Selection
#-----------------------------------------------------------------------------

message(STATUS "Configuring platform: ${NEXUS_PLATFORM}")

# Add platform-specific subdirectory
if(NEXUS_PLATFORM MATCHES "^(stm32|stm32f4|stm32h7|stm32l4)$")
    # All STM32 variants use the stm32 subdirectory
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32)
        add_subdirectory(stm32)
    else()
        message(WARNING "STM32 platform directory not found, using native platform")
        add_subdirectory(native)
    endif()

elseif(NEXUS_PLATFORM STREQUAL "native")
    add_subdirectory(native)

elseif(NEXUS_PLATFORM STREQUAL "esp32")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/esp32)
        add_subdirectory(esp32)
    else()
        message(WARNING "ESP32 platform not yet implemented")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "nrf52")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nrf52)
        add_subdirectory(nrf52)
    else()
        message(WARNING "nRF52 platform not yet implemented")
    endif()

elseif(NEXUS_PLATFORM STREQUAL "gd32")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/gd32)
        add_subdirectory(gd32)
    else()
        message(WARNING "GD32 platform not yet implemented")
    endif()

else()
    message(FATAL_ERROR "Unsupported platform: ${NEXUS_PLATFORM}")
endif()

#-----------------------------------------------------------------------------
# End of Platform Support
#-----------------------------------------------------------------------------
