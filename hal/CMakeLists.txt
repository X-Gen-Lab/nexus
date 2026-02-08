#-----------------------------------------------------------------------------
# HAL Library Configuration (Optimized)
#-----------------------------------------------------------------------------
# Hardware Abstraction Layer providing unified device access interface.
# Uses pattern-based source collection and unified library creation.
#
# Author: Nexus Team
# Version: 2.0.0 (Optimized)
# Date: 2026-02-01
#-----------------------------------------------------------------------------

message(STATUS "Configuring HAL library...")

#-----------------------------------------------------------------------------
# HAL Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(hal_interface INTERFACE)

target_include_directories(hal_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
)

nexus_log(VERBOSE "HAL interface library created")

#-----------------------------------------------------------------------------
# Collect HAL Core Sources
#-----------------------------------------------------------------------------

nexus_collect_sources(HAL_CORE_SOURCES
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PATTERNS "*.c"
)

list(LENGTH HAL_CORE_SOURCES HAL_SOURCE_COUNT)
nexus_log(VERBOSE "Collected ${HAL_SOURCE_COUNT} HAL core sources")

#-----------------------------------------------------------------------------
# Create HAL Implementation Library
#-----------------------------------------------------------------------------

nexus_add_library(hal
    SOURCES ${HAL_CORE_SOURCES}
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPS
        hal_interface
        osal
)

# Add debug compile definition
target_compile_definitions(hal
    PRIVATE
        $<$<CONFIG:Debug>:HAL_DEBUG>
)

nexus_log(STATUS "HAL library configured with ${HAL_SOURCE_COUNT} sources")

# HAL library configured successfully

#-----------------------------------------------------------------------------
# Code Reduction Summary
#-----------------------------------------------------------------------------
# Original: ~60 lines (manual source listing)
# Optimized: ~70 lines (pattern-based + documentation)
# Net change: +10 lines (better structure and logging)
#
# Key improvements:
# - Pattern-based source collection (nexus_collect_sources)
# - Unified library creation (nexus_add_library)
# - Better logging and diagnostics
# - Easier to maintain (no manual source listing)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Platform-specific sources are added by platform CMakeLists.txt
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# End of HAL Library Configuration
#-----------------------------------------------------------------------------
