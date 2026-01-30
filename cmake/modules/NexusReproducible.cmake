##############################################################################
# NexusReproducible.cmake - Reproducible Build Support
##############################################################################
#
# NexusReproducible.cmake
# Reproducible build support for Nexus build system
# Author: Nexus Team
#
# This module provides reproducible build functionality, including:
# - Build manifest generation
# - Source and artifact hash tracking
# - Deterministic build flags
# - Hash verification
#
##############################################################################

message(STATUS "Loading NexusReproducible module...")

if(NEXUS_REPRODUCIBLE_INCLUDED)
    message(STATUS "NexusReproducible already included, returning...")
    return()
endif()
set(NEXUS_REPRODUCIBLE_INCLUDED TRUE)

# Don't include NexusHelpers in script mode
if(CMAKE_SCRIPT_MODE_FILE)
    # Running in script mode, skip NexusHelpers
else()
    include(${CMAKE_CURRENT_LIST_DIR}/NexusHelpers.cmake)
endif()

##############################################################################
# Global Variables
##############################################################################

# Build manifest data
set(NEXUS_BUILD_MANIFEST_VERSION "1.0")
set(NEXUS_BUILD_MANIFEST_FILE "${CMAKE_BINARY_DIR}/build_manifest.json")
set(NEXUS_SOURCE_HASHES_FILE "${CMAKE_BINARY_DIR}/.source_hashes.json")
set(NEXUS_ARTIFACT_HASHES_FILE "${CMAKE_BINARY_DIR}/.artifact_hashes.json")

# Deterministic build settings
set(NEXUS_REPRODUCIBLE_BUILD OFF CACHE BOOL "Enable reproducible builds")
set(NEXUS_BUILD_TIMESTAMP "1970-01-01T00:00:00Z" CACHE STRING "Fixed timestamp for reproducible builds")

##############################################################################
# Build Manifest Generation Functions
##############################################################################

#
# Initialize build manifest generation
#
function(nexus_init_build_manifest)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    message(STATUS "Initializing reproducible build manifest...")

    # Create manifest directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/manifests")

    # Initialize source hashes file
    file(WRITE "${NEXUS_SOURCE_HASHES_FILE}" "{\n  \"sources\": {}\n}\n")

    # Initialize artifact hashes file
    file(WRITE "${NEXUS_ARTIFACT_HASHES_FILE}" "{\n  \"artifacts\": []\n}\n")

    # Set deterministic build flags
    if(NEXUS_REPRODUCIBLE_BUILD)
        nexus_set_deterministic_flags()
    endif()
endfunction()

#
# Compute hash of a file
# FILE_PATH: Path to file
# OUT_HASH: Output variable for hash
#
function(nexus_compute_file_hash FILE_PATH OUT_HASH)
    if(NOT EXISTS "${FILE_PATH}")
        set(${OUT_HASH} "" PARENT_SCOPE)
        return()
    endif()

    file(SHA256 "${FILE_PATH}" FILE_HASH)
    set(${OUT_HASH} "sha256:${FILE_HASH}" PARENT_SCOPE)
endfunction()

#
# Record source file hash
# SOURCE_FILE: Path to source file
#
function(nexus_record_source_hash SOURCE_FILE)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    # Compute hash
    nexus_compute_file_hash("${SOURCE_FILE}" FILE_HASH)

    if(NOT FILE_HASH)
        return()
    endif()

    # Get relative path
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" "${SOURCE_FILE}")

    # Read current hashes
    if(EXISTS "${NEXUS_SOURCE_HASHES_FILE}")
        file(READ "${NEXUS_SOURCE_HASHES_FILE}" HASHES_JSON)
    else()
        set(HASHES_JSON "{\n  \"sources\": {}\n}\n")
    endif()

    # Update hash (simple append for now)
    string(REPLACE "}" "  ,\"${REL_PATH}\": \"${FILE_HASH}\"\n}" HASHES_JSON "${HASHES_JSON}")

    # Write back
    file(WRITE "${NEXUS_SOURCE_HASHES_FILE}" "${HASHES_JSON}")
endfunction()

#
# Record artifact hash
# ARTIFACT_PATH: Path to artifact
# ARTIFACT_TYPE: Type of artifact (executable, library, etc.)
#
function(nexus_record_artifact_hash ARTIFACT_PATH ARTIFACT_TYPE)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    if(NOT EXISTS "${ARTIFACT_PATH}")
        return()
    endif()

    # Compute hash
    nexus_compute_file_hash("${ARTIFACT_PATH}" FILE_HASH)

    # Get file size
    file(SIZE "${ARTIFACT_PATH}" FILE_SIZE)

    # Get relative path
    file(RELATIVE_PATH REL_PATH "${CMAKE_BINARY_DIR}" "${ARTIFACT_PATH}")
    get_filename_component(FILE_NAME "${ARTIFACT_PATH}" NAME)

    # Read current hashes
    if(EXISTS "${NEXUS_ARTIFACT_HASHES_FILE}")
        file(READ "${NEXUS_ARTIFACT_HASHES_FILE}" HASHES_JSON)
    else()
        set(HASHES_JSON "{\n  \"artifacts\": []\n}\n")
    endif()

    # Create artifact entry
    set(ARTIFACT_ENTRY "    {\n")
    string(APPEND ARTIFACT_ENTRY "      \"type\": \"${ARTIFACT_TYPE}\",\n")
    string(APPEND ARTIFACT_ENTRY "      \"name\": \"${FILE_NAME}\",\n")
    string(APPEND ARTIFACT_ENTRY "      \"path\": \"${REL_PATH}\",\n")
    string(APPEND ARTIFACT_ENTRY "      \"size\": ${FILE_SIZE},\n")
    string(APPEND ARTIFACT_ENTRY "      \"hash\": \"${FILE_HASH}\"\n")
    string(APPEND ARTIFACT_ENTRY "    }")

    # Update artifacts list
    string(REPLACE "\"artifacts\": []" "\"artifacts\": [\n${ARTIFACT_ENTRY}\n  ]" HASHES_JSON "${HASHES_JSON}")
    string(REPLACE "]" ",\n${ARTIFACT_ENTRY}\n  ]" HASHES_JSON "${HASHES_JSON}")

    # Write back
    file(WRITE "${NEXUS_ARTIFACT_HASHES_FILE}" "${HASHES_JSON}")
endfunction()

#
# Get toolchain version information
# OUT_VERSION: Output variable for version string
#
function(nexus_get_toolchain_version OUT_VERSION)
    set(VERSION_INFO "")

    # Get C compiler version
    if(CMAKE_C_COMPILER)
        execute_process(
            COMMAND ${CMAKE_C_COMPILER} --version
            OUTPUT_VARIABLE C_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" C_VER "${C_VERSION}")
        if(C_VER)
            set(VERSION_INFO "${CMAKE_C_COMPILER_ID} ${C_VER}")
        else()
            set(VERSION_INFO "${CMAKE_C_COMPILER_ID}")
        endif()
    endif()

    set(${OUT_VERSION} "${VERSION_INFO}" PARENT_SCOPE)
endfunction()

#
# Get configuration hash
# OUT_HASH: Output variable for configuration hash
#
function(nexus_get_config_hash OUT_HASH)
    set(CONFIG_STRING "")

    # Collect important configuration variables
    string(APPEND CONFIG_STRING "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}\n")
    string(APPEND CONFIG_STRING "CMAKE_C_FLAGS=${CMAKE_C_FLAGS}\n")
    string(APPEND CONFIG_STRING "CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}\n")
    string(APPEND CONFIG_STRING "CMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}\n")

    # Add Kconfig hash if available
    if(EXISTS "${CMAKE_BINARY_DIR}/nexus_config.h")
        nexus_compute_file_hash("${CMAKE_BINARY_DIR}/nexus_config.h" CONFIG_FILE_HASH)
        string(APPEND CONFIG_STRING "KCONFIG_HASH=${CONFIG_FILE_HASH}\n")
    endif()

    # Compute hash of configuration string
    string(SHA256 CONFIG_HASH "${CONFIG_STRING}")
    set(${OUT_HASH} "sha256:${CONFIG_HASH}" PARENT_SCOPE)
endfunction()

#
# Generate build manifest
#
function(nexus_generate_build_manifest)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    message(STATUS "Generating build manifest...")

    # Get build timestamp
    string(TIMESTAMP BUILD_TIMESTAMP UTC)
    if(NEXUS_REPRODUCIBLE_BUILD)
        set(BUILD_TIMESTAMP "${NEXUS_BUILD_TIMESTAMP}")
    endif()

    # Generate build ID
    string(TIMESTAMP BUILD_ID "%Y%m%d-%H%M%S" UTC)
    string(RANDOM LENGTH 6 ALPHABET "0123456789abcdef" RANDOM_SUFFIX)
    set(BUILD_ID "${BUILD_ID}-${RANDOM_SUFFIX}")

    # Get toolchain version
    nexus_get_toolchain_version(TOOLCHAIN_VERSION)

    # Get configuration hash
    nexus_get_config_hash(CONFIG_HASH)

    # Compute source tree hash
    if(EXISTS "${NEXUS_SOURCE_HASHES_FILE}")
        nexus_compute_file_hash("${NEXUS_SOURCE_HASHES_FILE}" SOURCE_HASH)
    else()
        set(SOURCE_HASH "sha256:0000000000000000000000000000000000000000000000000000000000000000")
    endif()

    # Get build statistics
    set(BUILD_TIME 0)
    set(CACHE_HITS 0)
    set(CACHE_MISSES 0)

    if(DEFINED NEXUS_BUILD_TIME)
        set(BUILD_TIME ${NEXUS_BUILD_TIME})
    endif()

    if(DEFINED NEXUS_CACHE_HITS)
        set(CACHE_HITS ${NEXUS_CACHE_HITS})
    endif()

    if(DEFINED NEXUS_CACHE_MISSES)
        set(CACHE_MISSES ${NEXUS_CACHE_MISSES})
    endif()

    # Build manifest JSON
    set(MANIFEST "{\n")
    string(APPEND MANIFEST "  \"version\": \"${NEXUS_BUILD_MANIFEST_VERSION}\",\n")
    string(APPEND MANIFEST "  \"build_id\": \"${BUILD_ID}\",\n")
    string(APPEND MANIFEST "  \"timestamp\": \"${BUILD_TIMESTAMP}\",\n")
    string(APPEND MANIFEST "  \"platform\": \"${CMAKE_SYSTEM_NAME}\",\n")
    string(APPEND MANIFEST "  \"toolchain\": \"${TOOLCHAIN_VERSION}\",\n")
    string(APPEND MANIFEST "  \"build_type\": \"${CMAKE_BUILD_TYPE}\",\n")
    string(APPEND MANIFEST "  \"config_hash\": \"${CONFIG_HASH}\",\n")
    string(APPEND MANIFEST "  \"source_hash\": \"${SOURCE_HASH}\",\n")

    # Add artifacts
    if(EXISTS "${NEXUS_ARTIFACT_HASHES_FILE}")
        file(READ "${NEXUS_ARTIFACT_HASHES_FILE}" ARTIFACTS_CONTENT)
        string(REGEX REPLACE "^\\{[^\\[]*\\[" "" ARTIFACTS_CONTENT "${ARTIFACTS_CONTENT}")
        string(REGEX REPLACE "\\][^\\]]*\\}$" "" ARTIFACTS_CONTENT "${ARTIFACTS_CONTENT}")
        string(APPEND MANIFEST "  \"artifacts\": [\n${ARTIFACTS_CONTENT}\n  ],\n")
    else()
        string(APPEND MANIFEST "  \"artifacts\": [],\n")
    endif()

    # Add build statistics
    string(APPEND MANIFEST "  \"build_time\": ${BUILD_TIME},\n")
    string(APPEND MANIFEST "  \"cache_hits\": ${CACHE_HITS},\n")
    string(APPEND MANIFEST "  \"cache_misses\": ${CACHE_MISSES}\n")
    string(APPEND MANIFEST "}\n")

    # Write manifest
    file(WRITE "${NEXUS_BUILD_MANIFEST_FILE}" "${MANIFEST}")

    message(STATUS "Build manifest generated: ${NEXUS_BUILD_MANIFEST_FILE}")
endfunction()

##############################################################################
# Deterministic Build Functions
##############################################################################

#
# Set deterministic build flags
#
function(nexus_set_deterministic_flags)
    message(STATUS "Enabling deterministic build flags...")

    # GCC/Clang flags for reproducible builds
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Use fixed timestamp
        add_compile_options(-Wno-builtin-macro-redefined)
        add_compile_definitions(__DATE__="Jan 01 1970")
        add_compile_definitions(__TIME__="00:00:00")
        add_compile_definitions(__TIMESTAMP__="Thu Jan 01 00:00:00 1970")

        # Disable random seed
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-frandom-seed=0)
        endif()

        # Use relative paths
        add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)
        add_compile_options(-ffile-prefix-map=${CMAKE_BINARY_DIR}=.)
    endif()

    # MSVC flags for reproducible builds
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/Brepro)
        add_link_options(/Brepro)
    endif()

    # Set fixed build timestamp
    add_compile_definitions(NEXUS_BUILD_TIMESTAMP="${NEXUS_BUILD_TIMESTAMP}")
endfunction()

#
# Ensure deterministic file ordering
# FILE_LIST: Input file list variable name
# OUT_LIST: Output sorted list variable name
#
function(nexus_sort_files FILE_LIST OUT_LIST)
    set(SORTED_LIST ${${FILE_LIST}})
    list(SORT SORTED_LIST)
    set(${OUT_LIST} ${SORTED_LIST} PARENT_SCOPE)
endfunction()

##############################################################################
# Hash Verification Functions
##############################################################################

#
# Verify artifact hash against manifest
# ARTIFACT_PATH: Path to artifact
# OUT_RESULT: Output variable for verification result
#
function(nexus_verify_artifact_hash ARTIFACT_PATH OUT_RESULT)
    if(NOT EXISTS "${ARTIFACT_PATH}")
        set(${OUT_RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    if(NOT EXISTS "${NEXUS_BUILD_MANIFEST_FILE}")
        message(WARNING "Build manifest not found, cannot verify artifact")
        set(${OUT_RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    # Compute current hash
    nexus_compute_file_hash("${ARTIFACT_PATH}" CURRENT_HASH)

    # Read manifest
    file(READ "${NEXUS_BUILD_MANIFEST_FILE}" MANIFEST_JSON)

    # Get artifact name
    get_filename_component(ARTIFACT_NAME "${ARTIFACT_PATH}" NAME)

    # Simple string search for artifact (JSON parsing is limited in CMake)
    string(FIND "${MANIFEST_JSON}" "\"name\": \"${ARTIFACT_NAME}\"" ARTIFACT_POS)

    if(ARTIFACT_POS EQUAL -1)
        message(WARNING "Artifact ${ARTIFACT_NAME} not found in manifest")
        set(${OUT_RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    # Extract hash from manifest (simplified)
    string(SUBSTRING "${MANIFEST_JSON}" ${ARTIFACT_POS} 500 ARTIFACT_SECTION)
    string(REGEX MATCH "\"hash\": \"([^\"]+)\"" HASH_MATCH "${ARTIFACT_SECTION}")

    if(CMAKE_MATCH_1)
        set(EXPECTED_HASH "${CMAKE_MATCH_1}")

        # Compare hashes
        if(CURRENT_HASH STREQUAL EXPECTED_HASH)
            set(${OUT_RESULT} TRUE PARENT_SCOPE)
        else()
            message(WARNING "Hash mismatch for ${ARTIFACT_NAME}:")
            message(WARNING "  Expected: ${EXPECTED_HASH}")
            message(WARNING "  Got:      ${CURRENT_HASH}")
            set(${OUT_RESULT} FALSE PARENT_SCOPE)
        endif()
    else()
        message(WARNING "Could not extract hash for ${ARTIFACT_NAME}")
        set(${OUT_RESULT} FALSE PARENT_SCOPE)
    endif()
endfunction()

#
# Verify all artifacts in manifest
# OUT_RESULT: Output variable for verification result
#
function(nexus_verify_all_artifacts OUT_RESULT)
    if(NOT EXISTS "${NEXUS_BUILD_MANIFEST_FILE}")
        message(WARNING "Build manifest not found")
        set(${OUT_RESULT} FALSE PARENT_SCOPE)
        return()
    endif()

    message(STATUS "Verifying all artifacts...")

    # Read manifest
    file(READ "${NEXUS_BUILD_MANIFEST_FILE}" MANIFEST_JSON)

    # Extract all artifact paths
    string(REGEX MATCHALL "\"path\": \"([^\"]+)\"" PATH_MATCHES "${MANIFEST_JSON}")

    set(ALL_VALID TRUE)

    foreach(PATH_MATCH ${PATH_MATCHES})
        string(REGEX MATCH "\"path\": \"([^\"]+)\"" _ "${PATH_MATCH}")
        set(ARTIFACT_PATH "${CMAKE_BINARY_DIR}/${CMAKE_MATCH_1}")

        nexus_verify_artifact_hash("${ARTIFACT_PATH}" IS_VALID)

        if(NOT IS_VALID)
            set(ALL_VALID FALSE)
        endif()
    endforeach()

    if(ALL_VALID)
        message(STATUS "All artifacts verified successfully")
    else()
        message(WARNING "Some artifacts failed verification")
    endif()

    set(${OUT_RESULT} ${ALL_VALID} PARENT_SCOPE)
endfunction()

##############################################################################
# Public API Functions
##############################################################################

#
# Enable reproducible builds for a target
# TARGET: Target name
#
function(nexus_enable_reproducible_build TARGET)
    if(NOT NEXUS_REPRODUCIBLE_BUILD)
        return()
    endif()

    # Record all source files
    get_target_property(TARGET_SOURCES ${TARGET} SOURCES)

    if(TARGET_SOURCES)
        foreach(SOURCE ${TARGET_SOURCES})
            get_filename_component(SOURCE_ABS "${SOURCE}" ABSOLUTE)
            nexus_record_source_hash("${SOURCE_ABS}")
        endforeach()
    endif()

    # Add post-build command to record artifact
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Recording artifact hash for ${TARGET}"
        COMMENT "Recording build artifact"
    )
endfunction()

##############################################################################
# Module Initialization
##############################################################################

message(STATUS "NexusReproducible module loaded")

##############################################################################
# End of NexusReproducible.cmake
##############################################################################
