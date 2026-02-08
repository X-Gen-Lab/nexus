#-----------------------------------------------------------------------------
# OSAL Library Configuration (Optimized)
#-----------------------------------------------------------------------------
# Operating System Abstraction Layer providing unified OS interface.
# Uses table-driven backend selection for cleaner configuration.
#
# Author: Nexus Team
# Version: 2.0.0 (Optimized)
# Date: 2026-02-01
#-----------------------------------------------------------------------------

message(STATUS "Configuring OSAL library...")

#-----------------------------------------------------------------------------
# OSAL Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(osal_interface INTERFACE)

target_include_directories(osal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

nexus_log(VERBOSE "OSAL interface library created")

#-----------------------------------------------------------------------------
# Backend Selection Mapping
#-----------------------------------------------------------------------------

# Define backend mapping table: Kconfig flag -> backend info
set(BACKEND_MAP
    "CONFIG_OSAL_NATIVE:native:Native:adapters/native/osal_native.c"
    "CONFIG_OSAL_BAREMETAL:baremetal:Baremetal:adapters/baremetal/osal_baremetal.c"
    "CONFIG_OSAL_FREERTOS:freertos:FreeRTOS:adapters/freertos/osal_freertos.c"
    "CONFIG_OSAL_RTTHREAD:rtthread:RT-Thread:adapters/rtthread/osal_rtthread.c"
    "CONFIG_OSAL_ZEPHYR:zephyr:Zephyr:adapters/zephyr/osal_zephyr.c"
)

# Detect selected backend
set(OSAL_BACKEND "")
set(OSAL_BACKEND_NAME "")
set(OSAL_ADAPTER_SOURCES "")

foreach(MAPPING ${BACKEND_MAP})
    string(REPLACE ":" ";" MAPPING_PARTS ${MAPPING})
    list(GET MAPPING_PARTS 0 CONFIG_FLAG)
    list(GET MAPPING_PARTS 1 BACKEND_ID)
    list(GET MAPPING_PARTS 2 BACKEND_DISPLAY)
    list(GET MAPPING_PARTS 3 ADAPTER_SOURCE)

    if(${CONFIG_FLAG})
        set(OSAL_BACKEND ${BACKEND_ID})
        set(OSAL_BACKEND_NAME ${BACKEND_DISPLAY})
        set(OSAL_ADAPTER_SOURCES ${ADAPTER_SOURCE})
        break()
    endif()
endforeach()

# Fallback to CMake variable or platform default
if(NOT OSAL_BACKEND)
    if(DEFINED CONFIG_OSAL_BACKEND_NAME)
        set(OSAL_BACKEND "${CONFIG_OSAL_BACKEND_NAME}")
    elseif(DEFINED NEXUS_OSAL_BACKEND)
        set(OSAL_BACKEND "${NEXUS_OSAL_BACKEND}")
    elseif(NEXUS_PLATFORM STREQUAL "native")
        set(OSAL_BACKEND "native")
    endif()

    # Set adapter source based on backend
    if(OSAL_BACKEND)
        set(OSAL_ADAPTER_SOURCES "adapters/${OSAL_BACKEND}/osal_${OSAL_BACKEND}.c")
        string(TOUPPER ${OSAL_BACKEND} OSAL_BACKEND_UPPER)
        string(SUBSTRING ${OSAL_BACKEND_UPPER} 0 1 FIRST_CHAR)
        string(SUBSTRING ${OSAL_BACKEND} 1 -1 REST_CHARS)
        set(OSAL_BACKEND_NAME "${FIRST_CHAR}${REST_CHARS}")
    endif()
endif()

# Validate backend selection
if(NOT OSAL_BACKEND)
    nexus_fatal_error("No OSAL backend selected. Please configure in Kconfig.")
endif()

nexus_log(STATUS "Selected OSAL backend: ${OSAL_BACKEND_NAME}")

#-----------------------------------------------------------------------------
# Verify Adapter Source
#-----------------------------------------------------------------------------

set(ADAPTER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${OSAL_ADAPTER_SOURCES}")
if(NOT EXISTS ${ADAPTER_PATH})
    nexus_warning("OSAL adapter source not found: ${OSAL_ADAPTER_SOURCES}")
endif()

#-----------------------------------------------------------------------------
# Create OSAL Implementation Library
#-----------------------------------------------------------------------------

nexus_add_library(osal
    SOURCES ${OSAL_ADAPTER_SOURCES}
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters
    DEPS
        osal_interface
)

# Add backend-specific compile definitions
target_compile_definitions(osal
    PRIVATE
        OSAL_BACKEND_${OSAL_BACKEND_NAME}
        $<$<CONFIG:Debug>:OSAL_DEBUG>
)

#-----------------------------------------------------------------------------
# Backend-Specific Configuration
#-----------------------------------------------------------------------------

# Link FreeRTOS kernel if using FreeRTOS backend
if(OSAL_BACKEND STREQUAL "freertos")
    if(TARGET freertos_kernel)
        target_link_libraries(osal PRIVATE freertos_kernel)
        nexus_log(VERBOSE "Linked FreeRTOS kernel to OSAL")
    else()
        nexus_warning("FreeRTOS backend selected but freertos_kernel target not found")
    endif()
endif()

nexus_log(STATUS "OSAL configured with ${OSAL_BACKEND_NAME} backend")

# OSAL library configured successfully

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~100 lines (if-elseif chain)
# Optimized: ~120 lines (table-driven + documentation)
# Net change: +20 lines (better structure and logging)
#
# Key improvements:
# - Table-driven backend selection
# - Unified library creation (nexus_add_library)
# - Better error handling with nexus_fatal_error()
# - Cleaner logging with nexus_log()
# - Easier to add new backends (just add to BACKEND_MAP)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of OSAL Library Configuration
#-----------------------------------------------------------------------------
