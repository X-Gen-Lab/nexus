##############################################################################
# Platform Tests
##############################################################################

# Add STM32 platform tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/stm32/CMakeLists.txt)
    add_subdirectory(stm32)
endif()

