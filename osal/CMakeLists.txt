##############################################################################
# OSAL Library
##############################################################################

# OSAL interface library (header-only for now)
add_library(osal_interface INTERFACE)

target_include_directories(osal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# OSAL implementation library
add_library(osal STATIC)

# Select adapter based on backend and platform
if(NEXUS_PLATFORM STREQUAL "native")
    target_sources(osal
        PRIVATE
            adapters/native/osal_native.c
    )
    message(STATUS "OSAL: Using native adapter")
elseif(NEXUS_OSAL_BACKEND STREQUAL "baremetal")
    target_sources(osal
        PRIVATE
            adapters/baremetal/osal_baremetal.c
    )
    message(STATUS "OSAL: Using baremetal adapter")
elseif(NEXUS_OSAL_BACKEND STREQUAL "freertos")
    target_sources(osal
        PRIVATE
            adapters/freertos/osal_freertos.c
    )
    # Link FreeRTOS kernel
    target_link_libraries(osal
        PRIVATE
            freertos_kernel
    )
    message(STATUS "OSAL: Using FreeRTOS adapter")
else()
    message(FATAL_ERROR "Unknown OSAL backend: ${NEXUS_OSAL_BACKEND}")
endif()

target_link_libraries(osal
    PUBLIC
        osal_interface
)
