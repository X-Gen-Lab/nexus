if(NOT NEXUS_PLATFORM MATCHES "^(native|stm32)$")
    message(FATAL_ERROR "Platform '${NEXUS_PLATFORM}' has no maintained driver implementation")
endif()
add_subdirectory(${NEXUS_PLATFORM})
