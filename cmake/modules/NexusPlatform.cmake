##############################################################################
# Nexus Platform Detection and Configuration
#
# This module provides platform detection and configuration for the Nexus
# build system. It detects the host platform, compiler, and generator, and
# configures appropriate compiler flags and build settings.
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Platform Detection
##############################################################################

# Detect host platform (Windows, Linux, macOS)
function(nexus_detect_platform)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(NEXUS_HOST_WINDOWS TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "Windows" PARENT_SCOPE)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(NEXUS_HOST_LINUX TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "Linux" PARENT_SCOPE)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(NEXUS_HOST_MACOS TRUE PARENT_SCOPE)
        set(NEXUS_HOST_PLATFORM "macOS" PARENT_SCOPE)
    else()
        set(NEXUS_HOST_PLATFORM "Unknown" PARENT_SCOPE)
    endif()
endfunction()

# Detect compiler (MSVC, GCC, Clang, AppleClang)
function(nexus_detect_compiler)
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        set(NEXUS_COMPILER_MSVC TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "MSVC" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(NEXUS_COMPILER_GCC TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "GCC" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(NEXUS_COMPILER_CLANG TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "Clang" PARENT_SCOPE)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        set(NEXUS_COMPILER_APPLECLANG TRUE PARENT_SCOPE)
        set(NEXUS_COMPILER_NAME "AppleClang" PARENT_SCOPE)
    else()
        set(NEXUS_COMPILER_NAME "Unknown" PARENT_SCOPE)
    endif()

    set(NEXUS_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}" PARENT_SCOPE)
endfunction()

# Detect CMake generator (Visual Studio, Ninja, Make)
function(nexus_detect_generator)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(NEXUS_GENERATOR_VS TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Visual Studio" PARENT_SCOPE)
    elseif(CMAKE_GENERATOR STREQUAL "Ninja")
        set(NEXUS_GENERATOR_NINJA TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Ninja" PARENT_SCOPE)
    elseif(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
        set(NEXUS_GENERATOR_MAKE TRUE PARENT_SCOPE)
        set(NEXUS_GENERATOR_NAME "Make" PARENT_SCOPE)
    else()
        set(NEXUS_GENERATOR_NAME "${CMAKE_GENERATOR}" PARENT_SCOPE)
    endif()
endfunction()

##############################################################################
# Platform Configuration
##############################################################################

# Configure platform-specific settings
macro(nexus_configure_platform)
    # Detect platform, compiler, and generator
    nexus_detect_platform()
    nexus_detect_compiler()
    nexus_detect_generator()

    # Print configuration
    message(STATUS "")
    message(STATUS "=== Platform Configuration ===")
    message(STATUS "  Host Platform: ${NEXUS_HOST_PLATFORM}")
    message(STATUS "  Compiler:      ${NEXUS_COMPILER_NAME} ${NEXUS_COMPILER_VERSION}")
    message(STATUS "  Generator:     ${NEXUS_GENERATOR_NAME}")
    message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
    message(STATUS "===============================")
    message(STATUS "")

    # Set platform-specific defaults
    if(NEXUS_HOST_WINDOWS)
        # Windows-specific settings
        if(NEXUS_COMPILER_MSVC)
            # Enable multi-processor compilation
            add_compile_options(/MP)

            # Set runtime library
            if(BUILD_SHARED_LIBS)
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            else()
                set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            endif()
        endif()
    elseif(NEXUS_HOST_LINUX)
        # Linux-specific settings
        # Enable position independent code for shared libraries
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    elseif(NEXUS_HOST_MACOS)
        # macOS-specific settings
        set(CMAKE_MACOSX_RPATH ON)
    endif()
endmacro()

##############################################################################
# Compiler Configuration
##############################################################################

# Configure compiler flags for all supported compilers
macro(nexus_configure_compiler_flags)
    if(NEXUS_COMPILER_MSVC)
        # MSVC compiler flags
        add_compile_options(
            /W4                 # Warning level 4
            /WX                 # Treat warnings as errors
            /wd4100             # Unreferenced formal parameter
            /wd4996             # Deprecated functions
            /permissive-        # Standards conformance
            /Zc:__cplusplus     # Enable __cplusplus macro
        )

        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:/Od>)
        add_compile_options($<$<CONFIG:Debug>:/Zi>)
        add_compile_options($<$<CONFIG:Debug>:/RTC1>)

        # Release configuration
        add_compile_options($<$<CONFIG:Release>:/O2>)
        add_compile_options($<$<CONFIG:Release>:/Ob2>)
        add_compile_options($<$<CONFIG:Release>:/Oi>)

        # Preprocessor definitions
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Debug>:_DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
        )

    elseif(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
        # GCC/Clang compiler flags
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Wno-unused-parameter
            -ffunction-sections
            -fdata-sections
        )

        # Debug configuration
        add_compile_options($<$<CONFIG:Debug>:-Og>)
        add_compile_options($<$<CONFIG:Debug>:-g3>)

        # Release configuration
        add_compile_options($<$<CONFIG:Release>:-O2>)

        # Preprocessor definitions
        add_compile_definitions(
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )

        # Linker flags
        add_link_options(
            -Wl,--gc-sections
        )

        # Coverage flags
        if(NEXUS_ENABLE_COVERAGE)
            add_compile_options(--coverage -O0 -g)
            add_link_options(--coverage)

            if(NEXUS_COMPILER_GCC)
                add_compile_options(-fprofile-arcs -ftest-coverage)
            endif()

            # Disable unused warnings for coverage build
            add_compile_options(
                -Wno-unused-function
                -Wno-unused-variable
                -Wno-missing-field-initializers
            )
        endif()
    endif()
endmacro()

##############################################################################
# Build Type Configuration
##############################################################################

# Set default build type if not specified
macro(nexus_set_default_build_type)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
            "Choose the type of build (Debug, Release, MinSizeRel, RelWithDebInfo)"
            FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    endif()
endmacro()

##############################################################################
# Output Directory Configuration
##############################################################################

# Configure output directories for build artifacts
macro(nexus_configure_output_directories)
    # Set output directories for single-configuration generators
    if(NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    else()
        # Multi-configuration generators (Visual Studio, Xcode)
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin/${CONFIG})
            set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
            set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib/${CONFIG})
        endforeach()
    endif()
endmacro()
