##############################################################################
# native.cmake - Native Platform Toolchain Configuration
##############################################################################
#
# native.cmake
# Native platform toolchain configuration for Nexus build system
# Author: Nexus Team
#
# This toolchain file configures native platform compilers (MSVC, GCC, Clang)
# for PC-based development and testing. Supports Windows (MSVC), Linux
# (GCC/Clang), and macOS (Clang/AppleClang) for native platform builds.
#
# Validates: Requirements 4.3
#
##############################################################################

# Native platform - no cross-compilation
set(CMAKE_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})

##############################################################################
# Toolchain Detection
##############################################################################

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

##############################################################################
# Build Type Specific Flags
##############################################################################

if(MSVC)
    # MSVC-specific flags
    # Debug build flags
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /RTC1 /DDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /DDEBUG" CACHE STRING "" FORCE)

    # Release build flags
    set(CMAKE_C_FLAGS_RELEASE "/O2 /Ob2 /Oi /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /Oi /DNDEBUG" CACHE STRING "" FORCE)

    # Minimum size release flags
    set(CMAKE_C_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_MINSIZEREL "/O1 /Ob1 /DNDEBUG" CACHE STRING "" FORCE)

    # Release with debug info flags
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG" CACHE STRING "" FORCE)

else()
    # GCC/Clang-specific flags
    # Debug build flags
    set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -DDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3 -DDEBUG" CACHE STRING "" FORCE)

    # Release build flags
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)

    # Minimum size release flags
    set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG" CACHE STRING "" FORCE)

    # Release with debug info flags
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "" FORCE)
endif()

##############################################################################
# Platform-Specific Settings
##############################################################################

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

##############################################################################
# Toolchain Adapter Functions
##############################################################################

#
# Configure target for native platform
# TARGET: Target name
#
function(nexus_configure_native_target TARGET)
    # No special configuration needed for native targets
    # Standard CMake target configuration applies
endfunction()

##############################################################################
# Toolchain Information
##############################################################################

message(STATUS "Native Platform Toolchain Configuration:")
message(STATUS "  System:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Processor:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler:   ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Toolchain:  ${NEXUS_TOOLCHAIN_NAME}")
message(STATUS "  Vendor:     ${NEXUS_TOOLCHAIN_VENDOR}")

##############################################################################
# End of native.cmake
##############################################################################
