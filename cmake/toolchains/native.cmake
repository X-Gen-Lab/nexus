#-----------------------------------------------------------------------------
# native.cmake - Native Platform Toolchain Configuration
#-----------------------------------------------------------------------------
# Native platform toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures native platform compilers (MSVC, GCC, Clang)
# for PC-based development and testing. Supports Windows (MSVC), Linux
# (GCC/Clang), and macOS (Clang/AppleClang) for native platform builds.
#
# Requires: CMake 3.21+
# Validates: Requirements 4.3
#-----------------------------------------------------------------------------

# Native platform - no cross-compilation
set(CMAKE_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})

#-----------------------------------------------------------------------------
# Toolchain Detection
#-----------------------------------------------------------------------------

# Note: Toolchain identification is set after project() in main CMakeLists.txt
# because MSVC and compiler ID variables are not available in toolchain files.
# This file only sets up the basic system configuration for native builds.

#-----------------------------------------------------------------------------
# Build Type Specific Flags
#-----------------------------------------------------------------------------
# Note: Build type flags are now managed centrally by NexusCompilerConfig.cmake
# This provides better consistency and allows easier customization.
#
# To customize build type flags, use one of these methods:
#   1. Command line: cmake -DCMAKE_C_FLAGS_DEBUG="-O0 -g3"
#   2. CMakePresets.json: "CMAKE_C_FLAGS_DEBUG": "-O0 -g3"
#   3. Before project(): set(CMAKE_C_FLAGS_DEBUG "-O0 -g3" CACHE STRING "")

#-----------------------------------------------------------------------------
# Toolchain Information
#-----------------------------------------------------------------------------

message(STATUS "Native Platform Toolchain Configuration:")
message(STATUS "  System:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Processor:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Note: Compiler detection happens after project() call")

#-----------------------------------------------------------------------------
# End of native.cmake
#-----------------------------------------------------------------------------
