# Source SDK only: the consumer owns one assembly and its toolchain/ABI.
# Parent variables and cache entries cannot establish byte verification.
# Every consumption boundary rechecks the complete source snapshot.
if(EXISTS "${NEXUS_SOURCE_DIR}/.nexus-source-sdk.json")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/cmake/package/package_source_sdk.py"
        --verify "${NEXUS_SOURCE_DIR}" RESULT_VARIABLE sdk_result
        ERROR_VARIABLE sdk_error OUTPUT_QUIET)
    if(NOT sdk_result EQUAL 0)
        message(FATAL_ERROR "Source SDK identity failed: ${sdk_error}")
    endif()
    # Publish the verified source scope for direct and package consumers.
    get_property(sdk_existing_source GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
    if(sdk_existing_source AND NOT sdk_existing_source STREQUAL NEXUS_SOURCE_DIR)
        message(FATAL_ERROR "One build may contain only one resolved Nexus source SDK")
    endif()
    set_property(GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE "${NEXUS_SOURCE_DIR}")
endif()

include_guard(GLOBAL)

# An installed source SDK is configured by its consumer. It is deliberately not
# a binary export of this build's objects, selected Board or generated config.
function(nexus_package_source_sdk)
    cmake_parse_arguments(SDK "DEVELOPMENT_FIXTURE" "OUTPUT_DIRECTORY" "" ${ARGN})
    if(SDK_UNPARSED_ARGUMENTS OR NOT SDK_OUTPUT_DIRECTORY)
        message(FATAL_ERROR "nexus_package_source_sdk requires OUTPUT_DIRECTORY and known arguments")
    endif()
    if(NOT IS_ABSOLUTE "${SDK_OUTPUT_DIRECTORY}")
        message(FATAL_ERROR "The source SDK output directory must be absolute")
    endif()
    if(NOT TARGET Nexus::Config)
        message(FATAL_ERROR "Configure Nexus before preparing its source SDK")
    endif()
    get_target_property(_sdk_source Nexus::Config NEXUS_SOURCE_DIR)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_sdk_backend "${NEXUS_BACKEND}")
    if(_sdk_backend STREQUAL "native")
        set(_sdk_backend baremetal)
    endif()
    set(_sdk_arguments --source "${_sdk_source}" --output "${SDK_OUTPUT_DIRECTORY}"
        --backend "${_sdk_backend}")
    if(NEXUS_SOC_PACKAGE_ID)
        list(APPEND _sdk_arguments --package "${NEXUS_SOC_PACKAGE_ID}")
    endif()
    foreach(component IN LISTS NEXUS_SELECTED_COMPONENTS)
        list(APPEND _sdk_arguments --component "${component}")
    endforeach()
    if(NEXUS_TEST_SUPPORT)
        list(APPEND _sdk_arguments --host-support)
    endif()
    if(SDK_DEVELOPMENT_FIXTURE)
        list(APPEND _sdk_arguments --development-fixture)
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${_sdk_source}/cmake/package/package_source_sdk.py" ${_sdk_arguments}
        RESULT_VARIABLE _sdk_result OUTPUT_VARIABLE _sdk_output ERROR_VARIABLE _sdk_error)
    if(NOT _sdk_result EQUAL 0)
        message(FATAL_ERROR "Source SDK preparation rejected:\n${_sdk_output}${_sdk_error}")
    endif()
    set(NEXUS_SDK_PREFIX "${SDK_OUTPUT_DIRECTORY}" PARENT_SCOPE)
    message(STATUS "${_sdk_output}")
endfunction()
