##############################################################################
# HAL Tests
##############################################################################

#-----------------------------------------------------------------------------
# Platform-Specific HAL Tests
#-----------------------------------------------------------------------------

# Native platform tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/native)
    add_subdirectory(native)
    message(STATUS "HAL Tests: Native platform")
endif()

# STM32 platform tests (future)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32 AND NEXUS_PLATFORM MATCHES "^stm32")
    add_subdirectory(stm32)
    message(STATUS "HAL Tests: STM32 platform")
endif()

#-----------------------------------------------------------------------------
# End of HAL Tests
#-----------------------------------------------------------------------------
