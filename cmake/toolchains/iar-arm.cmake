#-----------------------------------------------------------------------------
# iar-arm.cmake - IAR ARM Toolchain Configuration
#-----------------------------------------------------------------------------
# IAR ARM toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures IAR Embedded Workbench for ARM (iccarm)
# compiler for ARM Cortex-M microcontrollers. IAR is a commercial toolchain
# with excellent code optimization and debugging capabilities.
#
# Requires: CMake 3.21+, IAR Embedded Workbench for ARM license
# Validates: Requirements 4.3
#-----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Generic)

#-----------------------------------------------------------------------------
# CPU Architecture Configuration (must be set before project())
#-----------------------------------------------------------------------------

# Default to Cortex-M4 with FPU if not specified
if(NOT DEFINED NEXUS_CPU_ARCH)
    set(NEXUS_CPU_ARCH "cortex-m4")
endif()

# CPU flags are set explicitly in CMAKE_C_FLAGS_INIT
# Do NOT set CMAKE_SYSTEM_PROCESSOR here to avoid CMake auto-detection issues

#-----------------------------------------------------------------------------
# Toolchain Programs
#-----------------------------------------------------------------------------

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

#-----------------------------------------------------------------------------
# CPU Architecture Flags Configuration
#-----------------------------------------------------------------------------

# Configure CPU flags based on target platform
# Supports Cortex-M0/M0+/M3/M4/M7/M33 with optional FPU

# Map lowercase CPU names to IAR's capitalized format
set(IAR_CPU_MAP_cortex-m0 "Cortex-M0")
set(IAR_CPU_MAP_cortex-m0plus "Cortex-M0+")
set(IAR_CPU_MAP_cortex-m1 "Cortex-M1")
set(IAR_CPU_MAP_cortex-m3 "Cortex-M3")
set(IAR_CPU_MAP_cortex-m4 "Cortex-M4")
set(IAR_CPU_MAP_cortex-m7 "Cortex-M7")
set(IAR_CPU_MAP_cortex-m23 "Cortex-M23")
set(IAR_CPU_MAP_cortex-m33 "Cortex-M33")
set(IAR_CPU_MAP_cortex-m35p "Cortex-M35P")
set(IAR_CPU_MAP_cortex-m55 "Cortex-M55")
set(IAR_CPU_MAP_cortex-m85 "Cortex-M85")

set(IAR_CPU_NAME "${IAR_CPU_MAP_${NEXUS_CPU_ARCH}}")
if(NOT IAR_CPU_NAME)
    # Fallback: capitalize first letter
    string(SUBSTRING "${NEXUS_CPU_ARCH}" 0 1 FIRST_CHAR)
    string(TOUPPER "${FIRST_CHAR}" FIRST_CHAR_UPPER)
    string(SUBSTRING "${NEXUS_CPU_ARCH}" 1 -1 REST_CHARS)
    set(IAR_CPU_NAME "${FIRST_CHAR_UPPER}${REST_CHARS}")
endif()

# Set default FPU configuration based on CPU if not specified
if(NOT DEFINED NEXUS_FPU_TYPE)
    if(NEXUS_CPU_ARCH MATCHES "cortex-m4")
        set(NEXUS_FPU_TYPE "fpv4-sp-d16")
    elseif(NEXUS_CPU_ARCH MATCHES "cortex-m7")
        set(NEXUS_FPU_TYPE "fpv5-d16")
    elseif(NEXUS_CPU_ARCH MATCHES "cortex-m33")
        set(NEXUS_FPU_TYPE "fpv5-sp-d16")
    else()
        set(NEXUS_FPU_TYPE "")
    endif()
endif()

# Map FPU type to IAR FPU name
set(IAR_FPU_MAP_fpv4-sp-d16 "VFPv4_sp")
set(IAR_FPU_MAP_fpv5-sp-d16 "VFPv5_sp")
set(IAR_FPU_MAP_fpv5-d16 "VFPv5_d16")

set(IAR_FPU_NAME "${IAR_FPU_MAP_${NEXUS_FPU_TYPE}}")

# Build CPU flags based on configuration
set(CPU_FLAGS "--cpu=${IAR_CPU_NAME}")

# Add FPU flags if specified
if(IAR_FPU_NAME)
    set(CPU_FLAGS "${CPU_FLAGS} --fpu=${IAR_FPU_NAME}")
endif()

# Initialize compiler flags
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} --endian=little --dlib_config normal")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} --endian=little --dlib_config normal")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

# Cache IAR_CPU_NAME for use in linker configuration
set(NEXUS_IAR_CPU_NAME "${IAR_CPU_NAME}" CACHE INTERNAL "IAR CPU name")
set(NEXUS_IAR_FPU_NAME "${IAR_FPU_NAME}" CACHE INTERNAL "IAR FPU name")

#-----------------------------------------------------------------------------
# Build Type Specific Flags
#-----------------------------------------------------------------------------
# Note: Build type flags are now managed centrally by NexusCompilerConfig.cmake
# This provides better consistency and allows easier customization.
#
# IAR-specific optimization flags:
#   -On:  No optimization
#   -Om:  Medium optimization
#   -Oh:  High optimization for speed
#   -Ohz: High optimization for size
#   -r:   Generate debug information

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

# Build base linker flags with CPU configuration
set(LINKER_FLAGS "--cpu=${IAR_CPU_NAME}")

# Add FPU to linker if specified
if(IAR_FPU_NAME)
    set(LINKER_FLAGS "${LINKER_FLAGS} --fpu=${IAR_FPU_NAME}")
endif()

# Add linker options
# Note: --config option will be added per-target with specific ICF file
# Note: --map option will be added per-target with specific filename
set(LINKER_FLAGS "${LINKER_FLAGS} --semihosting --entry __iar_program_start")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${LINKER_FLAGS}")

#-----------------------------------------------------------------------------
# Cross-Compilation Settings
#-----------------------------------------------------------------------------

# Don't try to compile test programs (cross-compiling)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths for find_* commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#-----------------------------------------------------------------------------
# IAR-Specific Settings
#-----------------------------------------------------------------------------

# Disable compiler warnings about unknown pragmas
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --diag_suppress=Pe1665")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --diag_suppress=Pe1665")

# Enable C99 mode
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --c99")

#-----------------------------------------------------------------------------
# Toolchain Information
#-----------------------------------------------------------------------------

message(STATUS "IAR ARM Toolchain Configuration:")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
message(STATUS "  Linker:     ${CMAKE_LINKER}")
message(STATUS "  Archiver:   ${CMAKE_AR}")
message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH} (IAR: ${IAR_CPU_NAME})")
message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE} (IAR: ${IAR_FPU_NAME})")
message(STATUS "  C Flags:    ${CMAKE_C_FLAGS_INIT}")
message(STATUS "  ASM Flags:  ${CMAKE_ASM_FLAGS_INIT}")
message(STATUS "  Link Flags: ${LINKER_FLAGS}")

#-----------------------------------------------------------------------------
# End of iar-arm.cmake
#-----------------------------------------------------------------------------
