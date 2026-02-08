#-----------------------------------------------------------------------------
# armclang.cmake - ARM Clang Toolchain Configuration
#-----------------------------------------------------------------------------
# ARM Clang toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures ARM Clang (armclang) compiler for ARM
# Cortex-M microcontrollers. ARM Clang is the official ARM compiler toolchain
# with excellent optimization and code generation capabilities.
#
# Requires: CMake 3.21+ for proper ARM Clang support
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
# Do NOT set CMAKE_SYSTEM_PROCESSOR or CMAKE_SYSTEM_ARCH
# (CMake 3.21+ handles this correctly without those variables)

#-----------------------------------------------------------------------------
# Toolchain Programs
#-----------------------------------------------------------------------------

# Find ARM Clang toolchain programs
find_program(CMAKE_C_COMPILER armclang)
find_program(CMAKE_CXX_COMPILER armclang)
find_program(CMAKE_ASM_COMPILER armasm)
find_program(CMAKE_AR armar)
find_program(CMAKE_LINKER armlink)
find_program(CMAKE_OBJCOPY fromelf)

# Verify compiler was found
if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "armclang compiler not found.\n"
        "Please install ARM Compiler 6 and add it to PATH.\n"
        "\n"
        "Installation instructions:\n"
        "  - Download from: https://developer.arm.com/tools-and-software/embedded/arm-compiler\n"
        "  - Requires license from ARM\n"
    )
endif()

# Set toolchain identification
set(NEXUS_TOOLCHAIN_NAME "armclang")
set(NEXUS_TOOLCHAIN_FAMILY "clang")
set(NEXUS_TOOLCHAIN_VENDOR "ARM")

#-----------------------------------------------------------------------------
# CPU Architecture Flags Configuration
#-----------------------------------------------------------------------------

# Configure CPU flags based on target platform
# Supports Cortex-M0/M0+/M3/M4/M7/M33 with optional FPU

if(NOT DEFINED NEXUS_FPU_TYPE)
    set(NEXUS_FPU_TYPE "fpv4-sp-d16")
endif()

if(NOT DEFINED NEXUS_FLOAT_ABI)
    set(NEXUS_FLOAT_ABI "hard")
endif()

# Build CPU flags based on configuration
set(CPU_FLAGS "--target=arm-arm-none-eabi -mcpu=${NEXUS_CPU_ARCH} -mthumb")

# Add FPU flags if specified
if(NEXUS_FPU_TYPE)
    set(CPU_FLAGS "${CPU_FLAGS} -mfpu=${NEXUS_FPU_TYPE} -mfloat-abi=${NEXUS_FLOAT_ABI}")
endif()

# Initialize compiler flags
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")

#-----------------------------------------------------------------------------
# Assembler Flags Configuration
#-----------------------------------------------------------------------------

# ARM assembler (armasm) uses different syntax than armclang
# Map CPU architecture to assembler CPU name
set(ASM_CPU_MAP_cortex-m0 "Cortex-M0")
set(ASM_CPU_MAP_cortex-m0plus "Cortex-M0plus")
set(ASM_CPU_MAP_cortex-m1 "Cortex-M1")
set(ASM_CPU_MAP_cortex-m3 "Cortex-M3")
set(ASM_CPU_MAP_cortex-m4 "Cortex-M4")
set(ASM_CPU_MAP_cortex-m7 "Cortex-M7")
set(ASM_CPU_MAP_cortex-m23 "Cortex-M23")
set(ASM_CPU_MAP_cortex-m33 "Cortex-M33")
set(ASM_CPU_MAP_cortex-m35p "Cortex-M35P")
set(ASM_CPU_MAP_cortex-m55 "Cortex-M55")
set(ASM_CPU_MAP_cortex-m85 "Cortex-M85")

set(ASM_CPU_NAME "${ASM_CPU_MAP_${NEXUS_CPU_ARCH}}")
if(NOT ASM_CPU_NAME)
    set(ASM_CPU_NAME "${NEXUS_CPU_ARCH}")
endif()

# Map FPU type to assembler FPU name
set(ASM_FPU_MAP_fpv4-sp-d16 "FPv4-SP")
set(ASM_FPU_MAP_fpv5-d16 "FPv5_D16")
set(ASM_FPU_MAP_fpv5-sp-d16 "FPv5-SP")

set(ASM_FPU_NAME "${ASM_FPU_MAP_${NEXUS_FPU_TYPE}}")

# Build assembler flags
set(ASM_FLAGS "--cpu=${ASM_CPU_NAME}")

# Add FPU to assembler if specified
if(ASM_FPU_NAME)
    set(ASM_FLAGS "${ASM_FLAGS} --fpu=${ASM_FPU_NAME}")
endif()

set(CMAKE_ASM_FLAGS_INIT "${ASM_FLAGS}")

# Cache ASM_CPU_NAME for use in linker configuration
set(NEXUS_ASM_CPU_NAME "${ASM_CPU_NAME}" CACHE INTERNAL "Assembler CPU name")

#-----------------------------------------------------------------------------
# Build Type Specific Flags
#-----------------------------------------------------------------------------
# Note: Build type flags are now managed centrally by NexusCompilerFlags.cmake
# See cmake/modules/NexusCompilerFlags.cmake::nexus_set_default_build_type_flags()
#
# ARM Clang uses slightly different optimization levels than GCC:
#   Debug:   -O1 (better debugging experience than -O0)
#   Release: -O3 (maximum optimization)
#   MinSize: -Oz (optimize for size)

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

# Build base linker flags with CPU configuration
set(LINKER_FLAGS "--cpu=${ASM_CPU_NAME}")

# Add FPU to linker if specified
if(ASM_FPU_NAME)
    set(LINKER_FLAGS "${LINKER_FLAGS} --fpu=${ASM_FPU_NAME}")
endif()

# Add linker options
# Note: --map option will be added per-target with specific filename
# Note: --info, --xref, --callgraph, --symbols controlled via Kconfig
set(LINKER_FLAGS "${LINKER_FLAGS} --strict --summary_stderr")

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
# Toolchain Information
#-----------------------------------------------------------------------------

message(STATUS "ARM Clang Toolchain Configuration:")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
message(STATUS "  Linker:     ${CMAKE_LINKER}")
message(STATUS "  Archiver:   ${CMAKE_AR}")
message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH}")
message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE}")
message(STATUS "  Float ABI:  ${NEXUS_FLOAT_ABI}")
message(STATUS "  C Flags:    ${CPU_FLAGS}")
message(STATUS "  ASM Flags:  ${ASM_FLAGS}")
message(STATUS "  Link Flags: ${LINKER_FLAGS}")

#-----------------------------------------------------------------------------
# End of armclang.cmake
#-----------------------------------------------------------------------------
