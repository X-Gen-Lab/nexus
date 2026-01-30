##############################################################################
# armclang.cmake - ARM Clang Toolchain Configuration
##############################################################################
#
# armclang.cmake
# ARM Clang toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures ARM Clang (armclang) compiler for ARM
# Cortex-M microcontrollers. ARM Clang is the official ARM compiler toolchain
# with excellent optimization and code generation capabilities.
#
# Validates: Requirements 4.3
#
##############################################################################

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

##############################################################################
# Toolchain Programs
##############################################################################

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

##############################################################################
# CPU Architecture Configuration
##############################################################################

#
# Configure CPU flags based on target platform
# Supports Cortex-M0/M0+/M3/M4/M7/M33 with optional FPU
#

# Default to Cortex-M4 with FPU if not specified
if(NOT DEFINED NEXUS_CPU_ARCH)
    set(NEXUS_CPU_ARCH "cortex-m4")
endif()

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
set(CMAKE_ASM_FLAGS_INIT "")

##############################################################################
# Build Type Specific Flags
##############################################################################

# Debug build flags
set(CMAKE_C_FLAGS_DEBUG "-O1 -g -DDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "-O1 -g -DDEBUG" CACHE STRING "" FORCE)

# Release build flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG" CACHE STRING "" FORCE)

# Minimum size release flags
set(CMAKE_C_FLAGS_MINSIZEREL "-Oz -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Oz -DNDEBUG" CACHE STRING "" FORCE)

# Release with debug info flags
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)

##############################################################################
# Linker Flags
##############################################################################

# Initialize linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--strict --summary_stderr --info summarysizes --map --load_addr_map_info --xref --callgraph --symbols"
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
# Toolchain Adapter Functions
##############################################################################

#
# Generate binary files from ELF
# TARGET: Target name
#
function(nexus_generate_binary TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --bin --output $<TARGET_FILE:${TARGET}> ${TARGET}.bin
        COMMAND ${CMAKE_OBJCOPY} --i32 --output $<TARGET_FILE:${TARGET}> ${TARGET}.hex
        COMMENT "Generating ${TARGET}.bin and ${TARGET}.hex"
    )
endfunction()

#
# Print section sizes
# TARGET: Target name
#
function(nexus_print_size TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --info=sizes,totals $<TARGET_FILE:${TARGET}>
        COMMENT "Size of ${TARGET}:"
    )
endfunction()

#
# Generate disassembly listing
# TARGET: Target name
#
function(nexus_generate_listing TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --text -c --output ${TARGET}.lst $<TARGET_FILE:${TARGET}>
        COMMENT "Generating ${TARGET}.lst"
    )
endfunction()

#
# Configure target for ARM Clang toolchain
# TARGET: Target name
# LINKER_SCRIPT: Path to linker script (scatter file)
#
function(nexus_configure_arm_target TARGET)
    # Parse arguments
    cmake_parse_arguments(ARG "" "LINKER_SCRIPT" "" ${ARGN})

    # Set linker script if provided
    if(ARG_LINKER_SCRIPT)
        target_link_options(${TARGET} PRIVATE
            --scatter=${ARG_LINKER_SCRIPT}
        )
    endif()

    # Generate binary and hex files
    nexus_generate_binary(${TARGET})
    nexus_print_size(${TARGET})
endfunction()

##############################################################################
# Toolchain Information
##############################################################################

message(STATUS "ARM Clang Toolchain Configuration:")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Assembler:  ${CMAKE_ASM_COMPILER}")
message(STATUS "  Linker:     ${CMAKE_LINKER}")
message(STATUS "  Archiver:   ${CMAKE_AR}")
message(STATUS "  CPU Arch:   ${NEXUS_CPU_ARCH}")
message(STATUS "  FPU Type:   ${NEXUS_FPU_TYPE}")
message(STATUS "  Float ABI:  ${NEXUS_FLOAT_ABI}")
message(STATUS "  CPU Flags:  ${CPU_FLAGS}")

##############################################################################
# End of armclang.cmake
##############################################################################
