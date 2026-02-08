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

# Detect compiler automatically
# CMake will use the default system compiler

# Set toolchain identification
if(MSVC)
    set(NEXUS_TOOLCHAIN_NAME "msvc")
    set(NEXUS_TOOLCHAIN_FAMILY "msvc")
    set(NEXUS_TOOLCHAIN_VENDOR "Microsoft")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(NEXUS_TOOLCHAIN_NAME "gcc")
    set(NEXUS_TOOLCHAIN_FAMILY "gcc")
    set(NEXUS_TOOLCHAIN_VENDOR "GNU")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(NEXUS_TOOLCHAIN_NAME "clang")
    set(NEXUS_TOOLCHAIN_FAMILY "clang")
    if(APPLE)
        set(NEXUS_TOOLCHAIN_VENDOR "Apple")
    else()
        set(NEXUS_TOOLCHAIN_VENDOR "LLVM")
    endif()
else()
    set(NEXUS_TOOLCHAIN_NAME "unknown")
    set(NEXUS_TOOLCHAIN_FAMILY "unknown")
    set(NEXUS_TOOLCHAIN_VENDOR "Unknown")
endif()

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
# Platform-Specific Settings
#-----------------------------------------------------------------------------

if(WIN32)
    # Windows-specific settings
    if(MSVC)
        # Enable multi-processor compilation
        add_compile_options(/MP)
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    # Enable position independent code
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
elseif(APPLE)
    # macOS-specific settings
    set(CMAKE_MACOSX_RPATH ON)
endif()

#-----------------------------------------------------------------------------
# Toolchain Adapter Functions
#-----------------------------------------------------------------------------

# Configure target for native platform
# Arguments:
#   TARGET: Target name
function(nexus_configure_native_target TARGET)
    # No special configuration needed for native targets
    # Standard CMake target configuration applies
endfunction()

#-----------------------------------------------------------------------------
# Toolchain Information
#-----------------------------------------------------------------------------

message(STATUS "Native Platform Toolchain Configuration:")
message(STATUS "  System:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Processor:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Toolchain:  ${NEXUS_TOOLCHAIN_NAME}")
message(STATUS "  Vendor:     ${NEXUS_TOOLCHAIN_VENDOR}")

#-----------------------------------------------------------------------------
# End of native.cmake
#-----------------------------------------------------------------------------
