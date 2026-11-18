# The sole production graph runs in a private directory scope.
# A consumer selects one assembly; there is no Kconfig/cache fallback truth.
if(NEXUS_SOURCE_DIR STREQUAL NEXUS_BINARY_DIR)
    message(FATAL_ERROR "Nexus requires an out-of-source build")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(NEXUS_TEST_SUPPORT "Enable explicit host-only test assemblies" OFF)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NEXUS_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NEXUS_BINARY_DIR}/bin")
if(NOT NEXUS_ASSEMBLY_FILE)
    message(
        FATAL_ERROR
        "Select a maintained assembly with a preset or "
        "NEXUS_ASSEMBLY_FILE; host models require NEXUS_TEST_SUPPORT"
    )
endif()
foreach(
    legacy
    NEXUS_CONFIG_FILE
    NEXUS_PLATFORM
    NEXUS_OSAL_BACKEND
    NEXUS_BOARD_DIR
    NEXUS_FLASH_LAYOUT_FILE
)
    if(DEFINED ${legacy} AND NOT "${${legacy}}" STREQUAL "")
        message(FATAL_ERROR "${legacy} is removed; use NEXUS_ASSEMBLY_FILE")
    endif()
endforeach()
include("${NEXUS_SOURCE_DIR}/cmake/platform/Configure.cmake")
nexus_resolve_assembly()
include("${NEXUS_SOURCE_DIR}/cmake/platform/Options.cmake")
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
nexus_cmsis_core_include(NEXUS_CMSIS_CORE_INCLUDE_DIR)
include("${NEXUS_SOURCE_DIR}/cmake/platform/Firmware.cmake")

#----------------------------------------------------------------------------
# Platform targets
#----------------------------------------------------------------------------
_nexus_add_common_targets()
include("${NEXUS_SOURCE_DIR}/cmake/platform/Components.cmake")
nexus_resolve_components(NEXUS_COMPONENT_TARGETS ${NEXUS_SELECTED_COMPONENTS})
if(NEXUS_SOC_FAMILY STREQUAL "native" AND NOT NEXUS_TEST_SUPPORT)
    message(FATAL_ERROR "Native models require NEXUS_TEST_SUPPORT")
endif()
if(
    NOT NEXUS_SOC_CMAKE_ENTRY
    OR NOT NEXUS_SOC_TARGET
    OR NOT IS_ABSOLUTE "${NEXUS_SOC_CMAKE_ENTRY}"
    OR NOT EXISTS "${NEXUS_SOC_CMAKE_ENTRY}"
)
    message(
        FATAL_ERROR
        "Selected package requires a resolved CMake entry and target"
    )
endif()
get_filename_component(
    nexus_provider_directory
    "${NEXUS_SOC_CMAKE_ENTRY}"
    DIRECTORY
)
get_filename_component(nexus_provider_entry "${NEXUS_SOC_CMAKE_ENTRY}" NAME)
if(NOT nexus_provider_entry STREQUAL "CMakeLists.txt")
    message(FATAL_ERROR "Selected package CMake entry must be CMakeLists.txt")
endif()
if(NEXUS_SOC_FAMILY STREQUAL "native")
    set(nexus_provider_binary "${NEXUS_BINARY_DIR}/tests/support/native")
else()
    set(nexus_provider_binary "${NEXUS_BINARY_DIR}/soc/${NEXUS_SOC_FAMILY}")
endif()
add_subdirectory("${nexus_provider_directory}" "${nexus_provider_binary}")
set(nexus_provider "${NEXUS_SOC_TARGET}")
include("${NEXUS_SOURCE_DIR}/cmake/platform/Provider.cmake")
nexus_validate_selected_provider("${nexus_provider}")
add_library(nexus_platform STATIC "${NEXUS_CONFIG_DIR}/bindings.c")
target_include_directories(
    nexus_platform
    PUBLIC "${NEXUS_CONFIG_DIR}"
    PRIVATE "${NEXUS_SOURCE_DIR}/io/private" "${NEXUS_SOURCE_DIR}/core/private"
)
target_link_libraries(
    nexus_platform
    PUBLIC ${nexus_provider}
    PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch
)
target_link_libraries(nexus_platform PUBLIC nexus_component_api)
target_link_libraries(nexus_platform PRIVATE nexus_component_bindings)
if(NEXUS_IO_CYCLE_PROBES)
    target_link_libraries(nexus_platform PRIVATE Nexus::IOCycleProbe)
    target_compile_definitions(nexus_platform PUBLIC NX_IO_CYCLE_PROBES=1)
endif()
get_target_property(
    startup_usage
    "${nexus_provider}"
    NEXUS_STARTUP_USAGE_TARGET
)
if(startup_usage)
    target_link_libraries(nexus_platform PRIVATE "${startup_usage}")
endif()
set(NEXUS_PROVIDER_TARGET "${nexus_provider}")
nexus_apply_target_policy(nexus_platform)
_nexus_firmware_properties(required optional)
foreach(property IN LISTS required optional)
    set_property(TARGET nexus_platform PROPERTY ${property} "${${property}}")
endforeach()
add_library(Nexus::Platform ALIAS nexus_platform)

message(
    STATUS
    "Nexus: ${NEXUS_EXACT_PART}/${NEXUS_BACKEND}, ${NEXUS_OPTIMIZATION}"
)
message(STATUS "Sole resolved configuration: ${NEXUS_CONFIG_DIR}/resolved.json")
