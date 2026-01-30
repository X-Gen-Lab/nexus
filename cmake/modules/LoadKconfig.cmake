##############################################################################
# LoadKconfig.cmake - Kconfig Configuration Loader
##############################################################################
#
# LoadKconfig.cmake
# Kconfig configuration loader for Nexus build system
# Author: Nexus Team
#
# This module reads the .config file and sets CMake variables for each
# configuration option.
#
##############################################################################

##############################################################################
# Kconfig Loading Function
##############################################################################

#
# Load Kconfig configuration from .config file
# CONFIG_FILE: Path to .config file
#
function(load_kconfig CONFIG_FILE)
    if(NOT EXISTS ${CONFIG_FILE})
        message(WARNING "Config file not found: ${CONFIG_FILE}")
        return()
    endif()

    message(STATUS "Loading Kconfig from: ${CONFIG_FILE}")

    # Read the config file
    file(STRINGS ${CONFIG_FILE} CONFIG_LINES)

    # Parse each line
    foreach(LINE ${CONFIG_LINES})
        # Skip comments and empty lines
        if(LINE MATCHES "^#" OR LINE MATCHES "^$")
            continue()
        endif()

        # Parse CONFIG_XXX=y format
        if(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=y$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} TRUE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} TRUE CACHE BOOL "Kconfig option" FORCE)
        # Parse CONFIG_XXX=n format
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=n$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        # Parse CONFIG_XXX="value" format (string)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=\"([^\"]*)\"$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" CACHE STRING "Kconfig option" FORCE)
        # Parse CONFIG_XXX=0xHEX format (hex number)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=(0x[0-9A-Fa-f]+)$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} CACHE STRING "Kconfig option" FORCE)
        # Parse CONFIG_XXX=number format (decimal number)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=([0-9]+)$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} CACHE STRING "Kconfig option" FORCE)
        # Parse # CONFIG_XXX is not set format
        elseif(LINE MATCHES "^# CONFIG_([A-Za-z0-9_]+) is not set$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        endif()
    endforeach()

    message(STATUS "Loaded Kconfig configuration")
endfunction()

#
# Apply Kconfig configuration to CMake variables
# Maps Kconfig options to CMake build variables
#
function(apply_kconfig_to_cmake)
    message(STATUS "Applying Kconfig configuration to CMake...")

    # Build Type Configuration
    if(DEFINED CONFIG_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "${CONFIG_BUILD_TYPE}" CACHE STRING "Build type from Kconfig" FORCE)
        message(STATUS "  Build Type: ${CONFIG_BUILD_TYPE}")
    endif()

    # Build Options
    if(DEFINED CONFIG_BUILD_TESTS)
        set(NEXUS_BUILD_TESTS ${CONFIG_BUILD_TESTS} CACHE BOOL "Build tests from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_BUILD_EXAMPLES)
        set(NEXUS_BUILD_EXAMPLES ${CONFIG_BUILD_EXAMPLES} CACHE BOOL "Build examples from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_ENABLE_COVERAGE)
        set(NEXUS_ENABLE_COVERAGE ${CONFIG_ENABLE_COVERAGE} CACHE BOOL "Enable coverage from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_ENABLE_SANITIZERS)
        set(NEXUS_ENABLE_SANITIZERS ${CONFIG_ENABLE_SANITIZERS} CACHE BOOL "Enable sanitizers from Kconfig" FORCE)
    endif()

    # Platform Configuration
    if(DEFINED CONFIG_PLATFORM_NAME)
        set(NEXUS_PLATFORM "${CONFIG_PLATFORM_NAME}" CACHE STRING "Platform from Kconfig" FORCE)
        message(STATUS "  Platform: ${CONFIG_PLATFORM_NAME}")
    endif()

    # Toolchain Configuration
    if(DEFINED CONFIG_TOOLCHAIN_NAME)
        set(NEXUS_TOOLCHAIN_NAME "${CONFIG_TOOLCHAIN_NAME}" CACHE STRING "Toolchain from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_TOOLCHAIN_FILE AND CONFIG_TOOLCHAIN_FILE)
        set(NEXUS_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/${CONFIG_TOOLCHAIN_FILE}" CACHE FILEPATH "Toolchain file from Kconfig" FORCE)
    endif()

    # ARM CPU Configuration
    if(DEFINED CONFIG_CPU_ARCH AND CONFIG_CPU_ARCH)
        set(NEXUS_CPU_ARCH "${CONFIG_CPU_ARCH}" CACHE STRING "CPU architecture from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_FPU_TYPE)
        set(NEXUS_FPU_TYPE "${CONFIG_FPU_TYPE}" CACHE STRING "FPU type from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_FLOAT_ABI)
        set(NEXUS_FLOAT_ABI "${CONFIG_FLOAT_ABI}" CACHE STRING "Float ABI from Kconfig" FORCE)
    endif()

    # Linker Configuration
    if(DEFINED CONFIG_LINKER_SCRIPT AND CONFIG_LINKER_SCRIPT)
        set(NEXUS_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/${CONFIG_LINKER_SCRIPT}" CACHE FILEPATH "Linker script from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_STACK_SIZE)
        set(NEXUS_STACK_SIZE "${CONFIG_STACK_SIZE}" CACHE STRING "Stack size from Kconfig" FORCE)
    endif()

    if(DEFINED CONFIG_HEAP_SIZE)
        set(NEXUS_HEAP_SIZE "${CONFIG_HEAP_SIZE}" CACHE STRING "Heap size from Kconfig" FORCE)
    endif()

    # Export to parent scope
    set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    set(NEXUS_BUILD_TESTS ${NEXUS_BUILD_TESTS} PARENT_SCOPE)
    set(NEXUS_BUILD_EXAMPLES ${NEXUS_BUILD_EXAMPLES} PARENT_SCOPE)
    set(NEXUS_ENABLE_COVERAGE ${NEXUS_ENABLE_COVERAGE} PARENT_SCOPE)
    set(NEXUS_ENABLE_SANITIZERS ${NEXUS_ENABLE_SANITIZERS} PARENT_SCOPE)
    set(NEXUS_PLATFORM "${NEXUS_PLATFORM}" PARENT_SCOPE)
    set(NEXUS_TOOLCHAIN_NAME "${NEXUS_TOOLCHAIN_NAME}" PARENT_SCOPE)
    set(NEXUS_TOOLCHAIN_FILE "${NEXUS_TOOLCHAIN_FILE}" PARENT_SCOPE)
    set(NEXUS_CPU_ARCH "${NEXUS_CPU_ARCH}" PARENT_SCOPE)
    set(NEXUS_FPU_TYPE "${NEXUS_FPU_TYPE}" PARENT_SCOPE)
    set(NEXUS_FLOAT_ABI "${NEXUS_FLOAT_ABI}" PARENT_SCOPE)
    set(NEXUS_LINKER_SCRIPT "${NEXUS_LINKER_SCRIPT}" PARENT_SCOPE)
    set(NEXUS_STACK_SIZE "${NEXUS_STACK_SIZE}" PARENT_SCOPE)
    set(NEXUS_HEAP_SIZE "${NEXUS_HEAP_SIZE}" PARENT_SCOPE)

    message(STATUS "Kconfig configuration applied to CMake")
endfunction()

#
# Apply compiler flags based on Kconfig configuration
#
macro(apply_kconfig_compiler_flags)
    # Sanitizers (only for native platform)
    if(DEFINED CONFIG_ENABLE_SANITIZERS AND CONFIG_ENABLE_SANITIZERS)
        if(NEXUS_PLATFORM STREQUAL "native")
            if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
                message(STATUS "Enabling sanitizers (ASan, UBSan)")
                add_compile_options(-fsanitize=address -fsanitize=undefined)
                add_link_options(-fsanitize=address -fsanitize=undefined)
            else()
                message(WARNING "Sanitizers requested but not supported by ${CMAKE_C_COMPILER_ID}")
            endif()
        else()
            message(WARNING "Sanitizers are only available for native platform")
        endif()
    endif()

    # Coverage flags are handled by NexusPlatform.cmake
endmacro()

##############################################################################
# End of LoadKconfig.cmake
##############################################################################
