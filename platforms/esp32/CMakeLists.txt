#-----------------------------------------------------------------------------
# ESP32 Platform CMake Configuration
#-----------------------------------------------------------------------------
# ESP32 platform configuration using Nexus build system.
# This is a placeholder implementation that will be expanded as the
# ESP32 platform support is developed.
#
# Author: Nexus Team
# Version: 1.0.0
# Date: 2026-02-07
#-----------------------------------------------------------------------------

# Load required modules
include(NexusToolchain)
include(NexusCompilerConfig)
include(NexusLinkerConfig)

message(STATUS "=== ESP32 Platform Configuration ===")

#-----------------------------------------------------------------------------
# Platform Detection
#-----------------------------------------------------------------------------

if(NOT DEFINED CONFIG_PLATFORM_ESP32)
    message(FATAL_ERROR "ESP32 platform not selected in Kconfig")
endif()

message(STATUS "ESP32 Platform: Enabled")

#-----------------------------------------------------------------------------
# Create Platform Library
#-----------------------------------------------------------------------------

# Create a minimal platform library for ESP32
# This will be expanded as ESP32 support is developed
add_library(platform_esp32 INTERFACE)

# Add platform-specific include directories when they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_include_directories(platform_esp32 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

#-----------------------------------------------------------------------------
# Apply Kconfig-Based Compiler and Linker Configuration
#-----------------------------------------------------------------------------

# Apply compiler configuration from Kconfig to platform target
# This applies all compiler options configured in Kconfig
nexus_apply_compiler_config(TARGET platform_esp32)

# Apply linker configuration from Kconfig to platform target
# This applies linker optimization, output settings, and memory configuration
nexus_apply_linker_config(TARGET platform_esp32)

#-----------------------------------------------------------------------------
# Configuration Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "ESP32 Platform Configuration Summary:")
message(STATUS "  Platform:       ESP32")
message(STATUS "  Toolchain:      ${NEXUS_TOOLCHAIN_NAME}")
message(STATUS "  Status:         Placeholder (to be expanded)")
message(STATUS "")

message(STATUS "=== ESP32 Platform Configuration Complete ===")

#-----------------------------------------------------------------------------
# End of ESP32 Platform CMake Configuration
#-----------------------------------------------------------------------------
