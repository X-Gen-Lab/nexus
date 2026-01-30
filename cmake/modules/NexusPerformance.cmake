##############################################################################
# NexusPerformance.cmake - Build Performance Optimization Module
##############################################################################
#
# NexusPerformance.cmake
# Performance optimization functions for Nexus build system
# Author: Nexus Team
#
# This module provides performance optimization features including parallel
# compilation scheduling, build progress display, statistics collection,
# and performance analysis tools.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Global Performance Variables
##############################################################################

# Build statistics
set(NEXUS_BUILD_START_TIME 0 CACHE INTERNAL "Build start timestamp")
set(NEXUS_BUILD_TOTAL_FILES 0 CACHE INTERNAL "Total files to compile")
set(NEXUS_BUILD_COMPILED_FILES 0 CACHE INTERNAL "Number of compiled files")
set(NEXUS_BUILD_CACHE_HITS 0 CACHE INTERNAL "Cache hit count")
set(NEXUS_BUILD_CACHE_MISSES 0 CACHE INTERNAL "Cache miss count")

# Performance settings
set(NEXUS_PARALLEL_JOBS 0 CACHE STRING "Number of parallel compilation jobs")
set(NEXUS_ENABLE_PROGRESS ON CACHE BOOL "Enable build progress display")
set(NEXUS_ENABLE_STATISTICS ON CACHE BOOL "Enable build statistics collection")

##############################################################################
# Parallel Compilation Scheduling
##############################################################################

#
# Configure parallel compilation based on CPU cores
#
function(nexus_configure_parallel_build)
    # Detect CPU core count
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)

    if(NOT CPU_CORES)
        set(CPU_CORES 1)
    endif()

    # Calculate optimal parallel jobs
    # Reserve 1 core for system, use remaining cores
    if(CPU_CORES GREATER 1)
        math(EXPR OPTIMAL_JOBS "${CPU_CORES} - 1")
    else()
        set(OPTIMAL_JOBS 1)
    endif()

    # Use user-specified value if provided, otherwise use optimal
    if(NEXUS_PARALLEL_JOBS EQUAL 0)
        set(NEXUS_PARALLEL_JOBS ${OPTIMAL_JOBS} CACHE STRING "Number of parallel jobs" FORCE)
    endif()

    # Set CMake parallel level
    set(CMAKE_BUILD_PARALLEL_LEVEL ${NEXUS_PARALLEL_JOBS} PARENT_SCOPE)

    # Configure Ninja/Make parallel jobs
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(CMAKE_JOB_POOLS "compile=${NEXUS_PARALLEL_JOBS};link=1" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_COMPILE "compile" PARENT_SCOPE)
        set(CMAKE_JOB_POOL_LINK "link" PARENT_SCOPE)
    endif()

    message(STATUS "Parallel build configured: ${NEXUS_PARALLEL_JOBS} jobs (${CPU_CORES} CPU cores detected)")
endfunction()

#
# Adjust parallel jobs dynamically based on system load
#
function(nexus_adjust_parallel_jobs)
    # Get available memory
    cmake_host_system_information(RESULT AVAILABLE_MEMORY
                                   QUERY AVAILABLE_PHYSICAL_MEMORY)

    # If low memory (< 2GB), reduce parallel jobs
    if(AVAILABLE_MEMORY LESS 2048)
        math(EXPR NEW_JOBS "${NEXUS_PARALLEL_JOBS} / 2")
        if(NEW_JOBS LESS 1)
            set(NEW_JOBS 1)
        endif()

        set(NEXUS_PARALLEL_JOBS ${NEW_JOBS} CACHE STRING "Adjusted parallel jobs" FORCE)
        message(STATUS "Low memory detected (${AVAILABLE_MEMORY} MB), reducing parallel jobs to ${NEW_JOBS}")
    endif()
endfunction()

#
# Initialize task queue for build scheduling
# TARGET: Target name
#
function(nexus_init_task_queue TARGET)
    # Get all source files for the target
    get_target_property(SOURCES ${TARGET} SOURCES)

    if(NOT SOURCES)
        return()
    endif()

    # Count total files
    list(LENGTH SOURCES FILE_COUNT)
    math(EXPR TOTAL "${NEXUS_BUILD_TOTAL_FILES} + ${FILE_COUNT}")
    set(NEXUS_BUILD_TOTAL_FILES ${TOTAL} CACHE INTERNAL "Total files to compile")

    # Create task queue file
    set(QUEUE_FILE "${CMAKE_BINARY_DIR}/.nexus/task_queue_${TARGET}.txt")
    file(WRITE ${QUEUE_FILE} "")

    foreach(SOURCE ${SOURCES})
        file(APPEND ${QUEUE_FILE} "${SOURCE}\n")
    endforeach()

    message(VERBOSE "Task queue initialized for ${TARGET}: ${FILE_COUNT} files")
endfunction()

#
# Optimize task allocation based on file dependencies
# TARGET: Target name
#
function(nexus_optimize_task_allocation TARGET)
    # Get dependency information
    get_target_property(SOURCES ${TARGET} SOURCES)

    if(NOT SOURCES)
        return()
    endif()

    # Sort sources by dependency depth (files with fewer dependencies first)
    # This allows independent files to compile in parallel first
    set(SORTED_SOURCES "")

    foreach(SOURCE ${SOURCES})
        # Get dependency file if it exists
        get_filename_component(SOURCE_NAME ${SOURCE} NAME_WE)
        set(DEP_FILE "${CMAKE_BINARY_DIR}/CMakeFiles/${TARGET}.dir/${SOURCE_NAME}.d")

        if(EXISTS ${DEP_FILE})
            file(READ ${DEP_FILE} DEP_CONTENT)
            string(REGEX MATCHALL "[^\n]+" DEP_LINES "${DEP_CONTENT}")
            list(LENGTH DEP_LINES DEP_COUNT)
        else()
            set(DEP_COUNT 0)
        endif()

        list(APPEND SORTED_SOURCES "${DEP_COUNT}:${SOURCE}")
    endforeach()

    # Sort by dependency count
    list(SORT SORTED_SOURCES)

    # Extract sources
    set(OPTIMIZED_SOURCES "")
    foreach(ITEM ${SORTED_SOURCES})
        string(REGEX REPLACE "^[0-9]+:" "" SOURCE "${ITEM}")
        list(APPEND OPTIMIZED_SOURCES ${SOURCE})
    endforeach()

    # Update target sources order
    set_target_properties(${TARGET} PROPERTIES SOURCES "${OPTIMIZED_SOURCES}")

    message(VERBOSE "Task allocation optimized for ${TARGET}")
endfunction()

##############################################################################
# Build Progress Display
##############################################################################

#
# Initialize build progress tracking
#
function(nexus_init_progress)
    if(NOT NEXUS_ENABLE_PROGRESS)
        return()
    endif()

    # Record build start time
    string(TIMESTAMP START_TIME "%s")
    set(NEXUS_BUILD_START_TIME ${START_TIME} CACHE INTERNAL "Build start time")

    # Reset counters
    set(NEXUS_BUILD_COMPILED_FILES 0 CACHE INTERNAL "Compiled files")

    # Create progress directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/.nexus")

    message(STATUS "Build progress tracking initialized")
endfunction()

#
# Update build progress for a file
# FILE: Source file being compiled
#
function(nexus_update_progress FILE)
    if(NOT NEXUS_ENABLE_PROGRESS)
        return()
    endif()

    # Increment compiled files counter
    math(EXPR COMPILED "${NEXUS_BUILD_COMPILED_FILES} + 1")
    set(NEXUS_BUILD_COMPILED_FILES ${COMPILED} CACHE INTERNAL "Compiled files")

    # Calculate progress percentage
    if(NEXUS_BUILD_TOTAL_FILES GREATER 0)
        math(EXPR PROGRESS "(${COMPILED} * 100) / ${NEXUS_BUILD_TOTAL_FILES}")
    else()
        set(PROGRESS 0)
    endif()

    # Calculate remaining files
    math(EXPR REMAINING "${NEXUS_BUILD_TOTAL_FILES} - ${COMPILED}")

    # Display progress
    get_filename_component(FILE_NAME ${FILE} NAME)
    message(STATUS "[${PROGRESS}%] Compiling ${FILE_NAME} (${REMAINING} remaining)")
endfunction()

#
# Display real-time build progress
# TARGET: Target name
#
function(nexus_display_progress TARGET)
    if(NOT NEXUS_ENABLE_PROGRESS)
        return()
    endif()

    # Add custom command to track compilation progress
    get_target_property(SOURCES ${TARGET} SOURCES)

    if(NOT SOURCES)
        return()
    endif()

    foreach(SOURCE ${SOURCES})
        # Add progress tracking to each source compilation
        set_source_files_properties(${SOURCE} PROPERTIES
            COMPILE_FLAGS "-DNEXUS_PROGRESS_TRACKING"
        )
    endforeach()

    message(VERBOSE "Progress display enabled for ${TARGET}")
endfunction()

##############################################################################
# Build Statistics Collection
##############################################################################

#
# Initialize build statistics collection
#
function(nexus_init_statistics)
    if(NOT NEXUS_ENABLE_STATISTICS)
        return()
    endif()

    # Create statistics file
    set(STATS_FILE "${CMAKE_BINARY_DIR}/.nexus/build_stats.json")
    file(WRITE ${STATS_FILE} "{\n")
    file(APPEND ${STATS_FILE} "  \"build_start\": \"${NEXUS_BUILD_START_TIME}\",\n")
    file(APPEND ${STATS_FILE} "  \"platform\": \"${CMAKE_SYSTEM_NAME}\",\n")
    file(APPEND ${STATS_FILE} "  \"compiler\": \"${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\",\n")
    file(APPEND ${STATS_FILE} "  \"build_type\": \"${CMAKE_BUILD_TYPE}\",\n")
    file(APPEND ${STATS_FILE} "  \"parallel_jobs\": ${NEXUS_PARALLEL_JOBS},\n")
    file(APPEND ${STATS_FILE} "  \"targets\": [\n")
    file(APPEND ${STATS_FILE} "  ]\n")
    file(APPEND ${STATS_FILE} "}\n")

    message(STATUS "Build statistics collection initialized")
endfunction()

#
# Record compilation time for a file
# FILE: Source file
# TIME: Compilation time in seconds
#
function(nexus_record_compile_time FILE TIME)
    if(NOT NEXUS_ENABLE_STATISTICS)
        return()
    endif()

    # Append to statistics file
    set(STATS_FILE "${CMAKE_BINARY_DIR}/.nexus/compile_times.txt")
    file(APPEND ${STATS_FILE} "${FILE}:${TIME}\n")
endfunction()

#
# Record cache hit/miss
# HIT: TRUE for cache hit, FALSE for cache miss
#
function(nexus_record_cache_stat HIT)
    if(NOT NEXUS_ENABLE_STATISTICS)
        return()
    endif()

    if(HIT)
        math(EXPR HITS "${NEXUS_BUILD_CACHE_HITS} + 1")
        set(NEXUS_BUILD_CACHE_HITS ${HITS} CACHE INTERNAL "Cache hits")
    else()
        math(EXPR MISSES "${NEXUS_BUILD_CACHE_MISSES} + 1")
        set(NEXUS_BUILD_CACHE_MISSES ${MISSES} CACHE INTERNAL "Cache misses")
    endif()
endfunction()

#
# Generate build statistics report
#
function(nexus_generate_statistics_report)
    if(NOT NEXUS_ENABLE_STATISTICS)
        return()
    endif()

    # Calculate build time
    string(TIMESTAMP END_TIME "%s")
    math(EXPR BUILD_TIME "${END_TIME} - ${NEXUS_BUILD_START_TIME}")

    # Calculate cache hit rate
    math(EXPR TOTAL_CACHE "${NEXUS_BUILD_CACHE_HITS} + ${NEXUS_BUILD_CACHE_MISSES}")
    if(TOTAL_CACHE GREATER 0)
        math(EXPR HIT_RATE "(${NEXUS_BUILD_CACHE_HITS} * 100) / ${TOTAL_CACHE}")
    else()
        set(HIT_RATE 0)
    endif()

    # Generate report
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
    file(APPEND ${REPORT_FILE} "Cache Statistics:\n")
    file(APPEND ${REPORT_FILE} "  Cache Hits:      ${NEXUS_BUILD_CACHE_HITS}\n")
    file(APPEND ${REPORT_FILE} "  Cache Misses:    ${NEXUS_BUILD_CACHE_MISSES}\n")
    file(APPEND ${REPORT_FILE} "  Hit Rate:        ${HIT_RATE}%\n")
    file(APPEND ${REPORT_FILE} "\n")
    file(APPEND ${REPORT_FILE} "=================================================\n")

    message(STATUS "Build statistics report generated: ${REPORT_FILE}")

    # Display summary
    message(STATUS "")
    message(STATUS "Build completed in ${BUILD_TIME} seconds")
    message(STATUS "Compiled ${NEXUS_BUILD_COMPILED_FILES}/${NEXUS_BUILD_TOTAL_FILES} files")
    message(STATUS "Cache hit rate: ${HIT_RATE}%")
endfunction()

##############################################################################
# Performance Analysis Tools
##############################################################################

#
# Identify build bottlenecks
#
function(nexus_identify_bottlenecks)
    # Read compile times
    set(TIMES_FILE "${CMAKE_BINARY_DIR}/.nexus/compile_times.txt")

    if(NOT EXISTS ${TIMES_FILE})
        message(STATUS "No compile time data available")
        return()
    endif()

    file(READ ${TIMES_FILE} TIMES_CONTENT)
    string(REGEX MATCHALL "[^\n]+" TIME_LINES "${TIMES_CONTENT}")

    # Find slowest files
    set(SLOWEST_FILES "")
    set(MAX_TIME 0)

    foreach(LINE ${TIME_LINES})
        string(REGEX MATCH "^([^:]+):([0-9.]+)$" MATCH "${LINE}")
        if(MATCH)
            set(FILE "${CMAKE_MATCH_1}")
            set(TIME "${CMAKE_MATCH_2}")

            if(TIME GREATER MAX_TIME)
                set(MAX_TIME ${TIME})
                list(INSERT SLOWEST_FILES 0 "${TIME}:${FILE}")
            else()
                list(APPEND SLOWEST_FILES "${TIME}:${FILE}")
            endif()
        endif()
    endforeach()

    # Display top 10 slowest files
    message(STATUS "")
    message(STATUS "Build Bottlenecks (Top 10 slowest files):")
    message(STATUS "==========================================")

    set(COUNT 0)
    foreach(ITEM ${SLOWEST_FILES})
        if(COUNT GREATER_EQUAL 10)
            break()
        endif()

        string(REGEX REPLACE "^([0-9.]+):" "" FILE "${ITEM}")
        string(REGEX MATCH "^([0-9.]+):" TIME_MATCH "${ITEM}")
        string(REGEX REPLACE ":" "" TIME "${TIME_MATCH}")

        get_filename_component(FILE_NAME ${FILE} NAME)
        message(STATUS "  ${TIME}s - ${FILE_NAME}")

        math(EXPR COUNT "${COUNT} + 1")
    endforeach()
endfunction()

#
# Generate performance report with optimization suggestions
#
function(nexus_generate_performance_report)
    set(REPORT_FILE "${CMAKE_BINARY_DIR}/.nexus/performance_report.txt")
    file(WRITE ${REPORT_FILE} "")
    file(APPEND ${REPORT_FILE} "=================================================\n")
    file(APPEND ${REPORT_FILE} "Nexus Build Performance Analysis\n")
    file(APPEND ${REPORT_FILE} "=================================================\n")
    file(APPEND ${REPORT_FILE} "\n")

    # Analyze parallel efficiency
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)

    if(NEXUS_PARALLEL_JOBS LESS CPU_CORES)
        file(APPEND ${REPORT_FILE} "SUGGESTION: Increase parallel jobs\n")
        file(APPEND ${REPORT_FILE} "  Current: ${NEXUS_PARALLEL_JOBS} jobs\n")
        file(APPEND ${REPORT_FILE} "  Available: ${CPU_CORES} CPU cores\n")
        file(APPEND ${REPORT_FILE} "  Recommendation: Use ${CPU_CORES} parallel jobs\n")
        file(APPEND ${REPORT_FILE} "\n")
    endif()

    # Analyze cache effectiveness
    math(EXPR TOTAL_CACHE "${NEXUS_BUILD_CACHE_HITS} + ${NEXUS_BUILD_CACHE_MISSES}")
    if(TOTAL_CACHE GREATER 0)
        math(EXPR HIT_RATE "(${NEXUS_BUILD_CACHE_HITS} * 100) / ${TOTAL_CACHE}")

        if(HIT_RATE LESS 50)
            file(APPEND ${REPORT_FILE} "WARNING: Low cache hit rate (${HIT_RATE}%)\n")
            file(APPEND ${REPORT_FILE} "  Possible causes:\n")
            file(APPEND ${REPORT_FILE} "  - Frequent code changes\n")
            file(APPEND ${REPORT_FILE} "  - Cache size too small\n")
            file(APPEND ${REPORT_FILE} "  - Cache not properly configured\n")
            file(APPEND ${REPORT_FILE} "\n")
        endif()
    endif()

    # Analyze build time
    string(TIMESTAMP END_TIME "%s")
    math(EXPR BUILD_TIME "${END_TIME} - ${NEXUS_BUILD_START_TIME}")

    if(BUILD_TIME GREATER 300)  # > 5 minutes
        file(APPEND ${REPORT_FILE} "SUGGESTION: Long build time detected (${BUILD_TIME}s)\n")
        file(APPEND ${REPORT_FILE} "  Recommendations:\n")
        file(APPEND ${REPORT_FILE} "  - Enable precompiled headers\n")
        file(APPEND ${REPORT_FILE} "  - Use ccache/sccache\n")
        file(APPEND ${REPORT_FILE} "  - Increase parallel jobs\n")
        file(APPEND ${REPORT_FILE} "  - Enable unity builds for large targets\n")
        file(APPEND ${REPORT_FILE} "\n")
    endif()

    file(APPEND ${REPORT_FILE} "=================================================\n")

    message(STATUS "Performance report generated: ${REPORT_FILE}")
endfunction()

#
# Provide optimization suggestions based on build analysis
#
function(nexus_suggest_optimizations)
    message(STATUS "")
    message(STATUS "Build Optimization Suggestions:")
    message(STATUS "================================")

    # Check parallel jobs
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    if(NEXUS_PARALLEL_JOBS LESS CPU_CORES)
        message(STATUS "  • Increase parallel jobs to ${CPU_CORES} (currently ${NEXUS_PARALLEL_JOBS})")
    endif()

    # Check cache usage
    math(EXPR TOTAL_CACHE "${NEXUS_BUILD_CACHE_HITS} + ${NEXUS_BUILD_CACHE_MISSES}")
    if(TOTAL_CACHE GREATER 0)
        math(EXPR HIT_RATE "(${NEXUS_BUILD_CACHE_HITS} * 100) / ${TOTAL_CACHE}")
        if(HIT_RATE LESS 50)
            message(STATUS "  • Enable or configure build cache (current hit rate: ${HIT_RATE}%)")
        endif()
    else()
        message(STATUS "  • Enable build cache (ccache/sccache)")
    endif()

    # Check build time
    string(TIMESTAMP END_TIME "%s")
    math(EXPR BUILD_TIME "${END_TIME} - ${NEXUS_BUILD_START_TIME}")
    if(BUILD_TIME GREATER 300)
        message(STATUS "  • Consider using precompiled headers")
        message(STATUS "  • Consider using unity builds for large targets")
    endif()

    message(STATUS "")
endfunction()

##############################################################################
# Initialization
##############################################################################

# Auto-configure parallel build on module load
nexus_configure_parallel_build()

