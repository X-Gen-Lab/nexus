#-----------------------------------------------------------------------------
# arm-gcc.cmake - ARM GCC Toolchain Configuration
#-----------------------------------------------------------------------------
# ARM GCC toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures ARM GCC (arm-none-eabi-gcc) compiler for
# ARM Cortex-M microcontrollers. ARM GCC is a free and open-source toolchain
# with excellent community support and wide adoption in embedded development.
#
# Requires: CMake 3.21+
# Validates: Requirements 4.3
#-----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Generic)

#-----------------------------------------------------------------------------
# CPU Architecture Configuration (must be set before project())
#-----------------------------------------------------------------------------

# NEXUS_CPU_ARCH, NEXUS_FPU_TYPE, and NEXUS_FLOAT_ABI are set by Kconfig
# via NexusConfig.cmake before the toolchain file is loaded.
# These values come from CONFIG_CPU_ARCH, CONFIG_FPU_TYPE, CONFIG_FLOAT_ABI.

# CPU flags are set explicitly in CMAKE_C_FLAGS_INIT
# Do NOT set CMAKE_SYSTEM_PROCESSOR here to avoid CMake auto-detection issues

#-----------------------------------------------------------------------------
# Toolchain Programs
#-----------------------------------------------------------------------------

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Find toolchain programs
find_program(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_AR ${TOOLCHAIN_PREFIX}ar)
find_program(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
find_program(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}objdump)
find_program(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)
find_program(CMAKE_GDB ${TOOLCHAIN_PREFIX}gdb)

# Verify compiler was found
if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "arm-none-eabi-gcc compiler not found.\n"
        "Please install ARM GCC toolchain and add it to PATH.\n"
        "\n"
        "Installation instructions:\n"
        "  - Windows: Download from https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm\n"
        "  - Linux:   sudo apt install gcc-arm-none-eabi\n"
        "  - macOS:   brew install --cask gcc-arm-embedded\n"
    )
endif()

# Set toolchain identification
set(NEXUS_TOOLCHAIN_NAME "arm-gcc")
set(NEXUS_TOOLCHAIN_FAMILY "gcc")
set(NEXUS_TOOLCHAIN_VENDOR "ARM")

#-----------------------------------------------------------------------------
# CPU Architecture Flags Configuration
#-----------------------------------------------------------------------------

# Configure CPU flags based on target platform
# Supports Cortex-M0/M0+/M3/M4/M7/M33 with optional FPU

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

if(NOT DEFINED NEXUS_FLOAT_ABI)
    if(NEXUS_FPU_TYPE)
        set(NEXUS_FLOAT_ABI "hard")
    else()
        set(NEXUS_FLOAT_ABI "soft")
    endif()
endif()

# Build CPU flags based on configuration
set(CPU_FLAGS "-mcpu=${NEXUS_CPU_ARCH} -mthumb")

# Add FPU flags if specified
if(NEXUS_FPU_TYPE)
    set(CPU_FLAGS "${CPU_FLAGS} -mfpu=${NEXUS_FPU_TYPE} -mfloat-abi=${NEXUS_FLOAT_ABI}")
endif()

# Initialize compiler flags with CPU architecture
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

#-----------------------------------------------------------------------------
# Build Type Specific Flags
#-----------------------------------------------------------------------------
# Note: Build type flags are now managed centrally by NexusCompilerConfig.cmake
# This provides better consistency and allows easier customization.
# The flags are set in the main CMakeLists.txt after toolchain detection.
#
# To customize build type flags, use one of these methods:
#   1. Command line: cmake -DCMAKE_C_FLAGS_DEBUG="-O0 -g3"
#   2. CMakePresets.json: "CMAKE_C_FLAGS_DEBUG": "-O0 -g3"
#   3. Before project(): set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "")

#-----------------------------------------------------------------------------
# Linker Flags
#-----------------------------------------------------------------------------

# Build base linker flags with CPU configuration
set(LINKER_FLAGS "-specs=nano.specs -specs=nosys.specs")

# Add garbage collection and memory usage reporting
set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--gc-sections -Wl,--print-memory-usage")

# Note: --map option will be added per-target with specific filename
# Note: Additional linker options controlled via Kconfig

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

message(STATUS "ARM GCC Toolchain Configuration:")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
message(STATUS "  Linker:     ${CMAKE_C_COMPILER}")
message(STATUS "  Archiver:   ${CMAKE_AR}")
message(STATUS "  Objcopy:    ${CMAKE_OBJCOPY}")
message(STATUS "  Size:       ${CMAKE_SIZE}")
message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH}")
message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE}")
message(STATUS "  Float ABI:  ${NEXUS_FLOAT_ABI}")
message(STATUS "  C Flags:    ${CPU_FLAGS}")
message(STATUS "  ASM Flags:  ${CMAKE_ASM_FLAGS_INIT}")
message(STATUS "  Link Flags: ${LINKER_FLAGS}")

#-----------------------------------------------------------------------------
# End of arm-gcc.cmake
#-----------------------------------------------------------------------------
