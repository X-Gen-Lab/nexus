##############################################################################
# iar-arm.cmake - IAR ARM Toolchain Configuration
##############################################################################
#
# iar-arm.cmake
# IAR ARM toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures IAR Embedded Workbench for ARM (iccarm)
# compiler for ARM Cortex-M microcontrollers. IAR is a commercial toolchain
# with excellent code optimization and debugging capabilities.
#
# Requires IAR Embedded Workbench for ARM license.
# Validates: Requirements 4.3
#
##############################################################################

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

##############################################################################
# Toolchain Programs
##############################################################################

# Find IAR toolchain programs
find_program(CMAKE_C_COMPILER iccarm)
find_program(CMAKE_CXX_COMPILER iccarm)
find_program(CMAKE_ASM_COMPILER iasmarm)
find_program(CMAKE_AR iarchive)
find_program(CMAKE_LINKER ilinkarm)
find_program(CMAKE_OBJCOPY ielftool)

# Verify compiler was found
if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "iccarm compiler not found.\n"
        "Please install IAR Embedded Workbench for ARM and add it to PATH.\n"
        "\n"
        "Installation instructions:\n"
        "  - Download from: https://www.iar.com/products/architectures/arm/\n"
        "  - Requires commercial license from IAR Systems\n"
    )
endif()

# Set toolchain identification
set(NEXUS_TOOLCHAIN_NAME "iar-arm")
set(NEXUS_TOOLCHAIN_FAMILY "iar")
set(NEXUS_TOOLCHAIN_VENDOR "IAR")

##############################################################################
# CPU Architecture Configuration
##############################################################################

#
# Configure CPU flags based on target platform
# Supports Cortex-M0/M0+/M3/M4/M7/M33 with optional FPU
#

# Default to Cortex-M4 with FPU if not specified
if(NOT DEFINED NEXUS_CPU_ARCH)
    set(NEXUS_CPU_ARCH "Cortex-M4")
endif()

if(NOT DEFINED NEXUS_FPU_TYPE)
    set(NEXUS_FPU_TYPE "VFPv4_sp")
endif()

# Build CPU flags based on configuration
set(CPU_FLAGS "--cpu=${NEXUS_CPU_ARCH}")

# Add FPU flags if specified
if(NEXUS_FPU_TYPE)
    set(CPU_FLAGS "${CPU_FLAGS} --fpu=${NEXUS_FPU_TYPE}")
endif()

# Initialize compiler flags
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} --endian=little --dlib_config normal")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} --endian=little --dlib_config normal")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

#-----------------------------------------------------------------------------
# Build Type Specific Flags
#-----------------------------------------------------------------------------
# Note: Build type flags are now managed centrally by NexusCompilerFlags.cmake
# See cmake/modules/NexusCompilerFlags.cmake::nexus_set_default_build_type_flags()
#
# IAR-specific optimization flags:
#   -On:  No optimization
#   -Om:  Medium optimization
#   -Oh:  High optimization for speed
#   -Ohz: High optimization for size
#   -r:   Generate debug information

##############################################################################
# Linker Flags
##############################################################################

# Initialize linker flags
# Note: Linker script will be added by platform CMakeLists.txt
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--semihosting --entry __iar_program_start"
)

##############################################################################
# Cross-Compilation Settings
##############################################################################

# Don't try to compile test programs (cross-compiling)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths for find_* commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

##############################################################################
# IAR-Specific Settings
##############################################################################

# Disable compiler warnings about unknown pragmas
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --diag_suppress=Pe1665")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --diag_suppress=Pe1665")

# Enable C99 mode
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --c99")

##############################################################################
# Toolchain Adapter Functions
##############################################################################

#
# Generate binary files from ELF
# TARGET: Target name
#
function(nexus_generate_binary TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --bin $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} --ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMENT "Generating ${TARGET}.bin and ${TARGET}.hex"
    )
endfunction()

#
# Print section sizes
# TARGET: Target name
#
function(nexus_print_size TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --size $<TARGET_FILE:${TARGET}>
        COMMENT "Size of ${TARGET}:"
    )
endfunction()

#
# Generate disassembly listing
# TARGET: Target name
#
function(nexus_generate_listing TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --code $<TARGET_FILE:${TARGET}> ${TARGET}.lst
        COMMENT "Generating ${TARGET}.lst"
    )
endfunction()

#
# Configure target for IAR toolchain
# TARGET: Target name
# LINKER_SCRIPT: Path to linker script (ICF file)
#
function(nexus_configure_arm_target TARGET)
    # Parse arguments
    cmake_parse_arguments(ARG "" "LINKER_SCRIPT" "" ${ARGN})

    # Set linker script if provided
    if(ARG_LINKER_SCRIPT)
        target_link_options(${TARGET} PRIVATE
            --config ${ARG_LINKER_SCRIPT}
        )
    endif()

    # Generate binary and hex files
    nexus_generate_binary(${TARGET})
    nexus_print_size(${TARGET})
endfunction()

##############################################################################
# Toolchain Information
##############################################################################

message(STATUS "IAR ARM Toolchain Configuration:")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
message(STATUS "  Linker:     ${CMAKE_LINKER}")
message(STATUS "  Archiver:   ${CMAKE_AR}")
message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH}")
message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE}")
message(STATUS "  CPU Flags:  ${CPU_FLAGS}")

##############################################################################
# End of iar-arm.cmake
##############################################################################
