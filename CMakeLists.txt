#----------------------------------------------------------------------------
# Project and source identity
#----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.31)
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(
        FATAL_ERROR
        "Use find_package(Nexus) and nexus_add_platform for consumers"
    )
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cmake/platform/TargetPolicy.cmake")
nexus_require_build_generator()
if(TARGET Nexus::Config)
    message(
        FATAL_ERROR
        "One build may contain only one resolved Nexus assembly"
    )
endif()
set(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
# Validate source SDK bytes before loading its resolver or metadata modules.
include("${NEXUS_SOURCE_DIR}/cmake/platform/SDK.cmake")
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_LIST_DIR}/tools/configure/metadata.py" --version
    RESULT_VARIABLE _nexus_version_result
    OUTPUT_VARIABLE NEXUS_PLATFORM_VERSION
    ERROR_VARIABLE _nexus_version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _nexus_version_result EQUAL 0)
    message(
        FATAL_ERROR
        "Nexus version policy rejected: ${_nexus_version_error}"
    )
endif()
project(nexus VERSION "${NEXUS_PLATFORM_VERSION}" LANGUAGES C ASM)

#----------------------------------------------------------------------------
# Explicit platform and developer workloads
#----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(NEXUS_ASSEMBLY_FILE
    ""
    CACHE FILEPATH
    "Developer project's authored assembly"
)
include("${NEXUS_SOURCE_DIR}/cmake/Nexus.cmake")
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
# Only this development project reads selection data for its test/workload graph.
get_target_property(NEXUS_CONFIG_DIR Nexus::Config NEXUS_CONFIG_DIR)
set(NEXUS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include("${NEXUS_CONFIG_DIR}/selection.cmake")
option(
    NEXUS_BUILD_TESTS
    "Build maintained host behavior tests"
    ${PROJECT_IS_TOP_LEVEL}
)
option(
    NEXUS_BUILD_WORKLOAD
    "Build a minimal developer linkage workload"
    ${PROJECT_IS_TOP_LEVEL}
)
if(NEXUS_BUILD_TESTS AND NEXUS_SOC_FAMILY STREQUAL "native")
    enable_testing()
    add_subdirectory("${NEXUS_SOURCE_DIR}/tests" tests)
endif()
if(NEXUS_BUILD_WORKLOAD)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/Workload.cmake")
    nexus_add_workload()
endif()
if(NEXUS_IO_EVOLUTION_FIXTURE)
    include(
        "${NEXUS_SOURCE_DIR}/tests/integration/assemblies/io_evolution_assembly/fixture.cmake"
    )
endif()
message(
    STATUS
    "Nexus: ${NEXUS_EXACT_PART}/${NEXUS_BACKEND}, ${NEXUS_OPTIMIZATION}"
)
message(STATUS "Sole resolved configuration: ${NEXUS_CONFIG_DIR}/resolved.json")
