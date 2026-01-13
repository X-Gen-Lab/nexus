# FreeRTOS Integration Module for Nexus Platform
# This module configures FreeRTOS kernel for use with the OSAL FreeRTOS adapter

# Only include FreeRTOS when the backend is freertos
if(NOT NEXUS_OSAL_BACKEND STREQUAL "freertos")
    return()
endif()

message(STATUS "Configuring FreeRTOS kernel...")

# Set FreeRTOS port based on platform
if(NEXUS_PLATFORM STREQUAL "native")
    # Native build uses POSIX port on Unix or MSVC/MinGW on Windows
    if(UNIX)
        set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "FreeRTOS port")
    elseif(MINGW)
        set(FREERTOS_PORT "MSVC_MINGW" CACHE STRING "FreeRTOS port")
    elseif(MSVC)
        set(FREERTOS_PORT "MSVC_MINGW" CACHE STRING "FreeRTOS port")
    else()
        message(FATAL_ERROR "Unsupported native platform for FreeRTOS")
    endif()
elseif(NEXUS_PLATFORM STREQUAL "stm32f4")
    set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "FreeRTOS port")
elseif(NEXUS_PLATFORM STREQUAL "stm32h7")
    set(FREERTOS_PORT "GCC_ARM_CM7" CACHE STRING "FreeRTOS port")
else()
    message(FATAL_ERROR "Unsupported platform for FreeRTOS: ${NEXUS_PLATFORM}")
endif()

# Set heap implementation (heap_4 is recommended for most applications)
set(FREERTOS_HEAP "4" CACHE STRING "FreeRTOS heap implementation (1-5)")

# Create freertos_config interface library
# This provides the path to FreeRTOSConfig.h
add_library(freertos_config INTERFACE)

# Check for platform-specific FreeRTOSConfig.h first
if(EXISTS "${CMAKE_SOURCE_DIR}/platforms/${NEXUS_PLATFORM}/FreeRTOSConfig.h")
    target_include_directories(freertos_config SYSTEM
        INTERFACE
            ${CMAKE_SOURCE_DIR}/platforms/${NEXUS_PLATFORM}
    )
    message(STATUS "FreeRTOS: Using platform-specific config from platforms/${NEXUS_PLATFORM}")
else()
    # Fall back to default config in osal/adapters/freertos
    target_include_directories(freertos_config SYSTEM
        INTERFACE
            ${CMAKE_SOURCE_DIR}/osal/adapters/freertos
    )
    message(STATUS "FreeRTOS: Using default config from osal/adapters/freertos")
endif()

# Add FreeRTOS kernel subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/ext/freertos ${CMAKE_BINARY_DIR}/freertos)

message(STATUS "FreeRTOS: Port = ${FREERTOS_PORT}")
message(STATUS "FreeRTOS: Heap = heap_${FREERTOS_HEAP}")
