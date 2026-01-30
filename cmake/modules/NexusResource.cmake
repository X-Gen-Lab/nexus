##############################################################################
# NexusResource.cmake - Resource Management Module
##############################################################################
#
# NexusResource.cmake
# Resource management functions for Nexus build system
# Author: Nexus Team
#
# This module provides comprehensive resource management features including
# adaptive parallelism, memory monitoring, priority scheduling, temporary
# file cleanup, and resource usage reporting.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Global Resource Management Variables
##############################################################################

# Resource limits
set(NEXUS_RESOURCE_MAX_MEMORY 0 CACHE STRING "Maximum memory limit in MB (0 = no limit)")
set(NEXUS_RESOURCE_MAX_CPU 0 CACHE STRING "Maximum CPU usage percentage (0 = no limit)")
set(NEXUS_RESOURCE_MAX_DISK 0 CACHE STRING "Maximum disk usage in MB (0 = no limit)")

# Adaptive parallelism
set(NEXUS_RESOURCE_ADAPTIVE_PARALLELISM ON CACHE BOOL "Enable adaptive parallelism")
set(NEXUS_RESOURCE_MIN_PARALLEL_JOBS 1 CACHE STRING "Minimum parallel jobs")
set(NEXUS_RESOURCE_MAX_PARALLEL_JOBS 0 CACHE STRING "Maximum parallel jobs (0 = auto)")

# Memory monitoring
set(NEXUS_RESOURCE_MEMORY_THRESHOLD 1024 CACHE STRING "Memory threshold in MB for adjustment")
set(NEXUS_RESOURCE_MEMORY_CHECK_INTERVAL 10 CACHE STRING "Memory check interval in seconds")

# Priority scheduling
set(NEXUS_RESOURCE_ENABLE_PRIORITY ON CACHE BOOL "Enable priority scheduling")

# Temporary file management
set(NEXUS_RESOURCE_TEMP_DIR "${CMAKE_BINARY_DIR}/.nexus/temp" CACHE STRING "Temporary files directory")
set(NEXUS_RESOURCE_AUTO_CLEANUP ON CACHE BOOL "Automatically cleanup temporary files")

# Resource usage tracking
set(NEXUS_RESOURCE_TRACK_USAGE ON CACHE BOOL "Track resource usage")
set(NEXUS_RESOURCE_USAGE_FILE "${CMAKE_BINARY_DIR}/.nexus/resource_usage.json" CACHE STRING "Resource usage report file")

# Internal state
set(NEXUS_RESOURCE_CURRENT_JOBS 0 CACHE INTERNAL "Current parallel jobs")
set(NEXUS_RESOURCE_PEAK_MEMORY 0 CACHE INTERNAL "Peak memory usage in MB")
set(NEXUS_RESOURCE_TOTAL_CPU_TIME 0 CACHE INTERNAL "Total CPU time in seconds")
set(NEXUS_RESOURCE_TEMP_FILES "" CACHE INTERNAL "List of temporary files")

##############################################################################
# Adaptive Parallelism (Task 18.1)
##############################################################################

#
# Detect system resources (CPU, memory, disk)
# Returns: Sets NEXUS_RESOURCE_CPU_CORES, NEXUS_RESOURCE_TOTAL_MEMORY,
#                  NEXUS_RESOURCE_AVAILABLE_MEMORY, NEXUS_RESOURCE_AVAILABLE_DISK
#
function(nexus_resource_detect_system)
    # Detect CPU cores
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    if(NOT CPU_CORES)
        set(CPU_CORES 1)
    endif()
    set(NEXUS_RESOURCE_CPU_CORES ${CPU_CORES} CACHE INTERNAL "Number of CPU cores")

    # Detect total physical memory
    cmake_host_system_information(RESULT TOTAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
    if(NOT TOTAL_MEMORY)
        set(TOTAL_MEMORY 0)
    endif()
    set(NEXUS_RESOURCE_TOTAL_MEMORY ${TOTAL_MEMORY} CACHE INTERNAL "Total physical memory in MB")

    # Detect available physical memory
    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)
    if(NOT AVAILABLE_MEMORY)
        set(AVAILABLE_MEMORY 0)
    endif()
    set(NEXUS_RESOURCE_AVAILABLE_MEMORY ${AVAILABLE_MEMORY} CACHE INTERNAL "Available physical memory in MB")

    # Detect available disk space
    # Note: AVAILABLE_DISK_SPACE may not be available in all CMake versions
    set(NEXUS_RESOURCE_AVAILABLE_DISK 0 CACHE INTERNAL "Available disk space in MB")

    message(VERBOSE "System resources detected:")
    message(VERBOSE "  CPU Cores: ${CPU_CORES}")
    message(VERBOSE "  Total Memory: ${TOTAL_MEMORY} MB")
    message(VERBOSE "  Available Memory: ${AVAILABLE_MEMORY} MB")
    message(VERBOSE "  Available Disk: ${NEXUS_RESOURCE_AVAILABLE_DISK} MB")
endfunction()

#
# Calculate optimal parallel jobs based on system resources
# Returns: Optimal number of parallel jobs
#
function(nexus_resource_calculate_optimal_jobs OUT_VAR)
    # Get system resources
    nexus_resource_detect_system()

    # Start with CPU cores - 1 (reserve one for system)
    set(CPU_CORES ${NEXUS_RESOURCE_CPU_CORES})
    if(CPU_CORES GREATER 1)
        math(EXPR OPTIMAL_JOBS "${CPU_CORES} - 1")
    else()
        set(OPTIMAL_JOBS 1)
    endif()

    # Adjust based on available memory
    # Assume each job needs ~512MB of memory
    set(MEMORY_PER_JOB 512)
    set(AVAILABLE_MEMORY ${NEXUS_RESOURCE_AVAILABLE_MEMORY})

    if(AVAILABLE_MEMORY GREATER 0)
        math(EXPR MEMORY_BASED_JOBS "${AVAILABLE_MEMORY} / ${MEMORY_PER_JOB}")

        if(MEMORY_BASED_JOBS LESS OPTIMAL_JOBS)
            set(OPTIMAL_JOBS ${MEMORY_BASED_JOBS})
            message(STATUS "Parallel jobs limited by available memory: ${OPTIMAL_JOBS}")
        endif()
    endif()

    # Ensure minimum of 1 job
    if(OPTIMAL_JOBS LESS 1)
        set(OPTIMAL_JOBS 1)
    endif()

    # Apply user-defined maximum if set
    if(NEXUS_RESOURCE_MAX_PARALLEL_JOBS GREATER 0)
        if(OPTIMAL_JOBS GREATER NEXUS_RESOURCE_MAX_PARALLEL_JOBS)
            set(OPTIMAL_JOBS ${NEXUS_RESOURCE_MAX_PARALLEL_JOBS})
        endif()
    endif()

    # Apply user-defined minimum
    if(OPTIMAL_JOBS LESS NEXUS_RESOURCE_MIN_PARALLEL_JOBS)
        set(OPTIMAL_JOBS ${NEXUS_RESOURCE_MIN_PARALLEL_JOBS})
    endif()

    set(${OUT_VAR} ${OPTIMAL_JOBS} PARENT_SCOPE)
endfunction()

#
# Dynamically adjust parallel tasks based on system resources
#
function(nexus_resource_adjust_parallelism)
    if(NOT NEXUS_RESOURCE_ADAPTIVE_PARALLELISM)
        return()
    endif()

    # Calculate optimal jobs
    nexus_resource_calculate_optimal_jobs(OPTIMAL_JOBS)

    # Get current jobs
    set(CURRENT_JOBS ${NEXUS_RESOURCE_CURRENT_JOBS})

    # Only adjust if there's a significant change
    if(NOT CURRENT_JOBS EQUAL OPTIMAL_JOBS)
        set(NEXUS_RESOURCE_CURRENT_JOBS ${OPTIMAL_JOBS} CACHE INTERNAL "Current parallel jobs")

        # Update CMake parallel level
        set(CMAKE_BUILD_PARALLEL_LEVEL ${OPTIMAL_JOBS} PARENT_SCOPE)

        # Update Ninja job pools if using Ninja
        if(CMAKE_GENERATOR MATCHES "Ninja")
            set(CMAKE_JOB_POOLS "compile=${OPTIMAL_JOBS};link=1" PARENT_SCOPE)
        endif()

        message(STATUS "Parallel jobs adjusted: ${CURRENT_JOBS} -> ${OPTIMAL_JOBS}")
    endif()
endfunction()

#
# Initialize adaptive parallelism
#
function(nexus_resource_init_adaptive_parallelism)
    if(NOT NEXUS_RESOURCE_ADAPTIVE_PARALLELISM)
        message(STATUS "Adaptive parallelism disabled")
        return()
    endif()

    # Detect system resources
    nexus_resource_detect_system()

    # Calculate and set initial parallel jobs
    nexus_resource_calculate_optimal_jobs(OPTIMAL_JOBS)
    set(NEXUS_RESOURCE_CURRENT_JOBS ${OPTIMAL_JOBS} CACHE INTERNAL "Current parallel jobs")

    # Configure CMake parallel level
    set(CMAKE_BUILD_PARALLEL_LEVEL ${OPTIMAL_JOBS} PARENT_SCOPE)

    # Configure Ninja job pools
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(CMAKE_JOB_POOLS "compile=${OPTIMAL_JOBS};link=1" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_COMPILE "compile" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_LINK "link" PARENT_SCOPE)
    endif()

    message(STATUS "Adaptive parallelism initialized: ${OPTIMAL_JOBS} jobs")
    message(STATUS "  CPU Cores: ${NEXUS_RESOURCE_CPU_CORES}")
    message(STATUS "  Available Memory: ${NEXUS_RESOURCE_AVAILABLE_MEMORY} MB")
endfunction()

##############################################################################
# Memory Monitoring (Task 18.3)
##############################################################################

#
# Monitor current memory usage
# Returns: Current memory usage in MB
#
function(nexus_resource_get_memory_usage OUT_VAR)
    # Get available memory
    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)

    # Calculate used memory
    set(TOTAL_MEMORY ${NEXUS_RESOURCE_TOTAL_MEMORY})
    if(TOTAL_MEMORY GREATER 0 AND AVAILABLE_MEMORY GREATER 0)
        math(EXPR USED_MEMORY "${TOTAL_MEMORY} - ${AVAILABLE_MEMORY}")
    else()
        set(USED_MEMORY 0)
    endif()

    # Update peak memory if needed
    if(USED_MEMORY GREATER NEXUS_RESOURCE_PEAK_MEMORY)
        set(NEXUS_RESOURCE_PEAK_MEMORY ${USED_MEMORY} CACHE INTERNAL "Peak memory usage")
    endif()

    set(${OUT_VAR} ${USED_MEMORY} PARENT_SCOPE)
endfunction()

#
# Check if memory is low and adjust parallelism
#
function(nexus_resource_check_memory)
    # Get available memory
    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)

    # Check if below threshold
    if(AVAILABLE_MEMORY LESS NEXUS_RESOURCE_MEMORY_THRESHOLD)
        message(WARNING "Low memory detected: ${AVAILABLE_MEMORY} MB available")

        # Reduce parallel jobs
        set(CURRENT_JOBS ${NEXUS_RESOURCE_CURRENT_JOBS})
        if(CURRENT_JOBS GREATER 1)
            math(EXPR NEW_JOBS "${CURRENT_JOBS} / 2")
            if(NEW_JOBS LESS 1)
                set(NEW_JOBS 1)
            endif()

            set(NEXUS_RESOURCE_CURRENT_JOBS ${NEW_JOBS} CACHE INTERNAL "Current parallel jobs")
            set(CMAKE_BUILD_PARALLEL_LEVEL ${NEW_JOBS} PARENT_SCOPE)

            message(STATUS "Reducing parallel jobs to ${NEW_JOBS} due to low memory")

            # Clean up temporary files to free memory
            nexus_resource_cleanup_temp_files()
        endif()
    endif()
endfunction()

#
# Initialize memory monitoring
#
function(nexus_resource_init_memory_monitoring)
    # Detect initial memory state
    nexus_resource_detect_system()

    # Reset peak memory
    set(NEXUS_RESOURCE_PEAK_MEMORY 0 CACHE INTERNAL "Peak memory usage")

    message(STATUS "Memory monitoring initialized")
    message(STATUS "  Total Memory: ${NEXUS_RESOURCE_TOTAL_MEMORY} MB")
    message(STATUS "  Memory Threshold: ${NEXUS_RESOURCE_MEMORY_THRESHOLD} MB")
endfunction()

##############################################################################
# Priority Scheduling (Task 18.5)
##############################################################################

#
# Set task priority for a target
# TARGET: Target name
# PRIORITY: Priority level (1-10, higher = more important)
#
function(nexus_resource_set_priority TARGET PRIORITY)
    if(NOT NEXUS_RESOURCE_ENABLE_PRIORITY)
        return()
    endif()

    # Validate priority
    if(PRIORITY LESS 1 OR PRIORITY GREATER 10)
        message(WARNING "Invalid priority ${PRIORITY} for ${TARGET}, must be 1-10")
        return()
    endif()

    # Store priority as target property
    set_target_properties(${TARGET} PROPERTIES
        NEXUS_BUILD_PRIORITY ${PRIORITY}
    )

    message(VERBOSE "Priority ${PRIORITY} set for target ${TARGET}")
endfunction()

#
# Get task priority for a target
# TARGET: Target name
# Returns: Priority level (default 5)
#
function(nexus_resource_get_priority TARGET OUT_VAR)
    get_target_property(PRIORITY ${TARGET} NEXUS_BUILD_PRIORITY)

    if(NOT PRIORITY)
        set(PRIORITY 5)  # Default priority
    endif()

    set(${OUT_VAR} ${PRIORITY} PARENT_SCOPE)
endfunction()

#
# Sort targets by priority
# TARGETS: List of targets
# Returns: Sorted list of targets (high priority first)
#
function(nexus_resource_sort_by_priority TARGETS OUT_VAR)
    if(NOT NEXUS_RESOURCE_ENABLE_PRIORITY)
        set(${OUT_VAR} ${TARGETS} PARENT_SCOPE)
        return()
    endif()

    # Create list with priority:target format
    set(PRIORITY_LIST "")
    foreach(TARGET ${TARGETS})
        nexus_resource_get_priority(${TARGET} PRIORITY)
        # Invert priority for sorting (10 -> 0, 1 -> 9)
        math(EXPR SORT_KEY "10 - ${PRIORITY}")
        list(APPEND PRIORITY_LIST "${SORT_KEY}:${TARGET}")
    endforeach()

    # Sort by priority
    list(SORT PRIORITY_LIST)

    # Extract targets
    set(SORTED_TARGETS "")
    foreach(ITEM ${PRIORITY_LIST})
        string(REGEX REPLACE "^[0-9]+:" "" TARGET "${ITEM}")
        list(APPEND SORTED_TARGETS ${TARGET})
    endforeach()

    set(${OUT_VAR} ${SORTED_TARGETS} PARENT_SCOPE)
endfunction()

#
# Initialize priority scheduling
#
function(nexus_resource_init_priority_scheduling)
    if(NOT NEXUS_RESOURCE_ENABLE_PRIORITY)
        message(STATUS "Priority scheduling disabled")
        return()
    endif()

    message(STATUS "Priority scheduling initialized")
endfunction()

##############################################################################
# Temporary File Management (Task 18.7)
##############################################################################

#
# Register a temporary file for cleanup
# FILE: Temporary file path
#
function(nexus_resource_register_temp_file FILE)
    # Add to temp files list
    set(TEMP_FILES ${NEXUS_RESOURCE_TEMP_FILES})
    list(APPEND TEMP_FILES ${FILE})
    set(NEXUS_RESOURCE_TEMP_FILES ${TEMP_FILES} CACHE INTERNAL "Temporary files")

    message(VERBOSE "Registered temporary file: ${FILE}")
endfunction()

#
# Create a temporary file
# PREFIX: File prefix
# SUFFIX: File suffix (e.g., .txt, .tmp)
# Returns: Path to created temporary file
#
function(nexus_resource_create_temp_file PREFIX SUFFIX OUT_VAR)
    # Ensure temp directory exists
    file(MAKE_DIRECTORY ${NEXUS_RESOURCE_TEMP_DIR})

    # Generate unique filename
    string(TIMESTAMP TIMESTAMP "%Y%m%d_%H%M%S")
    string(RANDOM LENGTH 8 RANDOM_STR)
    set(TEMP_FILE "${NEXUS_RESOURCE_TEMP_DIR}/${PREFIX}_${TIMESTAMP}_${RANDOM_STR}${SUFFIX}")

    # Create empty file
    file(WRITE ${TEMP_FILE} "")

    # Register for cleanup
    nexus_resource_register_temp_file(${TEMP_FILE})

    set(${OUT_VAR} ${TEMP_FILE} PARENT_SCOPE)
endfunction()

#
# Identify all temporary files in build directory
# Returns: List of temporary files
#
function(nexus_resource_identify_temp_files OUT_VAR)
    set(TEMP_PATTERNS
        "*.tmp"
        "*.temp"
        "*.cache"
        "*.log"
        "*.d"
        "*.o.tmp"
        "CMakeFiles/*.dir/*.tmp"
    )

    set(FOUND_FILES "")

    foreach(PATTERN ${TEMP_PATTERNS})
        file(GLOB_RECURSE PATTERN_FILES "${CMAKE_BINARY_DIR}/${PATTERN}")
        list(APPEND FOUND_FILES ${PATTERN_FILES})
    endforeach()

    # Add registered temp files
    list(APPEND FOUND_FILES ${NEXUS_RESOURCE_TEMP_FILES})

    # Remove duplicates
    if(FOUND_FILES)
        list(REMOVE_DUPLICATES FOUND_FILES)
    endif()

    set(${OUT_VAR} ${FOUND_FILES} PARENT_SCOPE)
endfunction()

#
# Clean up temporary files
#
function(nexus_resource_cleanup_temp_files)
    # Identify temp files
    nexus_resource_identify_temp_files(TEMP_FILES)

    if(NOT TEMP_FILES)
        message(VERBOSE "No temporary files to clean up")
        return()
    endif()

    # Count and calculate size
    list(LENGTH TEMP_FILES FILE_COUNT)
    set(TOTAL_SIZE 0)

    foreach(FILE ${TEMP_FILES})
        if(EXISTS ${FILE})
            file(SIZE ${FILE} FILE_SIZE)
            math(EXPR TOTAL_SIZE "${TOTAL_SIZE} + ${FILE_SIZE}")
        endif()
    endforeach()

    # Convert to MB
    math(EXPR TOTAL_SIZE_MB "${TOTAL_SIZE} / 1048576")

    # Delete files
    set(DELETED_COUNT 0)
    foreach(FILE ${TEMP_FILES})
        if(EXISTS ${FILE})
            file(REMOVE ${FILE})
            math(EXPR DELETED_COUNT "${DELETED_COUNT} + 1")
        endif()
    endforeach()

    # Clear registered temp files
    set(NEXUS_RESOURCE_TEMP_FILES "" CACHE INTERNAL "Temporary files")

    message(STATUS "Cleaned up ${DELETED_COUNT} temporary files (${TOTAL_SIZE_MB} MB)")
endfunction()

#
# Initialize temporary file management
#
function(nexus_resource_init_temp_file_management)
    # Create temp directory
    file(MAKE_DIRECTORY ${NEXUS_RESOURCE_TEMP_DIR})

    # Clear temp files list
    set(NEXUS_RESOURCE_TEMP_FILES "" CACHE INTERNAL "Temporary files")

    message(STATUS "Temporary file management initialized")
    message(STATUS "  Temp Directory: ${NEXUS_RESOURCE_TEMP_DIR}")
    message(STATUS "  Auto Cleanup: ${NEXUS_RESOURCE_AUTO_CLEANUP}")
endfunction()

##############################################################################
# Resource Usage Reporting (Task 18.9)
##############################################################################

#
# Record resource usage snapshot
#
function(nexus_resource_record_usage)
    if(NOT NEXUS_RESOURCE_TRACK_USAGE)
        return()
    endif()

    # Get current resource usage
    nexus_resource_get_memory_usage(MEMORY_USAGE)
    cmake_host_system_information(RESULT AVAILABLE_MEMORY QUERY AVAILABLE_PHYSICAL_MEMORY)

    # Get timestamp
    string(TIMESTAMP TIMESTAMP "%Y-%m-%d %H:%M:%S")

    # Append to usage log
    set(USAGE_LOG "${CMAKE_BINARY_DIR}/.nexus/resource_usage.log")
    file(APPEND ${USAGE_LOG} "${TIMESTAMP},${MEMORY_USAGE},${AVAILABLE_MEMORY},${NEXUS_RESOURCE_CURRENT_JOBS}\n")
endfunction()

#
# Generate resource usage report
#
function(nexus_resource_generate_report)
    if(NOT NEXUS_RESOURCE_TRACK_USAGE)
        return()
    endif()

    # Get final resource statistics
    nexus_resource_get_memory_usage(CURRENT_MEMORY)
    set(PEAK_MEMORY ${NEXUS_RESOURCE_PEAK_MEMORY})

    # Get disk usage
    if(EXISTS ${CMAKE_BINARY_DIR})
        file(GLOB_RECURSE ALL_FILES "${CMAKE_BINARY_DIR}/*")
        set(TOTAL_DISK 0)
        foreach(FILE ${ALL_FILES})
            if(EXISTS ${FILE} AND NOT IS_DIRECTORY ${FILE})
                file(SIZE ${FILE} FILE_SIZE)
                math(EXPR TOTAL_DISK "${TOTAL_DISK} + ${FILE_SIZE}")
            endif()
        endforeach()
        math(EXPR TOTAL_DISK_MB "${TOTAL_DISK} / 1048576")
    else()
        set(TOTAL_DISK_MB 0)
    endif()

    # Generate JSON report
    file(WRITE ${NEXUS_RESOURCE_USAGE_FILE} "{\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  \"timestamp\": \"${TIMESTAMP}\",\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  \"system\": {\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"cpu_cores\": ${NEXUS_RESOURCE_CPU_CORES},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"total_memory_mb\": ${NEXUS_RESOURCE_TOTAL_MEMORY},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"available_disk_mb\": ${NEXUS_RESOURCE_AVAILABLE_DISK}\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  },\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  \"usage\": {\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"current_memory_mb\": ${CURRENT_MEMORY},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"peak_memory_mb\": ${PEAK_MEMORY},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"disk_usage_mb\": ${TOTAL_DISK_MB},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"parallel_jobs\": ${NEXUS_RESOURCE_CURRENT_JOBS}\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  },\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  \"limits\": {\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"max_memory_mb\": ${NEXUS_RESOURCE_MAX_MEMORY},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"max_cpu_percent\": ${NEXUS_RESOURCE_MAX_CPU},\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "    \"max_disk_mb\": ${NEXUS_RESOURCE_MAX_DISK}\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "  }\n")
    file(APPEND ${NEXUS_RESOURCE_USAGE_FILE} "}\n")

    message(STATUS "Resource usage report generated: ${NEXUS_RESOURCE_USAGE_FILE}")

    # Display summary
    message(STATUS "")
    message(STATUS "Resource Usage Summary:")
    message(STATUS "=======================")
    message(STATUS "  Peak Memory: ${PEAK_MEMORY} MB")
    message(STATUS "  Disk Usage: ${TOTAL_DISK_MB} MB")
    message(STATUS "  Parallel Jobs: ${NEXUS_RESOURCE_CURRENT_JOBS}")
endfunction()

#
# Initialize resource usage tracking
#
function(nexus_resource_init_usage_tracking)
    if(NOT NEXUS_RESOURCE_TRACK_USAGE)
        message(STATUS "Resource usage tracking disabled")
        return()
    endif()

    # Create usage log file
    set(USAGE_LOG "${CMAKE_BINARY_DIR}/.nexus/resource_usage.log")
    file(WRITE ${USAGE_LOG} "timestamp,memory_used_mb,memory_available_mb,parallel_jobs\n")

    # Record initial snapshot
    nexus_resource_record_usage()

    message(STATUS "Resource usage tracking initialized")
endfunction()

##############################################################################
# Initialization
##############################################################################

#
# Initialize all resource management features
#
function(nexus_resource_init)
    message(STATUS "Initializing Nexus Resource Management...")

    # Initialize adaptive parallelism
    nexus_resource_init_adaptive_parallelism()

    # Initialize memory monitoring
    nexus_resource_init_memory_monitoring()

    # Initialize priority scheduling
    nexus_resource_init_priority_scheduling()

    # Initialize temporary file management
    nexus_resource_init_temp_file_management()

    # Initialize resource usage tracking
    nexus_resource_init_usage_tracking()

    message(STATUS "Nexus Resource Management initialized successfully")
endfunction()

#
# Finalize resource management (cleanup and reporting)
#
function(nexus_resource_finalize)
    message(STATUS "Finalizing Nexus Resource Management...")

    # Generate resource usage report
    nexus_resource_generate_report()

    # Clean up temporary files if enabled
    if(NEXUS_RESOURCE_AUTO_CLEANUP)
        nexus_resource_cleanup_temp_files()
    endif()

    message(STATUS "Nexus Resource Management finalized")
endfunction()

# Auto-initialize on module load
nexus_resource_init()
