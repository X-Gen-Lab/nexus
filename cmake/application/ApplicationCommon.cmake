#-----------------------------------------------------------------------------
# ApplicationCommon.cmake - Application Build Common Module
#-----------------------------------------------------------------------------
# This module provides common build configuration for all applications.
# It defines standard post-build actions and compile definitions that are
# shared across all application targets.
#
# Scope: Application-level module (used by all applications)
# Location: cmake/application/ApplicationCommon.cmake
#
# Why in cmake/application/ instead of cmake/modules/?
#   1. Logical Separation: Application-level vs Platform-level modules
#   2. Clear Organization: cmake/application/ for app-related CMake files
#   3. Extensibility: Future support for different app types
#      - ApplicationCommon.cmake (standard apps)
#      - BootloaderCommon.cmake (bootloader apps)
#      - TestCommon.cmake (test apps)
#
# This module provides:
#   - Post-build binary generation (.bin, .hex, .map files)
#   - Size information printing
#   - Memory usage reporting
#   - Common compile definitions (APP_NAME, APP_VERSION, etc.)
#   - Linker script configuration
#   - Garbage collection optimization
#
# Usage:
#   list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/application)
#   include(ApplicationCommon)
#
#   add_executable(my_app main.c)
#   target_link_libraries(my_app PRIVATE platform_stm32)
#   nexus_application_setup(my_app VERSION "1.0.0")
#
# Author: Nexus Team
# Version: 1.1.0
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Application Setup Functions
#-----------------------------------------------------------------------------

#
# Setup common application configuration
#
# This function configures standard application properties including:
#   - Executable suffix (.elf for embedded targets)
#   - Compile definitions (APP_NAME, APP_VERSION, APP_BUILD_TIMESTAMP)
#   - Debug output flags
#   - Extra library linking
#   - Post-build actions
#
# Arguments:
#   APP_TARGET: Target name (required)
#   VERSION: Application version string (optional, default: "1.0.0")
#   EXTRA_LIBS: Additional libraries to link (optional)
#
# Example:
#   nexus_application_setup(my_app VERSION "2.1.0")
#   nexus_application_setup(my_app VERSION "1.0.0" EXTRA_LIBS my_lib)
#
function(nexus_application_setup APP_TARGET)
    # Parse optional arguments
    set(options "")
    set(oneValueArgs VERSION)
    set(multiValueArgs EXTRA_LIBS)
    cmake_parse_arguments(APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set default version if not provided
    if(NOT DEFINED APP_VERSION)
        if(DEFINED CONFIG_APP_VERSION_MAJOR AND DEFINED CONFIG_APP_VERSION_MINOR AND DEFINED CONFIG_APP_VERSION_PATCH)
            set(APP_VERSION "${CONFIG_APP_VERSION_MAJOR}.${CONFIG_APP_VERSION_MINOR}.${CONFIG_APP_VERSION_PATCH}")
        else()
            set(APP_VERSION "1.0.0")
        endif()
    endif()

    # Set executable suffix for embedded targets
    if(NEXUS_PLATFORM STREQUAL "stm32f4" OR NEXUS_PLATFORM MATCHES "^stm32")
        set_target_properties(${APP_TARGET}
            PROPERTIES
                SUFFIX ".elf"
        )
    endif()

    # Add common compile definitions
    target_compile_definitions(${APP_TARGET}
        PRIVATE
            APP_NAME="${APP_TARGET}"
            APP_VERSION="${APP_VERSION}"
    )

    # Add version information if enabled
    if(CONFIG_APP_ENABLE_VERSION_INFO)
        string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)
        target_compile_definitions(${APP_TARGET}
            PRIVATE
                APP_BUILD_TIMESTAMP="${BUILD_TIMESTAMP}"
        )
    endif()

    # Add debug output flag if enabled
    if(CONFIG_APP_ENABLE_DEBUG_OUTPUT)
        target_compile_definitions(${APP_TARGET}
            PRIVATE
                APP_DEBUG_OUTPUT=1
        )
    endif()

    # Link extra libraries if provided
    if(APP_EXTRA_LIBS)
        target_link_libraries(${APP_TARGET}
            PRIVATE
                ${APP_EXTRA_LIBS}
        )
    endif()

    # Setup post-build actions
    nexus_application_postbuild(${APP_TARGET})

    message(STATUS "Configured application: ${APP_TARGET} (v${APP_VERSION})")
endfunction()

#-----------------------------------------------------------------------------
# Post-Build Actions
#-----------------------------------------------------------------------------

#
# Setup post-build actions for application
#
# This function configures post-build steps including:
#   - Binary file generation (.bin, .hex)
#   - Map file generation
#   - Size information printing
#   - Linker garbage collection
#   - Memory usage reporting
#
# All actions are controlled by Kconfig options:
#   - CONFIG_APP_GENERATE_BIN: Generate .bin file
#   - CONFIG_APP_GENERATE_HEX: Generate .hex file
#   - CONFIG_APP_GENERATE_MAP: Generate .map file
#   - CONFIG_APP_PRINT_SIZE: Print size information
#   - CONFIG_APP_PRINT_MEMORY_USAGE: Print memory usage
#
# Arguments:
#   APP_TARGET: Target name (required)
#
# Note:
#   This function is automatically called by nexus_application_setup().
#   Normally you don't need to call it directly.
#
function(nexus_application_postbuild APP_TARGET)
    # Include toolchain abstraction layer (auto-initializes)
    include(NexusToolchain)

    # Generate linker map file
    if(CONFIG_APP_GENERATE_MAP)
        nexus_generate_map(${APP_TARGET})
    endif()

    # Add garbage collection (not needed for ARM Clang, handled by scatter file)
    if(NOT NEXUS_TOOLCHAIN_IS_CLANG)
        target_link_options(${APP_TARGET}
            PRIVATE
                -Wl,--gc-sections
        )

        if(CONFIG_APP_PRINT_MEMORY_USAGE)
            target_link_options(${APP_TARGET}
                PRIVATE
                    -Wl,--print-memory-usage
            )
        endif()
    endif()

    # Generate binary file
    if(CONFIG_APP_GENERATE_BIN AND CMAKE_OBJCOPY)
        nexus_generate_bin(${APP_TARGET})
    endif()

    # Generate hex file
    if(CONFIG_APP_GENERATE_HEX AND CMAKE_OBJCOPY)
        nexus_generate_hex(${APP_TARGET})
    endif()

    # Print size information
    if(CONFIG_APP_PRINT_SIZE AND CMAKE_SIZE)
        nexus_print_target_size(${APP_TARGET})
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Linker Script Configuration
#-----------------------------------------------------------------------------

#
# Setup linker script for application
#
# This function configures the linker script for the application target.
# Currently, the linker script is already configured in platform_stm32 library
# via target_link_options, so this function is a placeholder for future
# application-specific linker script customization.
#
# Arguments:
#   APP_TARGET: Target name (required)
#
# Future enhancements:
#   - Application-specific memory layout
#   - Custom section placement
#   - Multi-region memory configuration
#
function(nexus_application_linker_script APP_TARGET)
    # Linker script is already configured in platform CMakeLists.txt
    # via target_link_options on platform_stm32
    # No need to add it again here
endfunction()

#-----------------------------------------------------------------------------
# End of ApplicationCommon.cmake
#-----------------------------------------------------------------------------
