#-----------------------------------------------------------------------------
# NexusToolchain.cmake - Toolchain Abstraction Layer
#-----------------------------------------------------------------------------
# Provides unified interface for different ARM toolchains
# Author: Nexus Team
#
# This module provides toolchain-agnostic functions that work across
# ARM GCC, ARM Clang, and IAR EWARM toolchains.
#
# Architecture:
#   Layer 2 - Toolchain Abstraction Layer (this file)
#   - Auto-detects toolchain once when included
#   - Provides unified cross-toolchain functions
#   - Handles toolchain-specific differences internally
#
# Usage:
#   include(NexusToolchain)  # Auto-initializes
#   nexus_generate_bin(my_app)
#   nexus_set_linker_script(my_app path/to/script)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Internal: Toolchain Detection
#-----------------------------------------------------------------------------

#
# Internal function to detect toolchain
# Called automatically when module is included
# Results are cached to avoid repeated detection
#
function(_nexus_detect_toolchain_internal)
    # Detect based on compiler ID
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(NEXUS_TOOLCHAIN_IS_GCC TRUE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_CLANG FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_IAR FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "arm-none-eabi-gcc" PARENT_SCOPE)
        message(STATUS "Toolchain detected: ARM GCC")

    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|ARMClang")
        set(NEXUS_TOOLCHAIN_IS_GCC FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_CLANG TRUE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_IAR FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "armclang" PARENT_SCOPE)
        message(STATUS "Toolchain detected: ARM Clang")

    elseif(CMAKE_C_COMPILER_ID STREQUAL "IAR")
        set(NEXUS_TOOLCHAIN_IS_GCC FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_CLANG FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_IAR TRUE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "iar-arm" PARENT_SCOPE)
        message(STATUS "Toolchain detected: IAR EWARM")

    else()
        set(NEXUS_TOOLCHAIN_IS_GCC FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_CLANG FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_IS_IAR FALSE PARENT_SCOPE)
        set(NEXUS_TOOLCHAIN_NAME "unknown" PARENT_SCOPE)
        message(WARNING "Unknown toolchain: ${CMAKE_C_COMPILER_ID}")
    endif()
endfunction()

#
# Public function for manual toolchain detection (backward compatibility)
# Normally not needed as detection is automatic
#
function(nexus_detect_toolchain)
    # Detection already done during module initialization
    # This function exists for backward compatibility
    if(NOT DEFINED NEXUS_TOOLCHAIN_INITIALIZED)
        message(WARNING "nexus_detect_toolchain() called before module initialization")
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Binary Generation Functions
#-----------------------------------------------------------------------------

#
# Generate binary file from ELF
# Arguments:
#   TARGET: Target name
#   OUTPUT: Output binary file path (optional, defaults to TARGET.bin)
#
function(nexus_generate_bin TARGET)
    cmake_parse_arguments(ARG "" "OUTPUT" "" ${ARGN})

    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT "${TARGET}.bin")
    endif()

    if(NEXUS_TOOLCHAIN_IS_GCC)
        # ARM GCC: objcopy
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary
                    $<TARGET_FILE:${TARGET}> ${ARG_OUTPUT}
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )

    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang: fromelf
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --bin -o ${ARG_OUTPUT}
                    $<TARGET_FILE:${TARGET}>
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )

    elseif(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR EWARM: ielftool
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --bin
                    $<TARGET_FILE:${TARGET}> ${ARG_OUTPUT}
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )
    endif()
endfunction()

#
# Generate Intel HEX file from ELF
# Arguments:
#   TARGET: Target name
#   OUTPUT: Output hex file path (optional, defaults to TARGET.hex)
#
function(nexus_generate_hex TARGET)
    cmake_parse_arguments(ARG "" "OUTPUT" "" ${ARGN})

    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT "${TARGET}.hex")
    endif()

    if(NEXUS_TOOLCHAIN_IS_GCC)
        # ARM GCC: objcopy
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex
                    $<TARGET_FILE:${TARGET}> ${ARG_OUTPUT}
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )

    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang: fromelf
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --i32 -o ${ARG_OUTPUT}
                    $<TARGET_FILE:${TARGET}>
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )

    elseif(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR EWARM: ielftool
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --ihex
                    $<TARGET_FILE:${TARGET}> ${ARG_OUTPUT}
            COMMENT "Generating ${ARG_OUTPUT}"
            VERBATIM
        )
    endif()
endfunction()

#
# Print target size information
# Arguments:
#   TARGET: Target name
#
function(nexus_print_target_size TARGET)
    if(NEXUS_TOOLCHAIN_IS_GCC)
        # ARM GCC: size utility
        if(CMAKE_SIZE)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_SIZE} --format=berkeley
                        $<TARGET_FILE:${TARGET}>
                COMMENT "Size of ${TARGET}:"
                VERBATIM
            )
        endif()

    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang: fromelf --info
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --info sizes,totals
                    $<TARGET_FILE:${TARGET}>
            COMMENT "Size of ${TARGET}:"
            VERBATIM
        )

    elseif(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR EWARM: ielftool --size
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --size
                    $<TARGET_FILE:${TARGET}>
            COMMENT "Size of ${TARGET}:"
            VERBATIM
        )
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Linker Script Configuration
#-----------------------------------------------------------------------------

#
# Set linker script for target
# Arguments:
#   TARGET: Target name
#   SCRIPT: Path to linker script
#
function(nexus_set_linker_script TARGET SCRIPT)
    if(NOT EXISTS ${SCRIPT})
        message(WARNING "Linker script not found: ${SCRIPT}")
        return()
    endif()

    if(NEXUS_TOOLCHAIN_IS_GCC)
        # ARM GCC: -T flag
        target_link_options(${TARGET} PRIVATE -T${SCRIPT})

    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang: --scatter flag
        target_link_options(${TARGET} PRIVATE --scatter=${SCRIPT})

    elseif(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR EWARM: --config flag
        target_link_options(${TARGET} PRIVATE --config ${SCRIPT})
    endif()

    message(STATUS "Linker script for ${TARGET}: ${SCRIPT}")
endfunction()

#
# Enable map file generation for target
# Arguments:
#   TARGET: Target name
#   OUTPUT: Output map file path (optional, defaults to TARGET.map)
#
function(nexus_generate_map TARGET)
    cmake_parse_arguments(ARG "" "OUTPUT" "" ${ARGN})

    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT "${TARGET}.map")
    endif()

    if(NEXUS_TOOLCHAIN_IS_GCC)
        # ARM GCC: -Wl,-Map=file.map
        target_link_options(${TARGET} PRIVATE -Wl,-Map=${ARG_OUTPUT})

    elseif(NEXUS_TOOLCHAIN_IS_CLANG)
        # ARM Clang: --map --list=file.map
        target_link_options(${TARGET} PRIVATE --map --list=${ARG_OUTPUT})

    elseif(NEXUS_TOOLCHAIN_IS_IAR)
        # IAR EWARM: --map file.map
        target_link_options(${TARGET} PRIVATE --map ${ARG_OUTPUT})
    endif()

    message(STATUS "Map file for ${TARGET}: ${ARG_OUTPUT}")
endfunction()

#-----------------------------------------------------------------------------
# Compiler Flags Configuration
#-----------------------------------------------------------------------------

#
# Add toolchain-specific compile options
# Arguments:
#   TARGET: Target name
#   OPTIONS: List of options (toolchain-agnostic)
#
function(nexus_add_compile_options TARGET)
    set(OPTIONS ${ARGN})

    foreach(OPT ${OPTIONS})
        if(OPT STREQUAL "WARNINGS_AS_ERRORS")
            if(NEXUS_TOOLCHAIN_IS_GCC OR NEXUS_TOOLCHAIN_IS_CLANG)
                target_compile_options(${TARGET} PRIVATE -Werror)
            elseif(NEXUS_TOOLCHAIN_IS_IAR)
                target_compile_options(${TARGET} PRIVATE --warnings_are_errors)
            endif()

        elseif(OPT STREQUAL "NO_BUILTIN")
            if(NEXUS_TOOLCHAIN_IS_GCC OR NEXUS_TOOLCHAIN_IS_CLANG)
                target_compile_options(${TARGET} PRIVATE -fno-builtin)
            elseif(NEXUS_TOOLCHAIN_IS_IAR)
                target_compile_options(${TARGET} PRIVATE --no_builtin)
            endif()

        elseif(OPT STREQUAL "FUNCTION_SECTIONS")
            if(NEXUS_TOOLCHAIN_IS_GCC OR NEXUS_TOOLCHAIN_IS_CLANG)
                target_compile_options(${TARGET} PRIVATE
                    -ffunction-sections -fdata-sections)
            endif()
        endif()
    endforeach()
endfunction()

#-----------------------------------------------------------------------------
# High-Level Configuration Function
#-----------------------------------------------------------------------------

#
# Configure ARM target with all necessary settings
# Arguments:
#   TARGET: Target name
#   LINKER_SCRIPT: Path to linker script (optional)
#   GENERATE_BIN: Generate .bin file (default: ON)
#   GENERATE_HEX: Generate .hex file (default: ON)
#   GENERATE_MAP: Generate .map file (default: ON)
#   PRINT_SIZE: Print size information (default: ON)
#
function(nexus_configure_target TARGET)
    cmake_parse_arguments(ARG
        ""
        "LINKER_SCRIPT;GENERATE_BIN;GENERATE_HEX;GENERATE_MAP;PRINT_SIZE"
        ""
        ${ARGN}
    )

    # Set defaults
    if(NOT DEFINED ARG_GENERATE_BIN)
        set(ARG_GENERATE_BIN ON)
    endif()

    if(NOT DEFINED ARG_GENERATE_HEX)
        set(ARG_GENERATE_HEX ON)
    endif()

    if(NOT DEFINED ARG_GENERATE_MAP)
        set(ARG_GENERATE_MAP ON)
    endif()

    if(NOT DEFINED ARG_PRINT_SIZE)
        set(ARG_PRINT_SIZE ON)
    endif()

    # Detect toolchain if not already done
    nexus_detect_toolchain()

    # Set linker script
    if(ARG_LINKER_SCRIPT)
        nexus_set_linker_script(${TARGET} ${ARG_LINKER_SCRIPT})
    endif()

    # Generate map file
    if(ARG_GENERATE_MAP)
        nexus_generate_map(${TARGET})
    endif()

    # Generate binary files
    if(ARG_GENERATE_BIN)
        nexus_generate_bin(${TARGET})
    endif()

    if(ARG_GENERATE_HEX)
        nexus_generate_hex(${TARGET})
    endif()

    # Print size information
    if(ARG_PRINT_SIZE)
        nexus_print_target_size(${TARGET})
    endif()

    message(STATUS "Configured target: ${TARGET}")
endfunction()

#-----------------------------------------------------------------------------
# Toolchain Validation
#-----------------------------------------------------------------------------

#
# Validate toolchain configuration
# Checks if required tools are available and properly configured
#
function(nexus_validate_toolchain)
    # Check if toolchain was initialized
    if(NOT DEFINED NEXUS_TOOLCHAIN_INITIALIZED)
        message(WARNING "Toolchain not initialized. Module should auto-initialize on include.")
        return()
    endif()

    # Validate C compiler
    if(NOT CMAKE_C_COMPILER)
        message(FATAL_ERROR "C compiler not found")
    endif()

    # Validate assembler for embedded targets
    if(NOT NEXUS_TARGET_NATIVE AND NOT CMAKE_ASM_COMPILER)
        message(WARNING "ASM compiler not found for embedded target")
    endif()

    # Validate objcopy for binary generation
    if(NOT CMAKE_OBJCOPY)
        if(NEXUS_TOOLCHAIN_IS_GCC)
            message(WARNING "objcopy not found - binary generation will be disabled")
        elseif(NEXUS_TOOLCHAIN_IS_CLANG)
            message(WARNING "fromelf not found - binary generation will be disabled")
        elseif(NEXUS_TOOLCHAIN_IS_IAR)
            message(WARNING "ielftool not found - binary generation will be disabled")
        endif()
    endif()

    message(STATUS "Toolchain validation: OK")
endfunction()

#-----------------------------------------------------------------------------
# Toolchain Information
#-----------------------------------------------------------------------------

#
# Print toolchain information
#
function(nexus_print_toolchain_info)
    message(STATUS "=== Toolchain Information ===")
    message(STATUS "  Name:       ${NEXUS_TOOLCHAIN_NAME}")
    message(STATUS "  Family:     ${NEXUS_TOOLCHAIN_FAMILY}")
    message(STATUS "  Vendor:     ${NEXUS_TOOLCHAIN_VENDOR}")
    message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")

    if(CMAKE_ASM_COMPILER)
        message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
    endif()

    if(CMAKE_LINKER)
        message(STATUS "  Linker:     ${CMAKE_LINKER}")
    endif()

    if(NEXUS_CPU_ARCH)
        message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH}")
    endif()

    if(NEXUS_FPU_TYPE)
        message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE}")
    endif()

    message(STATUS "=============================")
endfunction()

#-----------------------------------------------------------------------------
# Module Initialization
#-----------------------------------------------------------------------------

# Auto-detect toolchain when module is included
# This ensures toolchain is detected once and results are cached
if(NOT DEFINED NEXUS_TOOLCHAIN_INITIALIZED)
    _nexus_detect_toolchain_internal()

    # Cache detection results in parent scope
    set(NEXUS_TOOLCHAIN_IS_GCC ${NEXUS_TOOLCHAIN_IS_GCC} CACHE INTERNAL "")
    set(NEXUS_TOOLCHAIN_IS_CLANG ${NEXUS_TOOLCHAIN_IS_CLANG} CACHE INTERNAL "")
    set(NEXUS_TOOLCHAIN_IS_IAR ${NEXUS_TOOLCHAIN_IS_IAR} CACHE INTERNAL "")
    set(NEXUS_TOOLCHAIN_NAME ${NEXUS_TOOLCHAIN_NAME} CACHE INTERNAL "")
    set(NEXUS_TOOLCHAIN_INITIALIZED TRUE CACHE INTERNAL "Toolchain detection completed")

    message(STATUS "NexusToolchain: Initialized (${NEXUS_TOOLCHAIN_NAME})")
endif()

#-----------------------------------------------------------------------------
# End of NexusToolchain.cmake
#-----------------------------------------------------------------------------
