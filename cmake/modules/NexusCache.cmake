##############################################################################
# NexusCache.cmake - Cache Management Module
##############################################################################
#
# NexusCache.cmake
# Build cache management for Nexus build system
# Author: Nexus Team
#
# This module provides build cache management functionality, including
# local and remote cache support, content-addressable storage, and
# cache statistics.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Cache Configuration
##############################################################################

# Default cache settings
set(NEXUS_CACHE_ENABLED FALSE CACHE BOOL "Enable build cache")
set(NEXUS_CACHE_LOCAL_DIR "${CMAKE_BINARY_DIR}/.cache" CACHE PATH "Local cache directory")
set(NEXUS_CACHE_REMOTE_URL "" CACHE STRING "Remote cache URL (optional)")
set(NEXUS_CACHE_MAX_SIZE 1024 CACHE STRING "Maximum cache size in MB")
set(NEXUS_CACHE_COMPRESSION_LEVEL 6 CACHE STRING "Cache compression level (0-9)")
set(NEXUS_CACHE_ENCRYPTION FALSE CACHE BOOL "Enable cache encryption")
set(NEXUS_CACHE_TTL 2592000 CACHE STRING "Cache TTL in seconds (default: 30 days)")

##############################################################################
# Cache Initialization
##############################################################################

#
# Enable and configure build cache
# LOCAL_DIR: Local cache directory (optional)
# REMOTE_URL: Remote cache URL (optional)
# MAX_SIZE: Maximum cache size in MB (optional)
#
function(nexus_enable_cache)
    cmake_parse_arguments(
        ARG
        ""
        "LOCAL_DIR;REMOTE_URL;MAX_SIZE"
        ""
        ${ARGN}
    )

    # Set cache directory
    if(ARG_LOCAL_DIR)
        set(NEXUS_CACHE_LOCAL_DIR ${ARG_LOCAL_DIR} CACHE PATH "Local cache directory" FORCE)
    endif()

    # Set remote cache URL
    if(ARG_REMOTE_URL)
        set(NEXUS_CACHE_REMOTE_URL ${ARG_REMOTE_URL} CACHE STRING "Remote cache URL" FORCE)
    endif()

    # Set maximum cache size
    if(ARG_MAX_SIZE)
        set(NEXUS_CACHE_MAX_SIZE ${ARG_MAX_SIZE} CACHE STRING "Maximum cache size in MB" FORCE)
    endif()

    # Create cache directory
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR})
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/objects)
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/manifests)
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/metadata)

    # Enable cache
    set(NEXUS_CACHE_ENABLED TRUE CACHE BOOL "Enable build cache" FORCE)

    # Try to find ccache or sccache
    find_program(CCACHE_PROGRAM ccache)
    find_program(SCCACHE_PROGRAM sccache)

    if(CCACHE_PROGRAM)
        message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)

        # Configure ccache
        set(ENV{CCACHE_DIR} ${NEXUS_CACHE_LOCAL_DIR})
        set(ENV{CCACHE_MAXSIZE} "${NEXUS_CACHE_MAX_SIZE}M")
        set(ENV{CCACHE_COMPRESS} "true")
        set(ENV{CCACHE_COMPRESSLEVEL} ${NEXUS_CACHE_COMPRESSION_LEVEL})

    elseif(SCCACHE_PROGRAM)
        message(STATUS "Found sccache: ${SCCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)

        # Configure sccache
        set(ENV{SCCACHE_DIR} ${NEXUS_CACHE_LOCAL_DIR})
        set(ENV{SCCACHE_CACHE_SIZE} "${NEXUS_CACHE_MAX_SIZE}M")

    else()
        message(WARNING "Neither ccache nor sccache found. Cache will not be used.")
        set(NEXUS_CACHE_ENABLED FALSE CACHE BOOL "Enable build cache" FORCE)
        return()
    endif()

    message(STATUS "Build cache enabled:")
    message(STATUS "  Local directory: ${NEXUS_CACHE_LOCAL_DIR}")
    message(STATUS "  Max size: ${NEXUS_CACHE_MAX_SIZE} MB")
    if(ARG_REMOTE_URL)
        message(STATUS "  Remote URL: ${NEXUS_CACHE_REMOTE_URL}")
    endif()
endfunction()

##############################################################################
# Cache Configuration
##############################################################################

#
# Configure cache policy
# COMPRESSION: Compression level (0-9) (optional)
# ENCRYPTION: Enable encryption (optional)
# TTL: Cache TTL in seconds (optional)
#
function(nexus_configure_cache)
    cmake_parse_arguments(
        ARG
        ""
        "COMPRESSION;ENCRYPTION;TTL"
        ""
        ${ARGN}
    )

    # Set compression level
    if(DEFINED ARG_COMPRESSION)
        set(NEXUS_CACHE_COMPRESSION_LEVEL ${ARG_COMPRESSION} CACHE STRING "Cache compression level" FORCE)
    endif()

    # Set encryption
    if(DEFINED ARG_ENCRYPTION)
        set(NEXUS_CACHE_ENCRYPTION ${ARG_ENCRYPTION} CACHE BOOL "Enable cache encryption" FORCE)
    endif()

    # Set TTL
    if(DEFINED ARG_TTL)
        set(NEXUS_CACHE_TTL ${ARG_TTL} CACHE STRING "Cache TTL in seconds" FORCE)
    endif()

    message(STATUS "Cache configuration updated")
endfunction()

##############################################################################
# Cache Cleanup
##############################################################################

#
# Clean build cache
# OLDER_THAN: Remove entries older than N days (optional)
# SIZE_LIMIT: Remove entries to reach size limit in MB (optional)
#
function(nexus_clean_cache)
    cmake_parse_arguments(
        ARG
        ""
        "OLDER_THAN;SIZE_LIMIT"
        ""
        ${ARGN}
    )

    if(NOT NEXUS_CACHE_ENABLED)
        message(STATUS "Cache is not enabled, nothing to clean")
        return()
    endif()

    message(STATUS "Cleaning build cache...")

    # Clean by age (LRU)
    if(ARG_OLDER_THAN)
        message(STATUS "  Removing entries older than ${ARG_OLDER_THAN} days")
        nexus_cache_clean_by_age(${ARG_OLDER_THAN})
    endif()

    # Clean by size
    if(ARG_SIZE_LIMIT)
        message(STATUS "  Removing entries to reach ${ARG_SIZE_LIMIT} MB limit")
        nexus_cache_clean_by_size(${ARG_SIZE_LIMIT})
    endif()

    # Use ccache/sccache cleanup if available
    if(CCACHE_PROGRAM)
        execute_process(
            COMMAND ${CCACHE_PROGRAM} --cleanup
            OUTPUT_QUIET
            ERROR_QUIET
        )
    elseif(SCCACHE_PROGRAM)
        execute_process(
            COMMAND ${SCCACHE_PROGRAM} --stop-server
            OUTPUT_QUIET
            ERROR_QUIET
        )
    endif()

    message(STATUS "Cache cleanup complete")
endfunction()

#
# Clean cache entries older than specified days
# DAYS: Number of days
#
function(nexus_cache_clean_by_age DAYS)
    # Calculate cutoff timestamp
    string(TIMESTAMP CURRENT_TIME "%s")
    math(EXPR CUTOFF_TIME "${CURRENT_TIME} - (${DAYS} * 86400)")

    # Scan cache directory
    file(GLOB_RECURSE CACHE_FILES ${NEXUS_CACHE_LOCAL_DIR}/objects/*/*)

    set(REMOVED_COUNT 0)
    foreach(CACHE_FILE ${CACHE_FILES})
        # Get file modification time
        file(TIMESTAMP ${CACHE_FILE} FILE_TIME "%s")

        # Remove if older than cutoff
        if(FILE_TIME LESS CUTOFF_TIME)
            file(REMOVE ${CACHE_FILE})

            # Remove metadata if exists
            get_filename_component(CACHE_KEY ${CACHE_FILE} NAME)
            if(EXISTS ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
                file(REMOVE ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
            endif()

            math(EXPR REMOVED_COUNT "${REMOVED_COUNT} + 1")
        endif()
    endforeach()

    message(STATUS "  Removed ${REMOVED_COUNT} old cache entries")
endfunction()

#
# Clean cache to reach size limit (LRU)
# SIZE_LIMIT_MB: Size limit in MB
#
function(nexus_cache_clean_by_size SIZE_LIMIT_MB)
    # Convert MB to bytes
    math(EXPR SIZE_LIMIT "${SIZE_LIMIT_MB} * 1024 * 1024")

    # Get all cache files with timestamps
    file(GLOB_RECURSE CACHE_FILES ${NEXUS_CACHE_LOCAL_DIR}/objects/*/*)

    # Calculate total size and collect file info
    set(TOTAL_SIZE 0)
    set(FILE_INFO_LIST "")

    foreach(CACHE_FILE ${CACHE_FILES})
        file(SIZE ${CACHE_FILE} FILE_SIZE)
        file(TIMESTAMP ${CACHE_FILE} FILE_TIME "%s")

        math(EXPR TOTAL_SIZE "${TOTAL_SIZE} + ${FILE_SIZE}")
        list(APPEND FILE_INFO_LIST "${FILE_TIME}:${FILE_SIZE}:${CACHE_FILE}")
    endforeach()

    # Check if cleanup needed
    if(TOTAL_SIZE LESS_EQUAL SIZE_LIMIT)
        message(STATUS "  Cache size (${TOTAL_SIZE} bytes) within limit")
        return()
    endif()

    # Sort by timestamp (oldest first)
    list(SORT FILE_INFO_LIST)

    # Remove oldest files until size limit reached
    set(CURRENT_SIZE ${TOTAL_SIZE})
    set(REMOVED_COUNT 0)

    foreach(FILE_INFO ${FILE_INFO_LIST})
        if(CURRENT_SIZE LESS_EQUAL SIZE_LIMIT)
            break()
        endif()

        # Parse file info
        string(REPLACE ":" ";" INFO_PARTS ${FILE_INFO})
        list(GET INFO_PARTS 1 FILE_SIZE)
        list(GET INFO_PARTS 2 CACHE_FILE)

        # Remove file
        file(REMOVE ${CACHE_FILE})

        # Remove metadata if exists
        get_filename_component(CACHE_KEY ${CACHE_FILE} NAME)
        if(EXISTS ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
            file(REMOVE ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
        endif()

        math(EXPR CURRENT_SIZE "${CURRENT_SIZE} - ${FILE_SIZE}")
        math(EXPR REMOVED_COUNT "${REMOVED_COUNT} + 1")
    endforeach()

    # Convert sizes to MB for display
    math(EXPR TOTAL_SIZE_MB "${TOTAL_SIZE} / 1024 / 1024")
    math(EXPR CURRENT_SIZE_MB "${CURRENT_SIZE} / 1024 / 1024")

    message(STATUS "  Removed ${REMOVED_COUNT} entries (${TOTAL_SIZE_MB} MB -> ${CURRENT_SIZE_MB} MB)")
endfunction()

##############################################################################
# Cache Statistics
##############################################################################

#
# Initialize cache statistics file
#
function(nexus_cache_init_stats)
    set(STATS_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/cache_stats.json)

    if(NOT EXISTS ${STATS_FILE})
        # Create initial statistics
        set(INITIAL_STATS "{\n")
        string(APPEND INITIAL_STATS "  \"cache_hits\": 0,\n")
        string(APPEND INITIAL_STATS "  \"cache_misses\": 0,\n")
        string(APPEND INITIAL_STATS "  \"total_requests\": 0,\n")
        string(APPEND INITIAL_STATS "  \"hit_rate\": 0.0,\n")
        string(APPEND INITIAL_STATS "  \"last_updated\": \"")
        string(TIMESTAMP CURRENT_TIME "%Y-%m-%dT%H:%M:%S")
        string(APPEND INITIAL_STATS "${CURRENT_TIME}\"\n")
        string(APPEND INITIAL_STATS "}")

        file(WRITE ${STATS_FILE} "${INITIAL_STATS}")
    endif()
endfunction()

#
# Update cache statistics
# EVENT_TYPE: Type of event (hit, miss, store)
# CACHE_KEY: Cache key (optional)
#
function(nexus_cache_update_stats EVENT_TYPE CACHE_KEY)
    if(NOT NEXUS_CACHE_ENABLED)
        return()
    endif()

    set(STATS_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/cache_stats.json)

    # Initialize if needed
    if(NOT EXISTS ${STATS_FILE})
        nexus_cache_init_stats()
    endif()

    # Read current statistics
    file(READ ${STATS_FILE} STATS_JSON)

    # Parse statistics
    string(JSON CACHE_HITS ERROR_VARIABLE ERR1 GET ${STATS_JSON} "cache_hits")
    string(JSON CACHE_MISSES ERROR_VARIABLE ERR2 GET ${STATS_JSON} "cache_misses")
    string(JSON TOTAL_REQUESTS ERROR_VARIABLE ERR3 GET ${STATS_JSON} "total_requests")

    # Handle parse errors
    if(ERR1 OR ERR2 OR ERR3)
        set(CACHE_HITS 0)
        set(CACHE_MISSES 0)
        set(TOTAL_REQUESTS 0)
    endif()

    # Update counters based on event type
    if(EVENT_TYPE STREQUAL "hit")
        math(EXPR CACHE_HITS "${CACHE_HITS} + 1")
        math(EXPR TOTAL_REQUESTS "${TOTAL_REQUESTS} + 1")
    elseif(EVENT_TYPE STREQUAL "miss")
        math(EXPR CACHE_MISSES "${CACHE_MISSES} + 1")
        math(EXPR TOTAL_REQUESTS "${TOTAL_REQUESTS} + 1")
    elseif(EVENT_TYPE STREQUAL "store")
        # Store doesn't affect hit/miss counters
    endif()

    # Calculate hit rate
    if(TOTAL_REQUESTS GREATER 0)
        math(EXPR HIT_RATE_INT "(${CACHE_HITS} * 100) / ${TOTAL_REQUESTS}")
        set(HIT_RATE "${HIT_RATE_INT}.0")
    else()
        set(HIT_RATE "0.0")
    endif()

    # Write updated statistics
    set(UPDATED_STATS "{\n")
    string(APPEND UPDATED_STATS "  \"cache_hits\": ${CACHE_HITS},\n")
    string(APPEND UPDATED_STATS "  \"cache_misses\": ${CACHE_MISSES},\n")
    string(APPEND UPDATED_STATS "  \"total_requests\": ${TOTAL_REQUESTS},\n")
    string(APPEND UPDATED_STATS "  \"hit_rate\": ${HIT_RATE},\n")
    string(APPEND UPDATED_STATS "  \"last_updated\": \"")
    string(TIMESTAMP CURRENT_TIME "%Y-%m-%dT%H:%M:%S")
    string(APPEND UPDATED_STATS "${CURRENT_TIME}\"\n")
    string(APPEND UPDATED_STATS "}")

    file(WRITE ${STATS_FILE} "${UPDATED_STATS}")
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

    # Display ccache/sccache statistics if available
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

    # Display Nexus cache statistics
    set(STATS_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/cache_stats.json)
    if(EXISTS ${STATS_FILE})
        file(READ ${STATS_FILE} STATS_JSON)

        # Parse statistics
        string(JSON CACHE_HITS ERROR_VARIABLE ERR1 GET ${STATS_JSON} "cache_hits")
        string(JSON CACHE_MISSES ERROR_VARIABLE ERR2 GET ${STATS_JSON} "cache_misses")
        string(JSON TOTAL_REQUESTS ERROR_VARIABLE ERR3 GET ${STATS_JSON} "total_requests")
        string(JSON HIT_RATE ERROR_VARIABLE ERR4 GET ${STATS_JSON} "hit_rate")
        string(JSON LAST_UPDATED ERROR_VARIABLE ERR5 GET ${STATS_JSON} "last_updated")

        if(NOT ERR1 AND NOT ERR2 AND NOT ERR3)
            message(STATUS "")
            message(STATUS "Nexus Cache Statistics:")
            message(STATUS "  Cache Hits:       ${CACHE_HITS}")
            message(STATUS "  Cache Misses:     ${CACHE_MISSES}")
            message(STATUS "  Total Requests:   ${TOTAL_REQUESTS}")
            message(STATUS "  Hit Rate:         ${HIT_RATE}%")
            if(NOT ERR5)
                message(STATUS "  Last Updated:     ${LAST_UPDATED}")
            endif()
        endif()
    endif()

    # Display cache size
    file(GLOB_RECURSE CACHE_FILES ${NEXUS_CACHE_LOCAL_DIR}/objects/*/*)
    list(LENGTH CACHE_FILES FILE_COUNT)

    set(TOTAL_SIZE 0)
    foreach(CACHE_FILE ${CACHE_FILES})
        file(SIZE ${CACHE_FILE} FILE_SIZE)
        math(EXPR TOTAL_SIZE "${TOTAL_SIZE} + ${FILE_SIZE}")
    endforeach()

    math(EXPR TOTAL_SIZE_MB "${TOTAL_SIZE} / 1024 / 1024")

    message(STATUS "")
    message(STATUS "Cache Storage:")
    message(STATUS "  Directory:        ${NEXUS_CACHE_LOCAL_DIR}")
    message(STATUS "  Cached Files:     ${FILE_COUNT}")
    message(STATUS "  Total Size:       ${TOTAL_SIZE_MB} MB")
    message(STATUS "  Size Limit:       ${NEXUS_CACHE_MAX_SIZE} MB")

    message(STATUS "==============================")
    message(STATUS "")
endfunction()

#
# Generate cache statistics report
# OUTPUT_FILE: Output file path (optional)
#
function(nexus_cache_generate_report)
    cmake_parse_arguments(
        ARG
        ""
        "OUTPUT_FILE"
        ""
        ${ARGN}
    )

    if(NOT NEXUS_CACHE_ENABLED)
        message(STATUS "Cache is not enabled")
        return()
    endif()

    # Read statistics
    set(STATS_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/cache_stats.json)
    if(NOT EXISTS ${STATS_FILE})
        nexus_cache_init_stats()
        file(READ ${STATS_FILE} STATS_JSON)
    else()
        file(READ ${STATS_FILE} STATS_JSON)
    endif()

    # Calculate cache size
    file(GLOB_RECURSE CACHE_FILES ${NEXUS_CACHE_LOCAL_DIR}/objects/*/*)
    list(LENGTH CACHE_FILES FILE_COUNT)

    set(TOTAL_SIZE 0)
    foreach(CACHE_FILE ${CACHE_FILES})
        file(SIZE ${CACHE_FILE} FILE_SIZE)
        math(EXPR TOTAL_SIZE "${TOTAL_SIZE} + ${FILE_SIZE}")
    endforeach()

    math(EXPR TOTAL_SIZE_MB "${TOTAL_SIZE} / 1024 / 1024")

    # Generate report
    set(REPORT "# Nexus Build Cache Report\n\n")
    string(APPEND REPORT "Generated: ")
    string(TIMESTAMP CURRENT_TIME "%Y-%m-%d %H:%M:%S")
    string(APPEND REPORT "${CURRENT_TIME}\n\n")

    string(APPEND REPORT "## Cache Configuration\n\n")
    string(APPEND REPORT "- **Cache Directory**: ${NEXUS_CACHE_LOCAL_DIR}\n")
    string(APPEND REPORT "- **Max Size**: ${NEXUS_CACHE_MAX_SIZE} MB\n")
    string(APPEND REPORT "- **Compression Level**: ${NEXUS_CACHE_COMPRESSION_LEVEL}\n")
    string(APPEND REPORT "- **Encryption**: ${NEXUS_CACHE_ENCRYPTION}\n")
    string(APPEND REPORT "- **TTL**: ${NEXUS_CACHE_TTL} seconds\n\n")

    string(APPEND REPORT "## Cache Statistics\n\n")
    string(JSON CACHE_HITS ERROR_VARIABLE ERR1 GET ${STATS_JSON} "cache_hits")
    string(JSON CACHE_MISSES ERROR_VARIABLE ERR2 GET ${STATS_JSON} "cache_misses")
    string(JSON TOTAL_REQUESTS ERROR_VARIABLE ERR3 GET ${STATS_JSON} "total_requests")
    string(JSON HIT_RATE ERROR_VARIABLE ERR4 GET ${STATS_JSON} "hit_rate")

    if(NOT ERR1 AND NOT ERR2 AND NOT ERR3)
        string(APPEND REPORT "- **Cache Hits**: ${CACHE_HITS}\n")
        string(APPEND REPORT "- **Cache Misses**: ${CACHE_MISSES}\n")
        string(APPEND REPORT "- **Total Requests**: ${TOTAL_REQUESTS}\n")
        string(APPEND REPORT "- **Hit Rate**: ${HIT_RATE}%\n\n")
    endif()

    string(APPEND REPORT "## Cache Storage\n\n")
    string(APPEND REPORT "- **Cached Files**: ${FILE_COUNT}\n")
    string(APPEND REPORT "- **Total Size**: ${TOTAL_SIZE_MB} MB\n")
    string(APPEND REPORT "- **Size Limit**: ${NEXUS_CACHE_MAX_SIZE} MB\n")

    # Calculate usage percentage
    if(NEXUS_CACHE_MAX_SIZE GREATER 0)
        math(EXPR USAGE_PCT "(${TOTAL_SIZE_MB} * 100) / ${NEXUS_CACHE_MAX_SIZE}")
        string(APPEND REPORT "- **Usage**: ${USAGE_PCT}%\n")
    endif()

    # Write report to file or display
    if(ARG_OUTPUT_FILE)
        file(WRITE ${ARG_OUTPUT_FILE} "${REPORT}")
        message(STATUS "Cache report written to: ${ARG_OUTPUT_FILE}")
    else()
        message(STATUS "${REPORT}")
    endif()
endfunction()

##############################################################################
# Content Hashing
##############################################################################

#
# Compute SHA-256 hash of file content
# FILE_PATH: Path to file
# OUTPUT_VAR: Variable to store hash
#
function(nexus_compute_file_hash FILE_PATH OUTPUT_VAR)
    if(NOT EXISTS ${FILE_PATH})
        message(WARNING "File does not exist: ${FILE_PATH}")
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
        return()
    endif()

    # Read file content
    file(READ ${FILE_PATH} FILE_CONTENT)

    # Compute SHA-256 hash
    string(SHA256 FILE_HASH "${FILE_CONTENT}")

    # Return hash
    set(${OUTPUT_VAR} ${FILE_HASH} PARENT_SCOPE)
endfunction()

#
# Compute hash of multiple files
# FILES: List of file paths
# OUTPUT_VAR: Variable to store combined hash
#
function(nexus_compute_files_hash FILES OUTPUT_VAR)
    set(COMBINED_CONTENT "")

    foreach(FILE ${FILES})
        if(EXISTS ${FILE})
            file(READ ${FILE} FILE_CONTENT)
            string(APPEND COMBINED_CONTENT "${FILE_CONTENT}")
        endif()
    endforeach()

    # Compute combined hash
    string(SHA256 COMBINED_HASH "${COMBINED_CONTENT}")

    # Return hash
    set(${OUTPUT_VAR} ${COMBINED_HASH} PARENT_SCOPE)
endfunction()

##############################################################################
# Cache Key Generation
##############################################################################

#
# Generate cache key for source file
# SOURCE_FILE: Source file path
# COMPILE_FLAGS: Compile flags
# OUTPUT_VAR: Variable to store cache key
#
function(nexus_generate_cache_key SOURCE_FILE COMPILE_FLAGS OUTPUT_VAR)
    # Compute source file hash
    nexus_compute_file_hash(${SOURCE_FILE} SOURCE_HASH)

    # Compute flags hash
    string(SHA256 FLAGS_HASH "${COMPILE_FLAGS}")

    # Get compiler version
    set(COMPILER_VERSION "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
    string(SHA256 COMPILER_HASH "${COMPILER_VERSION}")

    # Get toolchain hash if available
    if(CMAKE_TOOLCHAIN_FILE)
        nexus_compute_file_hash(${CMAKE_TOOLCHAIN_FILE} TOOLCHAIN_HASH)
    else()
        set(TOOLCHAIN_HASH "no-toolchain")
    endif()

    # Get Kconfig hash if available
    if(EXISTS ${CMAKE_BINARY_DIR}/nexus_config.h)
        nexus_compute_file_hash(${CMAKE_BINARY_DIR}/nexus_config.h CONFIG_HASH)
    else()
        set(CONFIG_HASH "no-config")
    endif()

    # Combine all hashes to create cache key
    string(SHA256 CACHE_KEY "${SOURCE_HASH}${FLAGS_HASH}${COMPILER_HASH}${TOOLCHAIN_HASH}${CONFIG_HASH}")

    # Return cache key
    set(${OUTPUT_VAR} ${CACHE_KEY} PARENT_SCOPE)
endfunction()

#
# Generate cache key with dependency tracking
# SOURCE_FILE: Source file path
# DEPENDENCIES: List of dependency files (headers)
# COMPILE_FLAGS: Compile flags
# OUTPUT_VAR: Variable to store cache key
#
function(nexus_generate_cache_key_with_deps SOURCE_FILE DEPENDENCIES COMPILE_FLAGS OUTPUT_VAR)
    # Compute source file hash
    nexus_compute_file_hash(${SOURCE_FILE} SOURCE_HASH)

    # Compute dependencies hash
    if(DEPENDENCIES)
        nexus_compute_files_hash("${DEPENDENCIES}" DEPS_HASH)
    else()
        set(DEPS_HASH "no-deps")
    endif()

    # Compute flags hash
    string(SHA256 FLAGS_HASH "${COMPILE_FLAGS}")

    # Get compiler version
    set(COMPILER_VERSION "${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")
    string(SHA256 COMPILER_HASH "${COMPILER_VERSION}")

    # Get toolchain hash if available
    if(CMAKE_TOOLCHAIN_FILE)
        nexus_compute_file_hash(${CMAKE_TOOLCHAIN_FILE} TOOLCHAIN_HASH)
    else()
        set(TOOLCHAIN_HASH "no-toolchain")
    endif()

    # Get Kconfig hash if available
    if(EXISTS ${CMAKE_BINARY_DIR}/nexus_config.h)
        nexus_compute_file_hash(${CMAKE_BINARY_DIR}/nexus_config.h CONFIG_HASH)
    else()
        set(CONFIG_HASH "no-config")
    endif()

    # Combine all hashes to create cache key
    string(SHA256 CACHE_KEY "${SOURCE_HASH}${DEPS_HASH}${FLAGS_HASH}${COMPILER_HASH}${TOOLCHAIN_HASH}${CONFIG_HASH}")

    # Return cache key
    set(${OUTPUT_VAR} ${CACHE_KEY} PARENT_SCOPE)
endfunction()

##############################################################################
# Cache Compression and Encryption
##############################################################################

#
# Compress cache file
# INPUT_FILE: Input file path
# OUTPUT_FILE: Output compressed file path
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_compress INPUT_FILE OUTPUT_FILE SUCCESS_VAR)
    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Use gzip for compression (available on most systems)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar czf ${OUTPUT_FILE} ${INPUT_FILE}
        RESULT_VARIABLE COMPRESS_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(COMPRESS_RESULT EQUAL 0 AND EXISTS ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Compressed cache file: ${INPUT_FILE}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to compress cache file: ${INPUT_FILE}")
    endif()
endfunction()

#
# Decompress cache file
# INPUT_FILE: Input compressed file path
# OUTPUT_DIR: Output directory for decompressed file
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_decompress INPUT_FILE OUTPUT_DIR SUCCESS_VAR)
    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Create output directory
    file(MAKE_DIRECTORY ${OUTPUT_DIR})

    # Use gzip for decompression
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${INPUT_FILE}
        WORKING_DIRECTORY ${OUTPUT_DIR}
        RESULT_VARIABLE DECOMPRESS_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(DECOMPRESS_RESULT EQUAL 0)
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Decompressed cache file: ${INPUT_FILE}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to decompress cache file: ${INPUT_FILE}")
    endif()
endfunction()

#
# Encrypt cache file using OpenSSL (if available)
# INPUT_FILE: Input file path
# OUTPUT_FILE: Output encrypted file path
# KEY: Encryption key (optional, uses default if not provided)
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_encrypt INPUT_FILE OUTPUT_FILE SUCCESS_VAR)
    cmake_parse_arguments(
        ARG
        ""
        "KEY"
        ""
        ${ARGN}
    )

    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Check if OpenSSL is available
    find_program(OPENSSL_PROGRAM openssl)

    if(NOT OPENSSL_PROGRAM)
        message(VERBOSE "OpenSSL not found, skipping encryption")
        # Just copy the file without encryption
        file(COPY ${INPUT_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        get_filename_component(INPUT_NAME ${INPUT_FILE} NAME)
        file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${INPUT_NAME} ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Use default key if not provided
    if(NOT ARG_KEY)
        set(ARG_KEY "nexus-cache-default-key-2026")
    endif()

    # Encrypt using AES-256-CBC
    execute_process(
        COMMAND ${OPENSSL_PROGRAM} enc -aes-256-cbc
            -salt
            -in ${INPUT_FILE}
            -out ${OUTPUT_FILE}
            -k ${ARG_KEY}
            -pbkdf2
        RESULT_VARIABLE ENCRYPT_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(ENCRYPT_RESULT EQUAL 0 AND EXISTS ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Encrypted cache file: ${INPUT_FILE}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to encrypt cache file: ${INPUT_FILE}")
    endif()
endfunction()

#
# Decrypt cache file using OpenSSL (if available)
# INPUT_FILE: Input encrypted file path
# OUTPUT_FILE: Output decrypted file path
# KEY: Decryption key (optional, uses default if not provided)
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_decrypt INPUT_FILE OUTPUT_FILE SUCCESS_VAR)
    cmake_parse_arguments(
        ARG
        ""
        "KEY"
        ""
        ${ARGN}
    )

    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Check if OpenSSL is available
    find_program(OPENSSL_PROGRAM openssl)

    if(NOT OPENSSL_PROGRAM)
        message(VERBOSE "OpenSSL not found, assuming file is not encrypted")
        # Just copy the file
        file(COPY ${INPUT_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        get_filename_component(INPUT_NAME ${INPUT_FILE} NAME)
        file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${INPUT_NAME} ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Use default key if not provided
    if(NOT ARG_KEY)
        set(ARG_KEY "nexus-cache-default-key-2026")
    endif()

    # Decrypt using AES-256-CBC
    execute_process(
        COMMAND ${OPENSSL_PROGRAM} enc -aes-256-cbc -d
            -in ${INPUT_FILE}
            -out ${OUTPUT_FILE}
            -k ${ARG_KEY}
            -pbkdf2
        RESULT_VARIABLE DECRYPT_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(DECRYPT_RESULT EQUAL 0 AND EXISTS ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Decrypted cache file: ${INPUT_FILE}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to decrypt cache file: ${INPUT_FILE}")
    endif()
endfunction()

#
# Process cache file with compression and/or encryption
# INPUT_FILE: Input file path
# OUTPUT_FILE: Output processed file path
# COMPRESS: Enable compression (optional)
# ENCRYPT: Enable encryption (optional)
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_process_file INPUT_FILE OUTPUT_FILE SUCCESS_VAR)
    cmake_parse_arguments(
        ARG
        "COMPRESS;ENCRYPT"
        ""
        ""
        ${ARGN}
    )

    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    set(CURRENT_FILE ${INPUT_FILE})
    set(TEMP_DIR ${NEXUS_CACHE_LOCAL_DIR}/temp)
    file(MAKE_DIRECTORY ${TEMP_DIR})

    # Apply compression if enabled
    if(ARG_COMPRESS OR NEXUS_CACHE_COMPRESSION_LEVEL GREATER 0)
        set(COMPRESSED_FILE ${TEMP_DIR}/compressed_${CACHE_KEY})
        nexus_cache_compress(${CURRENT_FILE} ${COMPRESSED_FILE} COMPRESS_SUCCESS)

        if(COMPRESS_SUCCESS)
            set(CURRENT_FILE ${COMPRESSED_FILE})
        else()
            message(VERBOSE "Compression failed, using uncompressed file")
        endif()
    endif()

    # Apply encryption if enabled
    if(ARG_ENCRYPT OR NEXUS_CACHE_ENCRYPTION)
        set(ENCRYPTED_FILE ${TEMP_DIR}/encrypted_${CACHE_KEY})
        nexus_cache_encrypt(${CURRENT_FILE} ${ENCRYPTED_FILE} ENCRYPT_SUCCESS)

        if(ENCRYPT_SUCCESS)
            set(CURRENT_FILE ${ENCRYPTED_FILE})
        else()
            message(VERBOSE "Encryption failed, using unencrypted file")
        endif()
    endif()

    # Copy final processed file to output
    file(COPY ${CURRENT_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    get_filename_component(CURRENT_NAME ${CURRENT_FILE} NAME)
    file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_NAME} ${OUTPUT_FILE})

    # Clean up temporary files
    if(EXISTS ${COMPRESSED_FILE})
        file(REMOVE ${COMPRESSED_FILE})
    endif()
    if(EXISTS ${ENCRYPTED_FILE})
        file(REMOVE ${ENCRYPTED_FILE})
    endif()

    set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
endfunction()

#
# Unprocess cache file (decrypt and/or decompress)
# INPUT_FILE: Input processed file path
# OUTPUT_FILE: Output unprocessed file path
# COMPRESSED: File is compressed (optional)
# ENCRYPTED: File is encrypted (optional)
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_unprocess_file INPUT_FILE OUTPUT_FILE SUCCESS_VAR)
    cmake_parse_arguments(
        ARG
        "COMPRESSED;ENCRYPTED"
        ""
        ""
        ${ARGN}
    )

    if(NOT EXISTS ${INPUT_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    set(CURRENT_FILE ${INPUT_FILE})
    set(TEMP_DIR ${NEXUS_CACHE_LOCAL_DIR}/temp)
    file(MAKE_DIRECTORY ${TEMP_DIR})

    # Decrypt if encrypted
    if(ARG_ENCRYPTED OR NEXUS_CACHE_ENCRYPTION)
        set(DECRYPTED_FILE ${TEMP_DIR}/decrypted_${CACHE_KEY})
        nexus_cache_decrypt(${CURRENT_FILE} ${DECRYPTED_FILE} DECRYPT_SUCCESS)

        if(DECRYPT_SUCCESS)
            set(CURRENT_FILE ${DECRYPTED_FILE})
        else()
            message(VERBOSE "Decryption failed, assuming file is not encrypted")
        endif()
    endif()

    # Decompress if compressed
    if(ARG_COMPRESSED OR NEXUS_CACHE_COMPRESSION_LEVEL GREATER 0)
        set(DECOMPRESSED_DIR ${TEMP_DIR}/decompressed)
        nexus_cache_decompress(${CURRENT_FILE} ${DECOMPRESSED_DIR} DECOMPRESS_SUCCESS)

        if(DECOMPRESS_SUCCESS)
            # Find the decompressed file
            file(GLOB DECOMPRESSED_FILES ${DECOMPRESSED_DIR}/*)
            if(DECOMPRESSED_FILES)
                list(GET DECOMPRESSED_FILES 0 CURRENT_FILE)
            endif()
        else()
            message(VERBOSE "Decompression failed, assuming file is not compressed")
        endif()
    endif()

    # Copy final unprocessed file to output
    file(COPY ${CURRENT_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    get_filename_component(CURRENT_NAME ${CURRENT_FILE} NAME)
    file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_NAME} ${OUTPUT_FILE})

    # Clean up temporary files
    if(EXISTS ${DECRYPTED_FILE})
        file(REMOVE ${DECRYPTED_FILE})
    endif()
    if(EXISTS ${DECOMPRESSED_DIR})
        file(REMOVE_RECURSE ${DECOMPRESSED_DIR})
    endif()

    set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
endfunction()

##############################################################################
# Remote Cache Support
##############################################################################

#
# Upload cache entry to remote cache
# CACHE_KEY: Cache key
# CACHE_FILE: Local cache file path
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_remote_upload CACHE_KEY CACHE_FILE SUCCESS_VAR)
    if(NOT NEXUS_CACHE_REMOTE_URL)
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    if(NOT EXISTS ${CACHE_FILE})
        message(WARNING "Cache file does not exist: ${CACHE_FILE}")
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Process file (compress and/or encrypt) before upload
    set(PROCESSED_FILE ${NEXUS_CACHE_LOCAL_DIR}/temp/upload_${CACHE_KEY})
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/temp)

    nexus_cache_process_file(${CACHE_FILE} ${PROCESSED_FILE} PROCESS_SUCCESS
        COMPRESS ENCRYPT
    )

    if(NOT PROCESS_SUCCESS)
        message(WARNING "Failed to process cache file for upload")
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Construct remote URL
    string(SUBSTRING ${CACHE_KEY} 0 2 CACHE_SUBDIR)
    set(REMOTE_URL "${NEXUS_CACHE_REMOTE_URL}/${CACHE_SUBDIR}/${CACHE_KEY}")

    # Upload file using HTTP PUT
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            curl -X PUT
            --silent
            --show-error
            --fail
            --max-time 30
            --upload-file ${PROCESSED_FILE}
            ${REMOTE_URL}
        RESULT_VARIABLE UPLOAD_RESULT
        OUTPUT_VARIABLE UPLOAD_OUTPUT
        ERROR_VARIABLE UPLOAD_ERROR
        TIMEOUT 35
    )

    # Clean up processed file
    file(REMOVE ${PROCESSED_FILE})

    if(UPLOAD_RESULT EQUAL 0)
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Uploaded cache entry to remote: ${CACHE_KEY}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to upload cache entry: ${UPLOAD_ERROR}")
    endif()
endfunction()

#
# Download cache entry from remote cache
# CACHE_KEY: Cache key
# OUTPUT_FILE: Destination path for cache file
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_remote_download CACHE_KEY OUTPUT_FILE SUCCESS_VAR)
    if(NOT NEXUS_CACHE_REMOTE_URL)
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Construct remote URL
    string(SUBSTRING ${CACHE_KEY} 0 2 CACHE_SUBDIR)
    set(REMOTE_URL "${NEXUS_CACHE_REMOTE_URL}/${CACHE_SUBDIR}/${CACHE_KEY}")

    # Download to temporary file
    set(DOWNLOADED_FILE ${NEXUS_CACHE_LOCAL_DIR}/temp/download_${CACHE_KEY})
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/temp)

    # Download file using HTTP GET
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            curl -X GET
            --silent
            --show-error
            --fail
            --max-time 30
            --output ${DOWNLOADED_FILE}
            ${REMOTE_URL}
        RESULT_VARIABLE DOWNLOAD_RESULT
        OUTPUT_VARIABLE DOWNLOAD_OUTPUT
        ERROR_VARIABLE DOWNLOAD_ERROR
        TIMEOUT 35
    )

    if(NOT DOWNLOAD_RESULT EQUAL 0 OR NOT EXISTS ${DOWNLOADED_FILE})
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to download cache entry: ${DOWNLOAD_ERROR}")

        # Clean up partial download
        if(EXISTS ${DOWNLOADED_FILE})
            file(REMOVE ${DOWNLOADED_FILE})
        endif()
        return()
    endif()

    # Unprocess file (decrypt and/or decompress) after download
    nexus_cache_unprocess_file(${DOWNLOADED_FILE} ${OUTPUT_FILE} UNPROCESS_SUCCESS
        COMPRESSED ENCRYPTED
    )

    # Clean up downloaded file
    file(REMOVE ${DOWNLOADED_FILE})

    if(UNPROCESS_SUCCESS AND EXISTS ${OUTPUT_FILE})
        set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
        message(VERBOSE "Downloaded cache entry from remote: ${CACHE_KEY}")
    else()
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        message(VERBOSE "Failed to unprocess downloaded cache entry")

        # Clean up partial output
        if(EXISTS ${OUTPUT_FILE})
            file(REMOVE ${OUTPUT_FILE})
        endif()
    endif()
endfunction()

#
# Synchronize local cache with remote cache
# DIRECTION: Sync direction (PUSH, PULL, or BOTH)
#
function(nexus_cache_sync)
    cmake_parse_arguments(
        ARG
        ""
        "DIRECTION"
        ""
        ${ARGN}
    )

    if(NOT NEXUS_CACHE_REMOTE_URL)
        message(STATUS "Remote cache not configured, skipping sync")
        return()
    endif()

    if(NOT ARG_DIRECTION)
        set(ARG_DIRECTION "BOTH")
    endif()

    message(STATUS "Synchronizing cache with remote (${ARG_DIRECTION})...")

    # Get local cache files
    file(GLOB_RECURSE LOCAL_CACHE_FILES ${NEXUS_CACHE_LOCAL_DIR}/objects/*/*)
    list(LENGTH LOCAL_CACHE_FILES LOCAL_COUNT)

    set(UPLOADED_COUNT 0)
    set(DOWNLOADED_COUNT 0)

    # Push local entries to remote
    if(ARG_DIRECTION STREQUAL "PUSH" OR ARG_DIRECTION STREQUAL "BOTH")
        message(STATUS "  Uploading local cache entries...")

        foreach(CACHE_FILE ${LOCAL_CACHE_FILES})
            get_filename_component(CACHE_KEY ${CACHE_FILE} NAME)

            # Upload to remote
            nexus_cache_remote_upload(${CACHE_KEY} ${CACHE_FILE} SUCCESS)

            if(SUCCESS)
                math(EXPR UPLOADED_COUNT "${UPLOADED_COUNT} + 1")
            endif()
        endforeach()

        message(STATUS "  Uploaded ${UPLOADED_COUNT} entries")
    endif()

    # Pull remote entries to local (simplified - would need remote listing)
    if(ARG_DIRECTION STREQUAL "PULL" OR ARG_DIRECTION STREQUAL "BOTH")
        message(STATUS "  Note: Pull requires remote cache listing support")
    endif()

    message(STATUS "Cache synchronization complete")
endfunction()

##############################################################################
# Cache Storage
##############################################################################

#
# Store object file in cache
# CACHE_KEY: Cache key
# OBJECT_FILE: Object file to cache
# METADATA: Optional metadata (JSON string)
#
function(nexus_cache_store CACHE_KEY OBJECT_FILE)
    cmake_parse_arguments(
        ARG
        ""
        "METADATA"
        ""
        ${ARGN}
    )

    if(NOT NEXUS_CACHE_ENABLED)
        return()
    endif()

    if(NOT EXISTS ${OBJECT_FILE})
        message(WARNING "Object file does not exist: ${OBJECT_FILE}")
        return()
    endif()

    # Create cache subdirectory based on first 2 chars of key
    string(SUBSTRING ${CACHE_KEY} 0 2 CACHE_SUBDIR)
    file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR})

    # Copy object file to cache
    set(CACHE_FILE ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR}/${CACHE_KEY})
    file(COPY ${OBJECT_FILE} DESTINATION ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR})
    file(RENAME
        ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR}/${CMAKE_MATCH_1}
        ${CACHE_FILE}
    )

    # Store metadata if provided
    if(ARG_METADATA)
        file(WRITE ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json "${ARG_METADATA}")
    endif()

    # Upload to remote cache if configured
    if(NEXUS_CACHE_REMOTE_URL)
        nexus_cache_remote_upload(${CACHE_KEY} ${CACHE_FILE} UPLOAD_SUCCESS)

        # Also upload metadata if exists
        if(ARG_METADATA AND UPLOAD_SUCCESS)
            set(METADATA_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
            if(EXISTS ${METADATA_FILE})
                nexus_cache_remote_upload("${CACHE_KEY}.meta" ${METADATA_FILE} META_SUCCESS)
            endif()
        endif()
    endif()

    # Update cache statistics
    nexus_cache_update_stats("store" ${CACHE_KEY})
endfunction()

#
# Retrieve object file from cache
# CACHE_KEY: Cache key
# OUTPUT_FILE: Destination path for object file
# SUCCESS_VAR: Variable to store success status
#
function(nexus_cache_retrieve CACHE_KEY OUTPUT_FILE SUCCESS_VAR)
    if(NOT NEXUS_CACHE_ENABLED)
        set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Locate cache file
    string(SUBSTRING ${CACHE_KEY} 0 2 CACHE_SUBDIR)
    set(CACHE_FILE ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR}/${CACHE_KEY})

    # Try local cache first
    if(EXISTS ${CACHE_FILE})
        # Verify cache integrity
        nexus_cache_verify_integrity(${CACHE_KEY} ${CACHE_FILE} VALID)

        if(VALID)
            # Copy from cache to output
            file(COPY ${CACHE_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
            file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${CACHE_KEY} ${OUTPUT_FILE})

            set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
            nexus_cache_update_stats("hit" ${CACHE_KEY})
            return()
        else()
            message(WARNING "Cache entry corrupted: ${CACHE_KEY}")
            file(REMOVE ${CACHE_FILE})
        endif()
    endif()

    # Try remote cache if local cache missed
    if(NEXUS_CACHE_REMOTE_URL)
        message(VERBOSE "Local cache miss, trying remote cache for: ${CACHE_KEY}")

        # Create temporary file for download
        set(TEMP_FILE ${NEXUS_CACHE_LOCAL_DIR}/temp/${CACHE_KEY})
        file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/temp)

        # Download from remote
        nexus_cache_remote_download(${CACHE_KEY} ${TEMP_FILE} DOWNLOAD_SUCCESS)

        if(DOWNLOAD_SUCCESS)
            # Store in local cache for future use
            file(MAKE_DIRECTORY ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR})
            file(COPY ${TEMP_FILE} DESTINATION ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR})
            file(RENAME
                ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR}/${CACHE_KEY}
                ${CACHE_FILE}
            )

            # Copy to output
            file(COPY ${TEMP_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
            file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${CACHE_KEY} ${OUTPUT_FILE})

            # Clean up temp file
            file(REMOVE ${TEMP_FILE})

            set(${SUCCESS_VAR} TRUE PARENT_SCOPE)
            nexus_cache_update_stats("hit" ${CACHE_KEY})
            message(VERBOSE "Remote cache hit: ${CACHE_KEY}")
            return()
        endif()
    endif()

    # Both local and remote cache missed
    set(${SUCCESS_VAR} FALSE PARENT_SCOPE)
    nexus_cache_update_stats("miss" ${CACHE_KEY})
endfunction()

#
# Verify cache entry integrity
# CACHE_KEY: Cache key
# CACHE_FILE: Cache file path
# VALID_VAR: Variable to store validity status
#
function(nexus_cache_verify_integrity CACHE_KEY CACHE_FILE VALID_VAR)
    # Check if file exists
    if(NOT EXISTS ${CACHE_FILE})
        set(${VALID_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Check if metadata exists
    set(METADATA_FILE ${NEXUS_CACHE_LOCAL_DIR}/metadata/${CACHE_KEY}.json)
    if(NOT EXISTS ${METADATA_FILE})
        # No metadata, assume valid (for ccache/sccache entries)
        set(${VALID_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Read metadata and verify hash
    file(READ ${METADATA_FILE} METADATA)
    string(JSON EXPECTED_HASH ERROR_VARIABLE JSON_ERROR GET ${METADATA} "hash")

    if(JSON_ERROR)
        # Metadata format error, assume valid
        set(${VALID_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Compute actual hash
    nexus_compute_file_hash(${CACHE_FILE} ACTUAL_HASH)

    # Compare hashes
    if(EXPECTED_HASH STREQUAL ACTUAL_HASH)
        set(${VALID_VAR} TRUE PARENT_SCOPE)
    else()
        set(${VALID_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

##############################################################################
# Cache Hit/Miss Logic
##############################################################################

#
# Check if cache entry exists and is valid
# CACHE_KEY: Cache key
# EXISTS_VAR: Variable to store existence status
#
function(nexus_cache_exists CACHE_KEY EXISTS_VAR)
    if(NOT NEXUS_CACHE_ENABLED)
        set(${EXISTS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Locate cache file
    string(SUBSTRING ${CACHE_KEY} 0 2 CACHE_SUBDIR)
    set(CACHE_FILE ${NEXUS_CACHE_LOCAL_DIR}/objects/${CACHE_SUBDIR}/${CACHE_KEY})

    if(NOT EXISTS ${CACHE_FILE})
        set(${EXISTS_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Verify integrity
    nexus_cache_verify_integrity(${CACHE_KEY} ${CACHE_FILE} VALID)
    set(${EXISTS_VAR} ${VALID} PARENT_SCOPE)
endfunction()

#
# Check cache and compile if needed
# SOURCE_FILE: Source file path
# OUTPUT_FILE: Output object file path
# COMPILE_FLAGS: Compile flags
# COMPILE_COMMAND: Compile command to execute on miss
# CACHE_HIT_VAR: Variable to store cache hit status
#
function(nexus_cache_check_and_compile SOURCE_FILE OUTPUT_FILE COMPILE_FLAGS COMPILE_COMMAND CACHE_HIT_VAR)
    # Generate cache key
    nexus_generate_cache_key(${SOURCE_FILE} "${COMPILE_FLAGS}" CACHE_KEY)

    # Check if cache exists
    nexus_cache_exists(${CACHE_KEY} CACHE_EXISTS)

    if(CACHE_EXISTS)
        # Cache hit - retrieve from cache
        nexus_cache_retrieve(${CACHE_KEY} ${OUTPUT_FILE} SUCCESS)

        if(SUCCESS)
            set(${CACHE_HIT_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    # Cache miss - compile
    execute_process(
        COMMAND ${COMPILE_COMMAND}
        RESULT_VARIABLE COMPILE_RESULT
        OUTPUT_VARIABLE COMPILE_OUTPUT
        ERROR_VARIABLE COMPILE_ERROR
    )

    if(NOT COMPILE_RESULT EQUAL 0)
        message(FATAL_ERROR "Compilation failed:\n${COMPILE_ERROR}")
    endif()

    # Store in cache
    if(EXISTS ${OUTPUT_FILE})
        # Create metadata
        nexus_compute_file_hash(${OUTPUT_FILE} OBJECT_HASH)
        string(TIMESTAMP CURRENT_TIME "%Y-%m-%dT%H:%M:%S")

        set(METADATA "{\n")
        string(APPEND METADATA "  \"cache_key\": \"${CACHE_KEY}\",\n")
        string(APPEND METADATA "  \"source_file\": \"${SOURCE_FILE}\",\n")
        string(APPEND METADATA "  \"object_file\": \"${OUTPUT_FILE}\",\n")
        string(APPEND METADATA "  \"hash\": \"${OBJECT_HASH}\",\n")
        string(APPEND METADATA "  \"compile_flags\": \"${COMPILE_FLAGS}\",\n")
        string(APPEND METADATA "  \"timestamp\": \"${CURRENT_TIME}\"\n")
        string(APPEND METADATA "}")

        nexus_cache_store(${CACHE_KEY} ${OUTPUT_FILE} METADATA "${METADATA}")
    endif()

    set(${CACHE_HIT_VAR} FALSE PARENT_SCOPE)
endfunction()
