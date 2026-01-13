# FreeRTOS Adapter CMake Configuration
# This file is included when NEXUS_OSAL_BACKEND is set to "freertos"

# Note: The actual source file (osal_freertos.c) is added in the parent
# CMakeLists.txt. This file provides additional configuration if needed.

# Export the FreeRTOSConfig.h include directory
# This is used by the FreeRTOS.cmake module when no platform-specific
# config is available.
