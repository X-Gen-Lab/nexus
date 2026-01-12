##############################################################################
# Platform Support
##############################################################################

if(NEXUS_PLATFORM STREQUAL "stm32f4")
    add_subdirectory(stm32f4)
elseif(NEXUS_PLATFORM STREQUAL "stm32h7")
    add_subdirectory(stm32h7)
elseif(NEXUS_PLATFORM STREQUAL "native")
    add_subdirectory(native)
endif()
