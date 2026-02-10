#-----------------------------------------------------------------------------
# nRF52 Platform CMake Configuration
#-----------------------------------------------------------------------------
# nRF52 platform configuration using Nexus build system.
# This is a placeholder implementation that will be expanded as the
# nRF52 platform support is developed.
#
# Author: Nexus Team
# Version: 1.0.0
# Date: 2026-02-07
#-----------------------------------------------------------------------------

# Load required modules
include(NexusToolchain)
include(NexusCompilerConfig)
include(NexusLinkerConfig)

message(STATUS "=== nRF52 Platform Configuration ===")

#-----------------------------------------------------------------------------
# Platform Detection
#-----------------------------------------------------------------------------

if(NOT DEFINED CONFIG_PLATFORM_NRF52)
    message(FATAL_ERROR "nRF52 platform not selected in Kconfig")
endif()

message(STATUS "nRF52 Platform: Enabled")

#-----------------------------------------------------------------------------
# Create Platform Library
#-----------------------------------------------------------------------------

# Create a minimal platform library for nRF52
# This will be expanded as nRF52 support is developed
add_library(platform_nrf52 INTERFACE)

# Add platform-specific include directories when they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_include_directories(platform_nrf52 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

#-----------------------------------------------------------------------------
# Apply Kconfig-Based Compiler and Linker Configuration
#-----------------------------------------------------------------------------

# Apply compiler configuration from Kconfig to platform target
# This applies all compiler options configured in Kconfig
nexus_apply_compiler_config(TARGET platform_nrf52)

# Apply linker configuration from Kconfig to platform target
# This applies linker optimization, output settings, and memory configuration
nexus_apply_linker_config(TARGET platform_nrf52)

#-----------------------------------------------------------------------------
# Configuration Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "nRF52 Platform Configuration Summary:")
message(STATUS "  Platform:       nRF52")
message(STATUS "  Toolchain:      ${NEXUS_TOOLCHAIN_NAME}")
message(STATUS "  Status:         Placeholder (to be expanded)")
message(STATUS "")

message(STATUS "=== nRF52 Platform Configuration Complete ===")

#-----------------------------------------------------------------------------
# End of nRF52 Platform CMake Configuration
#-----------------------------------------------------------------------------
