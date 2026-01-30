##############################################################################
# OSAL Library
##############################################################################

#-----------------------------------------------------------------------------
# OSAL Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(osal_interface INTERFACE)

target_include_directories(osal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# OSAL Implementation Library
#-----------------------------------------------------------------------------

# Determine OSAL backend from Kconfig or CMake variable
if(DEFINED CONFIG_OSAL_BACKEND_NAME)
    set(OSAL_BACKEND "${CONFIG_OSAL_BACKEND_NAME}")
    message(STATUS "OSAL Backend from Kconfig: ${OSAL_BACKEND}")
else()
    set(OSAL_BACKEND "${NEXUS_OSAL_BACKEND}")
    message(STATUS "OSAL Backend from CMake: ${OSAL_BACKEND}")
endif()

# Select adapter based on backend and platform
set(OSAL_ADAPTER_SOURCES "")

# Check Kconfig flags first, then fall back to backend name
if(CONFIG_OSAL_NATIVE OR OSAL_BACKEND STREQUAL "native" OR NEXUS_PLATFORM STREQUAL "native")
    set(OSAL_ADAPTER_SOURCES adapters/native/osal_native.c)
    set(OSAL_BACKEND_NAME "Native")
    message(STATUS "OSAL Adapter: Native")

elseif(CONFIG_OSAL_BAREMETAL OR OSAL_BACKEND STREQUAL "baremetal")
    set(OSAL_ADAPTER_SOURCES adapters/baremetal/osal_baremetal.c)
    set(OSAL_BACKEND_NAME "Baremetal")
    message(STATUS "OSAL Adapter: Baremetal")

elseif(CONFIG_OSAL_FREERTOS OR OSAL_BACKEND STREQUAL "freertos")
    set(OSAL_ADAPTER_SOURCES adapters/freertos/osal_freertos.c)
    set(OSAL_BACKEND_NAME "FreeRTOS")
    message(STATUS "OSAL Adapter: FreeRTOS")

elseif(CONFIG_OSAL_RTTHREAD OR OSAL_BACKEND STREQUAL "rtthread")
    set(OSAL_ADAPTER_SOURCES adapters/rtthread/osal_rtthread.c)
    set(OSAL_BACKEND_NAME "RT-Thread")
    message(STATUS "OSAL Adapter: RT-Thread")

elseif(CONFIG_OSAL_ZEPHYR OR OSAL_BACKEND STREQUAL "zephyr")
    set(OSAL_ADAPTER_SOURCES adapters/zephyr/osal_zephyr.c)
    set(OSAL_BACKEND_NAME "Zephyr")
    message(STATUS "OSAL Adapter: Zephyr")

else()
    message(FATAL_ERROR "Unknown OSAL backend: ${OSAL_BACKEND}")
endif()

# Verify adapter source exists
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${OSAL_ADAPTER_SOURCES})
    message(WARNING "OSAL adapter source not found: ${OSAL_ADAPTER_SOURCES}")
endif()

# Create OSAL static library
add_library(osal STATIC ${OSAL_ADAPTER_SOURCES})

# Include directories
target_include_directories(osal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters
)

# Link dependencies
target_link_libraries(osal
    PUBLIC
        osal_interface
)

# Link FreeRTOS kernel if using FreeRTOS backend
if(CONFIG_OSAL_FREERTOS OR OSAL_BACKEND STREQUAL "freertos")
    if(TARGET freertos_kernel)
        target_link_libraries(osal PRIVATE freertos_kernel)
    else()
        message(WARNING "FreeRTOS backend selected but freertos_kernel target not found")
    endif()
endif()

# Compile definitions
target_compile_definitions(osal
    PRIVATE
        OSAL_BACKEND_${OSAL_BACKEND_NAME}
        $<$<CONFIG:Debug>:OSAL_DEBUG>
)

# Compiler options
if(MSVC)
    target_compile_options(osal PRIVATE /W4 /FS)
else()
    target_compile_options(osal PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "OSAL Library: Configured with ${OSAL_BACKEND_NAME} backend")

#-----------------------------------------------------------------------------
# End of OSAL Library
#-----------------------------------------------------------------------------
