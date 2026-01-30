##############################################################################
# NexusDependency.cmake - Dependency Management Module
##############################################################################
#
# NexusDependency.cmake
# Dependency management for Nexus build system
# Author: Nexus Team
#
# This module provides dependency management functionality, including
# dependency resolution, version management, and lock file support.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Dependency Configuration
##############################################################################

# Dependency graph storage
# Format: "module_name:dep1;dep2;dep3"
set(NEXUS_DEPENDENCY_GRAPH "" CACHE INTERNAL "Dependency graph")
set(NEXUS_DEPENDENCY_VERSIONS "" CACHE INTERNAL "Dependency versions")
set(NEXUS_DEPENDENCY_INFO "" CACHE INTERNAL "Dependency information")

# Lock file path
set(NEXUS_LOCK_FILE "${CMAKE_SOURCE_DIR}/nexus.lock" CACHE FILEPATH "Dependency lock file")

##############################################################################
# Dependency Declaration Functions
##############################################################################

#
# Declare a dependency
# NAME: Dependency name
# VERSION: Version specification (SemVer)
# REQUIRED: Dependency is required (optional)
# COMPONENTS: Required components (optional)
# CONDITION: Kconfig condition (optional)
#
function(nexus_declare_dependency)
    cmake_parse_arguments(
        ARG
        "REQUIRED"
        "NAME;VERSION;CONDITION"
        "COMPONENTS"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "nexus_declare_dependency: NAME argument is required")
    endif()

    if(NOT ARG_VERSION)
        message(FATAL_ERROR "nexus_declare_dependency: VERSION argument is required")
    endif()

    # Check condition if specified
    if(ARG_CONDITION)
        nexus_evaluate_condition(${ARG_CONDITION} CONDITION_MET)
        if(NOT CONDITION_MET)
            message(STATUS "Skipping dependency ${ARG_NAME}: condition not met (${ARG_CONDITION})")
            return()
        endif()
    endif()

    # Store dependency information
    set(DEP_INFO "name=${ARG_NAME};version=${ARG_VERSION}")

    if(ARG_REQUIRED)
        string(APPEND DEP_INFO ";required=TRUE")
    else()
        string(APPEND DEP_INFO ";required=FALSE")
    endif()

    if(ARG_COMPONENTS)
        string(JOIN "," COMPONENTS_STR ${ARG_COMPONENTS})
        string(APPEND DEP_INFO ";components=${COMPONENTS_STR}")
    endif()

    if(ARG_CONDITION)
        string(APPEND DEP_INFO ";condition=${ARG_CONDITION}")
    endif()

    # Add to dependency info list
    list(APPEND NEXUS_DEPENDENCY_INFO ${DEP_INFO})
    set(NEXUS_DEPENDENCY_INFO ${NEXUS_DEPENDENCY_INFO} CACHE INTERNAL "Dependency information")

    message(STATUS "Declared dependency: ${ARG_NAME} (${ARG_VERSION})")

    # Try to find the dependency
    nexus_resolve_dependency(${ARG_NAME} ${ARG_VERSION})
endfunction()

#
# Add module to dependency graph
# MODULE: Module name
# DEPS: List of dependencies
# CONDITION: Kconfig condition (optional)
#
function(nexus_add_module_dependencies MODULE)
    cmake_parse_arguments(
        ARG
        ""
        "CONDITION"
        "DEPS"
        ${ARGN}
    )

    # Check condition if specified
    if(ARG_CONDITION)
        nexus_evaluate_condition(${ARG_CONDITION} CONDITION_MET)
        if(NOT CONDITION_MET)
            message(STATUS "Skipping module ${MODULE}: condition not met (${ARG_CONDITION})")
            return()
        endif()
    endif()

    # Create dependency entry
    if(ARG_DEPS)
        string(JOIN ";" DEPS_STR ${ARG_DEPS})
        set(GRAPH_ENTRY "${MODULE}:${DEPS_STR}")
    else()
        set(GRAPH_ENTRY "${MODULE}:")
    endif()

    # Add to dependency graph
    list(APPEND NEXUS_DEPENDENCY_GRAPH ${GRAPH_ENTRY})
    set(NEXUS_DEPENDENCY_GRAPH ${NEXUS_DEPENDENCY_GRAPH} CACHE INTERNAL "Dependency graph")

    message(STATUS "Added module to dependency graph: ${MODULE}")
endfunction()

#
# Evaluate Kconfig condition
# CONDITION: Condition expression (e.g., "CONFIG_FEATURE_A")
# RESULT: TRUE if condition is met
#
function(nexus_evaluate_condition CONDITION RESULT_VAR)
    set(RESULT FALSE)

    # Handle simple variable check
    if(CONDITION MATCHES "^CONFIG_([A-Z0-9_]+)$")
        # Check if config variable is defined and true
        if(DEFINED ${CONDITION} AND ${CONDITION})
            set(RESULT TRUE)
        endif()
    # Handle NOT condition
    elseif(CONDITION MATCHES "^NOT[[:space:]]+CONFIG_([A-Z0-9_]+)$")
        string(REGEX REPLACE "^NOT[[:space:]]+" "" VAR_NAME ${CONDITION})
        if(NOT DEFINED ${VAR_NAME} OR NOT ${VAR_NAME})
            set(RESULT TRUE)
        endif()
    # Handle AND condition
    elseif(CONDITION MATCHES "CONFIG_([A-Z0-9_]+)[[:space:]]+AND[[:space:]]+CONFIG_([A-Z0-9_]+)")
        set(VAR1 "CONFIG_${CMAKE_MATCH_1}")
        set(VAR2 "CONFIG_${CMAKE_MATCH_2}")
        if(DEFINED ${VAR1} AND ${VAR1} AND DEFINED ${VAR2} AND ${VAR2})
            set(RESULT TRUE)
        endif()
    # Handle OR condition
    elseif(CONDITION MATCHES "CONFIG_([A-Z0-9_]+)[[:space:]]+OR[[:space:]]+CONFIG_([A-Z0-9_]+)")
        set(VAR1 "CONFIG_${CMAKE_MATCH_1}")
        set(VAR2 "CONFIG_${CMAKE_MATCH_2}")
        if((DEFINED ${VAR1} AND ${VAR1}) OR (DEFINED ${VAR2} AND ${VAR2}))
            set(RESULT TRUE)
        endif()
    else()
        # Try to evaluate as CMake expression
        if(${CONDITION})
            set(RESULT TRUE)
        endif()
    endif()

    set(${RESULT_VAR} ${RESULT} PARENT_SCOPE)
endfunction()

#
# Update dependency graph based on Kconfig changes
#
function(nexus_update_conditional_dependencies)
    message(STATUS "Updating conditional dependencies...")

    # Clear current graph
    set(NEXUS_DEPENDENCY_GRAPH "" CACHE INTERNAL "Dependency graph")

    # Re-evaluate all dependencies with conditions
    foreach(INFO ${NEXUS_DEPENDENCY_INFO})
        # Parse dependency info
        if(INFO MATCHES "name=([^;]+)")
            set(DEP_NAME ${CMAKE_MATCH_1})
        endif()

        if(INFO MATCHES "condition=([^;]+)")
            set(DEP_CONDITION ${CMAKE_MATCH_1})

            # Re-evaluate condition
            nexus_evaluate_condition(${DEP_CONDITION} CONDITION_MET)

            if(CONDITION_MET)
                message(STATUS "  Including dependency: ${DEP_NAME}")
            else()
                message(STATUS "  Excluding dependency: ${DEP_NAME}")
            endif()
        endif()
    endforeach()

    message(STATUS "Conditional dependencies updated")
endfunction()

##############################################################################
# Dependency Resolution Functions
##############################################################################

#
# Parse SemVer version string
# VERSION: Version string (e.g., "1.2.3", ">=2.0.0", "^1.5.0")
# MAJOR: Major version
# MINOR: Minor version
# PATCH: Patch version
# OPERATOR: Version operator (empty, >=, <=, ^, ~)
#
function(nexus_parse_semver VERSION MAJOR_VAR MINOR_VAR PATCH_VAR OPERATOR_VAR)
    set(OPERATOR "")
    set(VERSION_NUM ${VERSION})

    # Extract operator
    if(VERSION MATCHES "^([><=^~]+)(.+)$")
        set(OPERATOR ${CMAKE_MATCH_1})
        set(VERSION_NUM ${CMAKE_MATCH_2})
    endif()

    # Parse version numbers
    if(VERSION_NUM MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR ${CMAKE_MATCH_2})
        set(PATCH ${CMAKE_MATCH_3})
    elseif(VERSION_NUM MATCHES "^([0-9]+)\\.([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR ${CMAKE_MATCH_2})
        set(PATCH 0)
    elseif(VERSION_NUM MATCHES "^([0-9]+)")
        set(MAJOR ${CMAKE_MATCH_1})
        set(MINOR 0)
        set(PATCH 0)
    else()
        message(FATAL_ERROR "Invalid version format: ${VERSION}")
    endif()

    # Set output variables
    set(${MAJOR_VAR} ${MAJOR} PARENT_SCOPE)
    set(${MINOR_VAR} ${MINOR} PARENT_SCOPE)
    set(${PATCH_VAR} ${PATCH} PARENT_SCOPE)
    set(${OPERATOR_VAR} ${OPERATOR} PARENT_SCOPE)
endfunction()

#
# Compare two SemVer versions
# VERSION1: First version
# VERSION2: Second version
# RESULT: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
#
function(nexus_compare_versions VERSION1 VERSION2 RESULT_VAR)
    # Parse versions
    nexus_parse_semver(${VERSION1} MAJOR1 MINOR1 PATCH1 OP1)
    nexus_parse_semver(${VERSION2} MAJOR2 MINOR2 PATCH2 OP2)

    # Compare major
    if(MAJOR1 LESS MAJOR2)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
        return()
    elseif(MAJOR1 GREATER MAJOR2)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
        return()
    endif()

    # Compare minor
    if(MINOR1 LESS MINOR2)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
        return()
    elseif(MINOR1 GREATER MINOR2)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
        return()
    endif()

    # Compare patch
    if(PATCH1 LESS PATCH2)
        set(${RESULT_VAR} -1 PARENT_SCOPE)
        return()
    elseif(PATCH1 GREATER PATCH2)
        set(${RESULT_VAR} 1 PARENT_SCOPE)
        return()
    endif()

    # Versions are equal
    set(${RESULT_VAR} 0 PARENT_SCOPE)
endfunction()

#
# Check if version satisfies constraint
# VERSION: Actual version
# CONSTRAINT: Version constraint (e.g., ">=1.2.0", "^2.0.0")
# SATISFIES: TRUE if version satisfies constraint
#
function(nexus_version_satisfies VERSION CONSTRAINT SATISFIES_VAR)
    # Parse constraint
    nexus_parse_semver(${CONSTRAINT} REQ_MAJOR REQ_MINOR REQ_PATCH OPERATOR)

    # Parse actual version
    nexus_parse_semver(${VERSION} ACT_MAJOR ACT_MINOR ACT_PATCH ACT_OP)

    set(SATISFIES FALSE)

    # Handle different operators
    if(NOT OPERATOR OR OPERATOR STREQUAL "=")
        # Exact match
        if(ACT_MAJOR EQUAL REQ_MAJOR AND ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH EQUAL REQ_PATCH)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL ">=")
        # Greater than or equal
        nexus_compare_versions(${VERSION} ${REQ_MAJOR}.${REQ_MINOR}.${REQ_PATCH} CMP)
        if(CMP GREATER_EQUAL 0)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL "<=")
        # Less than or equal
        nexus_compare_versions(${VERSION} ${REQ_MAJOR}.${REQ_MINOR}.${REQ_PATCH} CMP)
        if(CMP LESS_EQUAL 0)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL ">")
        # Greater than
        nexus_compare_versions(${VERSION} ${REQ_MAJOR}.${REQ_MINOR}.${REQ_PATCH} CMP)
        if(CMP GREATER 0)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL "<")
        # Less than
        nexus_compare_versions(${VERSION} ${REQ_MAJOR}.${REQ_MINOR}.${REQ_PATCH} CMP)
        if(CMP LESS 0)
            set(SATISFIES TRUE)
        endif()
    elseif(OPERATOR STREQUAL "^")
        # Caret: Compatible with (same major version, >= minor.patch)
        if(ACT_MAJOR EQUAL REQ_MAJOR)
            if(ACT_MINOR GREATER REQ_MINOR)
                set(SATISFIES TRUE)
            elseif(ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH GREATER_EQUAL REQ_PATCH)
                set(SATISFIES TRUE)
            endif()
        endif()
    elseif(OPERATOR STREQUAL "~")
        # Tilde: Approximately equivalent (same major.minor, >= patch)
        if(ACT_MAJOR EQUAL REQ_MAJOR AND ACT_MINOR EQUAL REQ_MINOR AND ACT_PATCH GREATER_EQUAL REQ_PATCH)
            set(SATISFIES TRUE)
        endif()
    endif()

    set(${SATISFIES_VAR} ${SATISFIES} PARENT_SCOPE)
endfunction()

#
# Resolve version conflicts and select best version
# CONSTRAINTS: List of version constraints
# AVAILABLE: List of available versions
# SELECTED: Selected version (empty if no match)
#
function(nexus_resolve_version CONSTRAINTS AVAILABLE SELECTED_VAR)
    set(CANDIDATES "")

    # Find versions that satisfy all constraints
    foreach(VERSION ${AVAILABLE})
        set(SATISFIES_ALL TRUE)

        foreach(CONSTRAINT ${CONSTRAINTS})
            nexus_version_satisfies(${VERSION} ${CONSTRAINT} SATISFIES)
            if(NOT SATISFIES)
                set(SATISFIES_ALL FALSE)
                break()
            endif()
        endforeach()

        if(SATISFIES_ALL)
            list(APPEND CANDIDATES ${VERSION})
        endif()
    endforeach()

    # Select the highest version from candidates
    if(CANDIDATES)
        set(BEST_VERSION "")
        foreach(VERSION ${CANDIDATES})
            if(NOT BEST_VERSION)
                set(BEST_VERSION ${VERSION})
            else()
                nexus_compare_versions(${VERSION} ${BEST_VERSION} CMP)
                if(CMP GREATER 0)
                    set(BEST_VERSION ${VERSION})
                endif()
            endif()
        endforeach()

        set(${SELECTED_VAR} ${BEST_VERSION} PARENT_SCOPE)
    else()
        # No version satisfies all constraints
        message(WARNING "No version satisfies all constraints: ${CONSTRAINTS}")
        set(${SELECTED_VAR} "" PARENT_SCOPE)
    endif()
endfunction()

#
# Resolve a dependency
# NAME: Dependency name
# VERSION: Version specification
#
function(nexus_resolve_dependency NAME VERSION)
    # Check if dependency is already resolved
    if(TARGET ${NAME})
        message(STATUS "Dependency already resolved: ${NAME}")
        return()
    endif()

    # Check lock file first
    if(EXISTS ${NEXUS_LOCK_FILE})
        nexus_load_lockfile()
        # Check if dependency is in lock file
        nexus_get_locked_version(${NAME} LOCKED_VERSION)
        if(LOCKED_VERSION)
            # Verify locked version satisfies constraint
            nexus_version_satisfies(${LOCKED_VERSION} ${VERSION} SATISFIES)
            if(SATISFIES)
                message(STATUS "Using locked version: ${NAME} ${LOCKED_VERSION}")
                set(VERSION ${LOCKED_VERSION})
            else()
                message(WARNING "Locked version ${LOCKED_VERSION} does not satisfy constraint ${VERSION}")
            endif()
        endif()
    endif()

    # Try to find the dependency using find_package
    find_package(${NAME} ${VERSION} QUIET)

    if(${NAME}_FOUND)
        message(STATUS "Found dependency: ${NAME} ${${NAME}_VERSION}")

        # Store resolved version
        list(APPEND NEXUS_DEPENDENCY_VERSIONS "${NAME}=${${NAME}_VERSION}")
        set(NEXUS_DEPENDENCY_VERSIONS ${NEXUS_DEPENDENCY_VERSIONS} CACHE INTERNAL "Dependency versions")
    else()
        message(WARNING "Dependency not found: ${NAME} (${VERSION})")
        message(STATUS "  Please install it or add it to ${NEXUS_LOCK_FILE}")
    endif()
endfunction()

##############################################################################
# Vendor Library Integration Functions
##############################################################################

#
# Add a vendor library
# NAME: Library name
# SOURCE_DIR: Vendor source directory
# INTERFACE_ONLY: Create interface library (optional)
# COMPILE_DEFINITIONS: Compile definitions (optional)
#
function(nexus_add_vendor_library)
    cmake_parse_arguments(
        ARG
        "INTERFACE_ONLY"
        "NAME;SOURCE_DIR"
        "COMPILE_DEFINITIONS"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "nexus_add_vendor_library: NAME argument is required")
    endif()

    if(NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "nexus_add_vendor_library: SOURCE_DIR argument is required")
    endif()

    if(NOT EXISTS ${ARG_SOURCE_DIR})
        message(FATAL_ERROR "Vendor directory not found: ${ARG_SOURCE_DIR}")
    endif()

    # Create interface library
    if(ARG_INTERFACE_ONLY)
        add_library(${ARG_NAME} INTERFACE)

        target_include_directories(${ARG_NAME}
            INTERFACE
                ${ARG_SOURCE_DIR}/include
        )

        if(ARG_COMPILE_DEFINITIONS)
            target_compile_definitions(${ARG_NAME}
                INTERFACE
                    ${ARG_COMPILE_DEFINITIONS}
            )
        endif()
    else()
        # Collect source files
        file(GLOB_RECURSE VENDOR_SOURCES
            ${ARG_SOURCE_DIR}/*.c
            ${ARG_SOURCE_DIR}/*.cpp
        )

        if(NOT VENDOR_SOURCES)
            message(FATAL_ERROR "No source files found in: ${ARG_SOURCE_DIR}")
        endif()

        # Create library
        add_library(${ARG_NAME} STATIC ${VENDOR_SOURCES})

        target_include_directories(${ARG_NAME}
            PUBLIC
                ${ARG_SOURCE_DIR}/include
        )

        if(ARG_COMPILE_DEFINITIONS)
            target_compile_definitions(${ARG_NAME}
                PUBLIC
                    ${ARG_COMPILE_DEFINITIONS}
            )
        endif()
    endif()

    message(STATUS "Added vendor library: ${ARG_NAME}")
endfunction()

##############################################################################
# Lock File Management Functions
##############################################################################

# Lock file data storage
set(NEXUS_LOCKFILE_DATA "" CACHE INTERNAL "Parsed lock file data")

#
# Generate dependency lock file
# OUTPUT: Output file path (optional)
#
function(nexus_generate_lockfile)
    cmake_parse_arguments(
        ARG
        ""
        "OUTPUT"
        ""
        ${ARGN}
    )

    # Set output file
    if(ARG_OUTPUT)
        set(LOCK_FILE ${ARG_OUTPUT})
    else()
        set(LOCK_FILE ${NEXUS_LOCK_FILE})
    endif()

    message(STATUS "Generating lock file: ${LOCK_FILE}")

    # Get current timestamp
    string(TIMESTAMP CURRENT_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

    # Create JSON structure
    set(LOCK_CONTENT "{\n")
    string(APPEND LOCK_CONTENT "  \"version\": \"1.0\",\n")
    string(APPEND LOCK_CONTENT "  \"generated\": \"${CURRENT_TIME}\",\n")
    string(APPEND LOCK_CONTENT "  \"dependencies\": {\n")

    # Add dependencies
    list(LENGTH NEXUS_DEPENDENCY_VERSIONS DEP_COUNT)
    set(DEP_INDEX 0)

    foreach(DEP_VERSION ${NEXUS_DEPENDENCY_VERSIONS})
        string(REPLACE "=" ";" DEP_PARTS ${DEP_VERSION})
        list(GET DEP_PARTS 0 DEP_NAME)
        list(GET DEP_PARTS 1 DEP_VER)

        # Get additional info from NEXUS_DEPENDENCY_INFO
        set(DEP_REQUIRED "false")
        set(DEP_COMPONENTS "")

        foreach(INFO ${NEXUS_DEPENDENCY_INFO})
            if(INFO MATCHES "name=${DEP_NAME}")
                if(INFO MATCHES "required=TRUE")
                    set(DEP_REQUIRED "true")
                endif()
                if(INFO MATCHES "components=([^;]+)")
                    set(DEP_COMPONENTS ${CMAKE_MATCH_1})
                endif()
                break()
            endif()
        endforeach()

        # Calculate hash (placeholder - would use actual file hash in production)
        string(SHA256 DEP_HASH "${DEP_NAME}${DEP_VER}")

        string(APPEND LOCK_CONTENT "    \"${DEP_NAME}\": {\n")
        string(APPEND LOCK_CONTENT "      \"version\": \"${DEP_VER}\",\n")
        string(APPEND LOCK_CONTENT "      \"required\": ${DEP_REQUIRED},\n")

        if(DEP_COMPONENTS)
            string(APPEND LOCK_CONTENT "      \"components\": \"${DEP_COMPONENTS}\",\n")
        endif()

        string(APPEND LOCK_CONTENT "      \"hash\": \"sha256:${DEP_HASH}\"\n")

        math(EXPR DEP_INDEX "${DEP_INDEX} + 1")
        if(DEP_INDEX LESS DEP_COUNT)
            string(APPEND LOCK_CONTENT "    },\n")
        else()
            string(APPEND LOCK_CONTENT "    }\n")
        endif()
    endforeach()

    string(APPEND LOCK_CONTENT "  }\n")
    string(APPEND LOCK_CONTENT "}\n")

    # Write lock file
    file(WRITE ${LOCK_FILE} ${LOCK_CONTENT})

    message(STATUS "Lock file generated: ${LOCK_FILE}")
endfunction()

#
# Load dependency lock file
#
function(nexus_load_lockfile)
    if(NOT EXISTS ${NEXUS_LOCK_FILE})
        message(STATUS "Lock file not found: ${NEXUS_LOCK_FILE}")
        return()
    endif()

    message(STATUS "Loading lock file: ${NEXUS_LOCK_FILE}")

    # Read lock file
    file(READ ${NEXUS_LOCK_FILE} LOCK_CONTENT)

    # Parse JSON (simplified parsing)
    # Extract dependencies section
    if(LOCK_CONTENT MATCHES "\"dependencies\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}")
        set(DEPS_SECTION ${CMAKE_MATCH_1})

        # Parse each dependency
        string(REGEX MATCHALL "\"([^\"]+)\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}" DEP_MATCHES ${DEPS_SECTION})

        set(LOCKFILE_DATA "")
        foreach(DEP_MATCH ${DEP_MATCHES})
            if(DEP_MATCH MATCHES "\"([^\"]+)\"[[:space:]]*:[[:space:]]*\\{([^}]+)\\}")
                set(DEP_NAME ${CMAKE_MATCH_1})
                set(DEP_INFO ${CMAKE_MATCH_2})

                # Extract version
                if(DEP_INFO MATCHES "\"version\"[[:space:]]*:[[:space:]]*\"([^\"]+)\"")
                    set(DEP_VERSION ${CMAKE_MATCH_1})
                endif()

                # Extract hash
                if(DEP_INFO MATCHES "\"hash\"[[:space:]]*:[[:space:]]*\"([^\"]+)\"")
                    set(DEP_HASH ${CMAKE_MATCH_1})
                endif()

                # Store in lockfile data
                list(APPEND LOCKFILE_DATA "${DEP_NAME}:${DEP_VERSION}:${DEP_HASH}")
            endif()
        endforeach()

        set(NEXUS_LOCKFILE_DATA ${LOCKFILE_DATA} CACHE INTERNAL "Parsed lock file data")
    endif()

    message(STATUS "Lock file loaded")
endfunction()

#
# Get locked version for a dependency
# NAME: Dependency name
# VERSION: Locked version (empty if not found)
#
function(nexus_get_locked_version NAME VERSION_VAR)
    set(LOCKED_VERSION "")

    foreach(ENTRY ${NEXUS_LOCKFILE_DATA})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 ENTRY_NAME)

        if(ENTRY_NAME STREQUAL NAME)
            list(GET PARTS 1 LOCKED_VERSION)
            break()
        endif()
    endforeach()

    set(${VERSION_VAR} ${LOCKED_VERSION} PARENT_SCOPE)
endfunction()

#
# Verify lock file integrity
# VALID: TRUE if lock file is valid
#
function(nexus_verify_lockfile VALID_VAR)
    if(NOT EXISTS ${NEXUS_LOCK_FILE})
        set(${VALID_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Load lock file if not already loaded
    if(NOT NEXUS_LOCKFILE_DATA)
        nexus_load_lockfile()
    endif()

    set(IS_VALID TRUE)

    # Verify each dependency hash
    foreach(ENTRY ${NEXUS_LOCKFILE_DATA})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 DEP_NAME)
        list(GET PARTS 1 DEP_VERSION)
        list(GET PARTS 2 DEP_HASH)

        # Calculate expected hash
        string(SHA256 EXPECTED_HASH "${DEP_NAME}${DEP_VERSION}")
        string(PREPEND EXPECTED_HASH "sha256:")

        if(NOT DEP_HASH STREQUAL EXPECTED_HASH)
            message(WARNING "Lock file integrity check failed for ${DEP_NAME}")
            message(WARNING "  Expected: ${EXPECTED_HASH}")
            message(WARNING "  Got: ${DEP_HASH}")
            set(IS_VALID FALSE)
        endif()
    endforeach()

    set(${VALID_VAR} ${IS_VALID} PARENT_SCOPE)
endfunction()

##############################################################################
# Dependency Graph Analysis Functions
##############################################################################

#
# Check for circular dependencies
# OUTPUT_VAR: Variable to store result (TRUE if cycle found)
#
function(nexus_check_circular_dependencies OUTPUT_VAR)
    set(HAS_CYCLE FALSE)

    # Get all modules from dependency graph
    set(ALL_MODULES "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)
        list(APPEND ALL_MODULES ${MODULE})
    endforeach()

    # Check each module for cycles
    foreach(MODULE ${ALL_MODULES})
        set(VISITED "")
        set(REC_STACK "")
        nexus_has_cycle_dfs(${MODULE} VISITED REC_STACK CYCLE_FOUND)

        if(CYCLE_FOUND)
            set(HAS_CYCLE TRUE)
            message(WARNING "Circular dependency detected starting from: ${MODULE}")
            break()
        endif()
    endforeach()

    set(${OUTPUT_VAR} ${HAS_CYCLE} PARENT_SCOPE)
endfunction()

#
# DFS helper for cycle detection
# NODE: Current node
# VISITED: Visited nodes
# REC_STACK: Recursion stack
# CYCLE_FOUND: TRUE if cycle found
#
function(nexus_has_cycle_dfs NODE VISITED_VAR REC_STACK_VAR CYCLE_FOUND_VAR)
    # Get current visited and rec_stack from parent scope
    set(VISITED ${${VISITED_VAR}})
    set(REC_STACK ${${REC_STACK_VAR}})

    # Check if node is in recursion stack (cycle detected)
    list(FIND REC_STACK ${NODE} IN_REC_STACK)
    if(NOT IN_REC_STACK EQUAL -1)
        set(${CYCLE_FOUND_VAR} TRUE PARENT_SCOPE)
        return()
    endif()

    # Check if already visited
    list(FIND VISITED ${NODE} IN_VISITED)
    if(NOT IN_VISITED EQUAL -1)
        set(${CYCLE_FOUND_VAR} FALSE PARENT_SCOPE)
        return()
    endif()

    # Mark as visited and add to recursion stack
    list(APPEND VISITED ${NODE})
    list(APPEND REC_STACK ${NODE})

    # Get dependencies of current node
    set(NODE_DEPS "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)

        if(MODULE STREQUAL NODE)
            list(LENGTH PARTS PARTS_LEN)
            if(PARTS_LEN GREATER 1)
                list(GET PARTS 1 DEPS_STR)
                if(DEPS_STR)
                    string(REPLACE ";" "," DEPS_STR_TEMP ${DEPS_STR})
                    string(REPLACE "," ";" NODE_DEPS ${DEPS_STR_TEMP})
                endif()
            endif()
            break()
        endif()
    endforeach()

    # Visit all dependencies
    foreach(DEP ${NODE_DEPS})
        nexus_has_cycle_dfs(${DEP} VISITED REC_STACK CYCLE_FOUND)
        if(CYCLE_FOUND)
            set(${CYCLE_FOUND_VAR} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Remove from recursion stack
    list(REMOVE_ITEM REC_STACK ${NODE})

    # Update parent scope
    set(${VISITED_VAR} ${VISITED} PARENT_SCOPE)
    set(${REC_STACK_VAR} ${REC_STACK} PARENT_SCOPE)
    set(${CYCLE_FOUND_VAR} FALSE PARENT_SCOPE)
endfunction()

#
# Get dependency graph in topological order
# OUTPUT_VAR: Variable to store ordered dependencies
#
function(nexus_get_dependency_order OUTPUT_VAR)
    set(ORDERED "")
    set(VISITED "")

    # Get all modules
    set(ALL_MODULES "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)
        list(APPEND ALL_MODULES ${MODULE})
    endforeach()

    # Perform topological sort using DFS
    foreach(MODULE ${ALL_MODULES})
        list(FIND VISITED ${MODULE} IN_VISITED)
        if(IN_VISITED EQUAL -1)
            nexus_topological_sort_dfs(${MODULE} VISITED ORDERED)
        endif()
    endforeach()

    # Reverse the order (DFS gives reverse topological order)
    list(REVERSE ORDERED)

    set(${OUTPUT_VAR} ${ORDERED} PARENT_SCOPE)
endfunction()

#
# DFS helper for topological sort
# NODE: Current node
# VISITED: Visited nodes
# ORDERED: Ordered list
#
function(nexus_topological_sort_dfs NODE VISITED_VAR ORDERED_VAR)
    # Get current visited and ordered from parent scope
    set(VISITED ${${VISITED_VAR}})
    set(ORDERED ${${ORDERED_VAR}})

    # Mark as visited
    list(APPEND VISITED ${NODE})

    # Get dependencies of current node
    set(NODE_DEPS "")
    foreach(ENTRY ${NEXUS_DEPENDENCY_GRAPH})
        string(REPLACE ":" ";" PARTS ${ENTRY})
        list(GET PARTS 0 MODULE)

        if(MODULE STREQUAL NODE)
            list(LENGTH PARTS PARTS_LEN)
            if(PARTS_LEN GREATER 1)
                list(GET PARTS 1 DEPS_STR)
                if(DEPS_STR)
                    string(REPLACE ";" "," DEPS_STR_TEMP ${DEPS_STR})
                    string(REPLACE "," ";" NODE_DEPS ${DEPS_STR_TEMP})
                endif()
            endif()
            break()
        endif()
    endforeach()

    # Visit all dependencies
    foreach(DEP ${NODE_DEPS})
        list(FIND VISITED ${DEP} IN_VISITED)
        if(IN_VISITED EQUAL -1)
            nexus_topological_sort_dfs(${DEP} VISITED ORDERED)
        endif()
    endforeach()

    # Add current node to ordered list
    list(APPEND ORDERED ${NODE})

    # Update parent scope
    set(${VISITED_VAR} ${VISITED} PARENT_SCOPE)
    set(${ORDERED_VAR} ${ORDERED} PARENT_SCOPE)
endfunction()

##############################################################################
# End of NexusDependency.cmake
##############################################################################
