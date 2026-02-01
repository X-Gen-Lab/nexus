##############################################################################
# NexusOptimization.cmake - Build Optimization Module
##############################################################################
#
# NexusOptimization.cmake
# Cache management, incremental builds, performance optimization, and resource management
# Author: Nexus Team
#
# This module provides:
# - Build cache management (ccache/sccache)
# - Incremental build with dependency tracking
# - Performance optimization and parallel scheduling
# - Resource management and monitoring
# - Temporary file cleanup
#
# Consolidated from:
# - NexusCache.cmake
# - NexusIncremental.cmake
# - NexusPerformance.cmake
# - NexusResource.cmake
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Cache Management
##############################################################################

set(NEXUS_CACHE_ENABLED FALSE CACHE BOOL "Enable build cache")
set(NEXUS_CACHE_LOCAL_DIR "${CMAKE_BINARY_DIR}/.cache" CACHE PATH "Local cache directory")
set(NEXUS_CACHE_MAX_SIZE 1024 CACHE STRING "Maximum cache size in MB")

#
# Enable and configure build cache
#
function(nexus_enable_cache)
    cmake_parse_arguments(
        ARG
        ""
        "LOCAL_DIR;MAX_SIZE;REMOTE_URL"
        ""
        ${ARGN}
    )

    if(ARG_LOCAL_DIR)
        set(NEXUS_CACHE_LOCAL_DIR ${ARG_LOCAL_DIR} CACHE PATH "Local cache directory" FORCE)
    endif()

    if(ARG_MAX_SIZE)
        set(NEXUS_CACHE_MAX_SIZE ${ARG_MAX_SIZE} CACHE STRING "Maximum cache size in MB" FORCE)
    endif()

    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR})

    find_program(CCACHE_PROGRAM ccache)
    find_program(SCCACHE_PROGRAM sccache)

    if(CCACHE_PROGRAM)
        message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)

        # Enhanced ccache configuration
        set(ENV{CCACHE_DIR} ${NEXUS_CACHE_LOCAL_DIR})
        set(ENV{CCACHE_MAXSIZE} "${NEXUS_CACHE_MAX_SIZE}M")
        set(ENV{CCACHE_COMPRESS} "true")
        set(ENV{CCACHE_COMPRESSLEVEL} "6")  # Balance compression ratio and speed
        set(ENV{CCACHE_SLOPPINESS} "pch_defines,time_macros")

        # Remote cache support (optional)
        if(ARG_REMOTE_URL)
            set(ENV{CCACHE_REMOTE_STORAGE} ${ARG_REMOTE_URL})
        endif()

        message(STATUS "ccache configuration:")
        message(STATUS "  Directory: ${NEXUS_CACHE_LOCAL_DIR}")
        message(STATUS "  Max size:  ${NEXUS_CACHE_MAX_SIZE} MB")
        if(ARG_REMOTE_URL)
            message(STATUS "  Remote:    ${ARG_REMOTE_URL}")
        endif()

    elseif(SCCACHE_PROGRAM)
        message(STATUS "Found sccache: ${SCCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)

        set(ENV{SCCACHE_DIR} ${NEXUS_CACHE_LOCAL_DIR})
        set(ENV{SCCACHE_CACHE_SIZE} "${NEXUS_CACHE_MAX_SIZE}M")

        message(STATUS "sccache configuration:")
        message(STATUS "  Directory: ${NEXUS_CACHE_LOCAL_DIR}")
        message(STATUS "  Max size:  ${NEXUS_CACHE_MAX_SIZE} MB")

    else()
        message(WARNING
            "Build cache requested but ccache/sccache not found.\n"
            "Install ccache: sudo apt-get install ccache (Linux)\n"
            "                choco install ccache (Windows)\n"
            "                brew install ccache (macOS)")
        return()
    endif()

    set(NEXUS_CACHE_ENABLED TRUE CACHE BOOL "Enable build cache" FORCE)

    # Add cache statistics target
    add_custom_target(cache-stats
        COMMAND ${CMAKE_COMMAND} -E echo "=== Build Cache Statistics ==="
        COMMAND $<IF:$<BOOL:${CCACHE_PROGRAM}>,${CCACHE_PROGRAM} -s,${SCCACHE_PROGRAM} --show-stats>
        COMMENT "Displaying cache statistics"
    )

    # Add cache clear target
    add_custom_target(cache-clear
        COMMAND $<IF:$<BOOL:${CCACHE_PROGRAM}>,${CCACHE_PROGRAM} -C,${SCCACHE_PROGRAM} --stop-server>
        COMMENT "Clearing cache"
    )
endfunction()

#
# Display cache statistics
#
function(nexus_cache_stats)
    if(NOT NEXUS_CACHE_ENABLED)
        message(STATUS "Cache is not enabled")
        return()
    endif()

    message(STATUS "")
    message(STATUS "=== Build Cache Statistics ===")

    if(CCACHE_PROGRAM)
        execute_process(
            COMMAND ${CCACHE_PROGRAM} --show-stats
            OUTPUT_VARIABLE CACHE_STATS
            ERROR_QUIET
        )
        message(STATUS "${CACHE_STATS}")
    elseif(SCCACHE_PROGRAM)
        execute_process(
            COMMAND ${SCCACHE_PROGRAM} --show-stats
            OUTPUT_VARIABLE CACHE_STATS
            ERROR_QUIET
        )
        message(STATUS "${CACHE_STATS}")
    endif()

    message(STATUS "==============================")
    message(STATUS "")
endfunction()

##############################################################################
# Incremental Build
##############################################################################

set(NEXUS_DEP_DB_DIR "${CMAKE_BINARY_DIR}/.nexus/deps" CACHE PATH "Dependency tracking directory")
set(NEXUS_HASH_CACHE_DIR "${CMAKE_BINARY_DIR}/.nexus/hashes" CACHE PATH "Hash cache directory")

file(MAKE_DIRECTORY "${NEXUS_DEP_DB_DIR}")
file(MAKE_DIRECTORY "${NEXUS_HASH_CACHE_DIR}")

#
# Enable dependency tracking for target
#
function(nexus_enable_dependency_tracking TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "Target does not exist: ${TARGET}")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${TARGET} PRIVATE -MMD -MP)
    elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET} PRIVATE /showIncludes)
    endif()

    set_target_properties(${TARGET} PROPERTIES
        C_DEPENDENCY_FILE_GENERATION ON
        CXX_DEPENDENCY_FILE_GENERATION ON
    )

    set(TARGET_DEP_DIR "${NEXUS_DEP_DB_DIR}/${TARGET}")
    file(MAKE_DIRECTORY "${TARGET_DEP_DIR}")

    set_target_properties(${TARGET} PROPERTIES
        NEXUS_DEP_DIR "${TARGET_DEP_DIR}"
    )

    message(STATUS "Enabled dependency tracking for: ${TARGET}")
endfunction()

#
# Calculate SHA-256 hash of file
#
function(nexus_calculate_file_hash FILE_PATH HASH_VAR)
    if(NOT EXISTS "${FILE_PATH}")
        set(${HASH_VAR} "" PARENT_SCOPE)
        return()
    endif()

    file(SHA256 "${FILE_PATH}" FILE_HASH)
    set(${HASH_VAR} "${FILE_HASH}" PARENT_SCOPE)
endfunction()

#
# Check if file has changed using content hash
#
function(nexus_file_has_changed FILE_PATH CHANGED_VAR)
    if(NOT EXISTS "${FILE_PATH}")
        set(${CHANGED_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    nexus_calculate_file_hash("${FILE_PATH}" CURRENT_HASH)

    get_filename_component(ABS_PATH "${FILE_PATH}" ABSOLUTE)
    string(SHA256 PATH_HASH "${ABS_PATH}")
    set(CACHE_FILE "${NEXUS_HASH_CACHE_DIR}/${PATH_HASH}.txt")

    if(EXISTS "${CACHE_FILE}")
        file(READ "${CACHE_FILE}" CACHED_HASH)
        string(STRIP "${CACHED_HASH}" CACHED_HASH)
    else()
        set(CACHED_HASH "")
    endif()

    if(NOT CACHED_HASH OR NOT CURRENT_HASH STREQUAL CACHED_HASH)
        set(${CHANGED_VAR} TRUE PARENT_SCOPE)
        file(WRITE "${CACHE_FILE}" "${CURRENT_HASH}")
    else()
        set(${CHANGED_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

#
# Enable incremental build for target
#
function(nexus_enable_incremental_build TARGET)
    nexus_enable_dependency_tracking(${TARGET})

    set_target_properties(${TARGET} PROPERTIES
        NEXUS_INCREMENTAL_BUILD ON
    )

    message(STATUS "Enabled incremental build for: ${TARGET}")
endfunction()

#
# Enable incremental linking for target
#
function(nexus_enable_incremental_linking TARGET)
    get_target_property(TARGET_TYPE ${TARGET} TYPE)

    if(NOT TARGET_TYPE MATCHES "EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY")
        message(WARNING "Incremental linking only supported for executables and libraries")
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${TARGET} PRIVATE
            $<$<CONFIG:Debug>:-Wl,--incremental>
        )
    elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
        target_link_options(${TARGET} PRIVATE
            $<$<CONFIG:Debug>:/INCREMENTAL>
        )
    endif()

    set_target_properties(${TARGET} PROPERTIES
        NEXUS_INCREMENTAL_LINKING ON
    )

    message(STATUS "Enabled incremental linking for: ${TARGET}")
endfunction()

##############################################################################
# Performance Optimization
##############################################################################

set(NEXUS_BUILD_START_TIME 0 CACHE INTERNAL "Build start timestamp")
set(NEXUS_BUILD_TOTAL_FILES 0 CACHE INTERNAL "Total files to compile")
set(NEXUS_BUILD_COMPILED_FILES 0 CACHE INTERNAL "Number of compiled files")
set(NEXUS_PARALLEL_JOBS 0 CACHE STRING "Number of parallel compilation jobs")
set(NEXUS_ENABLE_PROGRESS ON CACHE BOOL "Enable build progress display")

#
# Configure parallel compilation with memory awareness
#
function(nexus_configure_parallel_build)
    # Get system resources
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    cmake_host_system_information(RESULT TOTAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)

    if(NOT CPU_CORES)
        set(CPU_CORES 1)
    endif()

    # Calculate based on CPU cores
    if(CPU_CORES GREATER 1)
        math(EXPR CPU_JOBS "${CPU_CORES} - 1")
    else()
        set(CPU_JOBS 1)
    endif()

    # Calculate based on available memory (assume 512MB per job)
    set(MEMORY_PER_JOB 512)  # MB
    if(AVAILABLE_MEMORY GREATER 0)
        math(EXPR MEMORY_JOBS "${AVAILABLE_MEMORY} / ${MEMORY_PER_JOB}")
    else()
        set(MEMORY_JOBS ${CPU_JOBS})
    endif()

    # Take the smaller value to avoid OOM
    if(MEMORY_JOBS LESS CPU_JOBS)
        set(OPTIMAL_JOBS ${MEMORY_JOBS})
        message(STATUS
            "Parallel jobs limited by memory: ${OPTIMAL_JOBS} "
            "(CPU would allow ${CPU_JOBS})")
    else()
        set(OPTIMAL_JOBS ${CPU_JOBS})
    endif()

    # Ensure at least 1 job
    if(OPTIMAL_JOBS LESS 1)
        set(OPTIMAL_JOBS 1)
    endif()

    # Set different parallelism for compile and link stages
    set(COMPILE_JOBS ${OPTIMAL_JOBS})
    math(EXPR LINK_JOBS "${OPTIMAL_JOBS} / 2")  # Linking is more memory-intensive
    if(LINK_JOBS LESS 1)
        set(LINK_JOBS 1)
    endif()

    if(NEXUS_PARALLEL_JOBS EQUAL 0)
        set(NEXUS_PARALLEL_JOBS ${OPTIMAL_JOBS} CACHE STRING "Number of parallel jobs" FORCE)
    endif()

    set(CMAKE_BUILD_PARALLEL_LEVEL ${NEXUS_PARALLEL_JOBS} PARENT_SCOPE)

    # Ninja-specific job pool configuration
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(CMAKE_JOB_POOLS "compile=${COMPILE_JOBS};link=${LINK_JOBS}" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_COMPILE "compile" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_LINK "link" PARENT_SCOPE)
    endif()

    message(STATUS "Parallel build configuration:")
    message(STATUS "  CPU cores:        ${CPU_CORES}")
    if(AVAILABLE_MEMORY GREATER 0)
        message(STATUS "  Available memory: ${AVAILABLE_MEMORY} MB")
    endif()
    message(STATUS "  Compile jobs:     ${COMPILE_JOBS}")
    message(STATUS "  Link jobs:        ${LINK_JOBS}")
endfunction()

#
# Initialize build progress tracking
#
function(nexus_init_progress)
    if(NOT NEXUS_ENABLE_PROGRESS)
        return()
    endif()

    string(TIMESTAMP START_TIME "%s")
    set(NEXUS_BUILD_START_TIME ${START_TIME} CACHE INTERNAL "Build start time")
    set(NEXUS_BUILD_COMPILED_FILES 0 CACHE INTERNAL "Compiled files")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/.nexus")

    message(STATUS "Build progress tracking initialized")
endfunction()

#
# Generate build statistics report
#
function(nexus_generate_statistics_report)
    string(TIMESTAMP END_TIME "%s")
    math(EXPR BUILD_TIME "${END_TIME} - ${NEXUS_BUILD_START_TIME}")

    set(REPORT_FILE "${CMAKE_BINARY_DIR}/.nexus/build_report.txt")
    file(WRITE ${REPORT_FILE} "")
    file(APPEND ${REPORT_FILE} "=================================================\n")
    file(APPEND ${REPORT_FILE} "Nexus Build Statistics Report\n")
    file(APPEND ${REPORT_FILE} "=================================================\n")
    file(APPEND ${REPORT_FILE} "\n")
    file(APPEND ${REPORT_FILE} "Build Time:        ${BUILD_TIME} seconds\n")
    file(APPEND ${REPORT_FILE} "Total Files:       ${NEXUS_BUILD_TOTAL_FILES}\n")
    file(APPEND ${REPORT_FILE} "Compiled Files:    ${NEXUS_BUILD_COMPILED_FILES}\n")
    file(APPEND ${REPORT_FILE} "Parallel Jobs:     ${NEXUS_PARALLEL_JOBS}\n")
    file(APPEND ${REPORT_FILE} "\n")
    file(APPEND ${REPORT_FILE} "=================================================\n")

    message(STATUS "Build statistics report generated: ${REPORT_FILE}")
    message(STATUS "")
    message(STATUS "Build completed in ${BUILD_TIME} seconds")
    message(STATUS "Compiled ${NEXUS_BUILD_COMPILED_FILES}/${NEXUS_BUILD_TOTAL_FILES} files")
endfunction()

##############################################################################
# Resource Management
##############################################################################

set(NEXUS_RESOURCE_ADAPTIVE_PARALLELISM ON CACHE BOOL "Enable adaptive parallelism")
set(NEXUS_RESOURCE_MEMORY_THRESHOLD 1024 CACHE STRING "Memory threshold in MB")
set(NEXUS_RESOURCE_TEMP_DIR "${CMAKE_BINARY_DIR}/.nexus/temp" CACHE STRING "Temporary files directory")
set(NEXUS_RESOURCE_AUTO_CLEANUP ON CACHE BOOL "Automatically cleanup temporary files")

#
# Detect system resources
#
function(nexus_resource_detect_system)
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    if(NOT CPU_CORES)
        set(CPU_CORES 1)
    endif()
    set(NEXUS_RESOURCE_CPU_CORES ${CPU_CORES} CACHE INTERNAL "Number of CPU cores")

    cmake_host_system_information(RESULT TOTAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
    if(NOT TOTAL_MEMORY)
        set(TOTAL_MEMORY 0)
    endif()
    set(NEXUS_RESOURCE_TOTAL_MEMORY ${TOTAL_MEMORY} CACHE INTERNAL "Total physical memory in MB")

    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)
    if(NOT AVAILABLE_MEMORY)
        set(AVAILABLE_MEMORY 0)
    endif()
    set(NEXUS_RESOURCE_AVAILABLE_MEMORY ${AVAILABLE_MEMORY} CACHE INTERNAL "Available physical memory in MB")

    message(VERBOSE "System resources detected:")
    message(VERBOSE "  CPU Cores: ${CPU_CORES}")
    message(VERBOSE "  Total Memory: ${TOTAL_MEMORY} MB")
    message(VERBOSE "  Available Memory: ${AVAILABLE_MEMORY} MB")
endfunction()

#
# Calculate optimal parallel jobs
#
function(nexus_resource_calculate_optimal_jobs OUT_VAR)
    nexus_resource_detect_system()

    set(CPU_CORES ${NEXUS_RESOURCE_CPU_CORES})
    if(CPU_CORES GREATER 1)
        math(EXPR OPTIMAL_JOBS "${CPU_CORES} - 1")
    else()
        set(OPTIMAL_JOBS 1)
    endif()

    set(MEMORY_PER_JOB 512)
    set(AVAILABLE_MEMORY ${NEXUS_RESOURCE_AVAILABLE_MEMORY})

    if(AVAILABLE_MEMORY GREATER 0)
        math(EXPR MEMORY_BASED_JOBS "${AVAILABLE_MEMORY} / ${MEMORY_PER_JOB}")

        if(MEMORY_BASED_JOBS LESS OPTIMAL_JOBS)
            set(OPTIMAL_JOBS ${MEMORY_BASED_JOBS})
            message(STATUS "Parallel jobs limited by available memory: ${OPTIMAL_JOBS}")
        endif()
    endif()

    if(OPTIMAL_JOBS LESS 1)
        set(OPTIMAL_JOBS 1)
    endif()

    set(${OUT_VAR} ${OPTIMAL_JOBS} PARENT_SCOPE)
endfunction()

#
# Initialize adaptive parallelism
#
function(nexus_resource_init_adaptive_parallelism)
    if(NOT NEXUS_RESOURCE_ADAPTIVE_PARALLELISM)
        message(STATUS "Adaptive parallelism disabled")
        return()
    endif()

    nexus_resource_detect_system()
    nexus_resource_calculate_optimal_jobs(OPTIMAL_JOBS)

    set(CMAKE_BUILD_PARALLEL_LEVEL ${OPTIMAL_JOBS} PARENT_SCOPE)

    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(CMAKE_JOB_POOLS "compile=${OPTIMAL_JOBS};link=1" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_COMPILE "compile" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_LINK "link" PARENT_SCOPE)
    endif()

    message(STATUS "Adaptive parallelism initialized: ${OPTIMAL_JOBS} jobs")
    message(STATUS "  CPU Cores: ${NEXUS_RESOURCE_CPU_CORES}")
    message(STATUS "  Available Memory: ${NEXUS_RESOURCE_AVAILABLE_MEMORY} MB")
endfunction()

#
# Create temporary file
#
function(nexus_resource_create_temp_file PREFIX SUFFIX OUT_VAR)
    file(MAKE_DIRECTORY ${NEXUS_RESOURCE_TEMP_DIR})

    string(TIMESTAMP TIMESTAMP "%Y%m%d_%H%M%S")
    string(RANDOM LENGTH 8 RANDOM_STR)
    set(TEMP_FILE "${NEXUS_RESOURCE_TEMP_DIR}/${PREFIX}_${TIMESTAMP}_${RANDOM_STR}${SUFFIX}")

    file(WRITE ${TEMP_FILE} "")

    set(${OUT_VAR} ${TEMP_FILE} PARENT_SCOPE)
endfunction()

#
# Clean up temporary files
#
function(nexus_resource_cleanup_temp_files)
    set(TEMP_PATTERNS
        "*.tmp"
        "*.temp"
        "*.cache"
        "*.log"
        "*.d"
    )

    set(FOUND_FILES "")

    foreach(PATTERN ${TEMP_PATTERNS})
        file(GLOB_RECURSE PATTERN_FILES "${CMAKE_BINARY_DIR}/${PATTERN}")
        list(APPEND FOUND_FILES ${PATTERN_FILES})
    endforeach()

    if(NOT FOUND_FILES)
        message(VERBOSE "No temporary files to clean up")
        return()
    endif()

    list(LENGTH FOUND_FILES FILE_COUNT)
    set(TOTAL_SIZE 0)

    foreach(FILE ${FOUND_FILES})
        if(EXISTS ${FILE})
            file(SIZE ${FILE} FILE_SIZE)
            math(EXPR TOTAL_SIZE "${TOTAL_SIZE} + ${FILE_SIZE}")
        endif()
    endforeach()

    math(EXPR TOTAL_SIZE_MB "${TOTAL_SIZE} / 1048576")

    set(DELETED_COUNT 0)
    foreach(FILE ${FOUND_FILES})
        if(EXISTS ${FILE})
            file(REMOVE ${FILE})
            math(EXPR DELETED_COUNT "${DELETED_COUNT} + 1")
        endif()
    endforeach()

    message(STATUS "Cleaned up ${DELETED_COUNT} temporary files (${TOTAL_SIZE_MB} MB)")
endfunction()

#
# Initialize resource management
#
function(nexus_resource_init)
    message(STATUS "Initializing Nexus Resource Management...")

    nexus_resource_init_adaptive_parallelism()

    file(MAKE_DIRECTORY ${NEXUS_RESOURCE_TEMP_DIR})

    message(STATUS "Nexus Resource Management initialized successfully")
endfunction()

#
# Finalize resource management
#
function(nexus_resource_finalize)
    message(STATUS "Finalizing Nexus Resource Management...")

    if(NEXUS_RESOURCE_AUTO_CLEANUP)
        nexus_resource_cleanup_temp_files()
    endif()

    message(STATUS "Nexus Resource Management finalized")
endfunction()

# Auto-initialize
nexus_configure_parallel_build()
nexus_resource_init()

message(STATUS "NexusOptimization module loaded")

##############################################################################
# End of NexusOptimization.cmake
##############################################################################
