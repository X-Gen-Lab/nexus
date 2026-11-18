# Source SDK only: the consumer owns one assembly and its toolchain/ABI.
# Parent variables and cache entries cannot establish byte verification.
# Every explicit consumption boundary verifies current source bytes.
if(
    EXISTS "${NEXUS_SOURCE_DIR}/.nexus-source-sdk.json"
    OR IS_SYMLINK "${NEXUS_SOURCE_DIR}/.nexus-source-sdk.json"
)
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/cmake/package/package_source_sdk.py" --verify
            "${NEXUS_SOURCE_DIR}"
        RESULT_VARIABLE admission_result
        OUTPUT_VARIABLE admission
        ERROR_VARIABLE admission_error
    )
    if(NOT admission_result EQUAL 0)
        message(FATAL_ERROR "Source SDK identity failed: ${admission_error}")
    endif()
    string(JSON publishable GET "${admission}" publishable)
    string(JSON revision GET "${admission}" source_revision)
    if(NOT publishable AND NOT NEXUS_ALLOW_SOURCE_SDK_FIXTURE)
        message(
            FATAL_ERROR
            "This source SDK is a nonpublishable development fixture"
        )
    endif()
    if(
        NEXUS_EXPECTED_SOURCE_REVISION
        AND NOT NEXUS_EXPECTED_SOURCE_REVISION STREQUAL revision
    )
        message(
            FATAL_ERROR
            "Installed Nexus source revision differs from the consumer's expected revision"
        )
    endif()
    if(NEXUS_BUILD_TESTS OR NEXUS_BUILD_WORKLOAD)
        message(
            FATAL_ERROR
            "The installed source SDK excludes development tests/contracts and their dependencies; validate the complete pinned checkout separately"
        )
    endif()
    # Publish the verified source scope for direct and package consumers.
    get_property(sdk_existing_source GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
    if(
        sdk_existing_source
        AND NOT sdk_existing_source STREQUAL NEXUS_SOURCE_DIR
    )
        message(
            FATAL_ERROR
            "One build may contain only one resolved Nexus source SDK"
        )
    endif()
    set_property(GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE "${NEXUS_SOURCE_DIR}")
endif()

include_guard(GLOBAL)

# An installed source SDK is configured by its consumer. It is deliberately not
# a binary export of this build's objects, selected Board or generated config.
function(nexus_package_source_sdk)
    cmake_parse_arguments(
        SDK
        "DEVELOPMENT_FIXTURE"
        "OUTPUT_DIRECTORY"
        ""
        ${ARGN}
    )
    if(
        SDK_UNPARSED_ARGUMENTS
        OR SDK_KEYWORDS_MISSING_VALUES
        OR NOT SDK_OUTPUT_DIRECTORY
    )
        message(
            FATAL_ERROR
            "nexus_package_source_sdk requires OUTPUT_DIRECTORY and known arguments"
        )
    endif()
    if(NOT IS_ABSOLUTE "${SDK_OUTPUT_DIRECTORY}")
        message(FATAL_ERROR "The source SDK output directory must be absolute")
    endif()
    if(NOT TARGET Nexus::Config)
        message(FATAL_ERROR "Configure Nexus before preparing its source SDK")
    endif()
    get_target_property(_sdk_source Nexus::Config NEXUS_SOURCE_DIR)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    get_target_property(_sdk_backend Nexus::Config NEXUS_BACKEND)
    get_target_property(_sdk_components Nexus::Config NEXUS_SELECTED_COMPONENTS)
    get_target_property(_sdk_package Nexus::Config NEXUS_SOC_PACKAGE_ID)
    get_target_property(_sdk_test_support Nexus::Config NEXUS_TEST_SUPPORT)
    foreach(
        property
        _sdk_backend
        _sdk_components
        _sdk_package
        _sdk_test_support
    )
        if("${${property}}" MATCHES "-NOTFOUND$")
            message(
                FATAL_ERROR
                "Nexus::Config is missing resolved SDK profile: ${property}"
            )
        endif()
    endforeach()
    if(_sdk_backend STREQUAL "native")
        set(_sdk_backend baremetal)
    endif()
    set(_sdk_arguments
        --source
        "${_sdk_source}"
        --output
        "${SDK_OUTPUT_DIRECTORY}"
        --backend
        "${_sdk_backend}"
    )
    if(_sdk_package)
        list(APPEND _sdk_arguments --package "${_sdk_package}")
    endif()
    foreach(component IN LISTS _sdk_components)
        list(APPEND _sdk_arguments --component "${component}")
    endforeach()
    if(_sdk_test_support)
        list(APPEND _sdk_arguments --host-support)
    endif()
    if(SDK_DEVELOPMENT_FIXTURE)
        list(APPEND _sdk_arguments --development-fixture)
    endif()
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${_sdk_source}/cmake/package/package_source_sdk.py"
            ${_sdk_arguments}
        RESULT_VARIABLE _sdk_result
        OUTPUT_VARIABLE _sdk_output
        ERROR_VARIABLE _sdk_error
    )
    if(NOT _sdk_result EQUAL 0)
        message(
            FATAL_ERROR
            "Source SDK preparation rejected:\n${_sdk_output}${_sdk_error}"
        )
    endif()
    set(NEXUS_SDK_PREFIX "${SDK_OUTPUT_DIRECTORY}" PARENT_SCOPE)
    message(STATUS "${_sdk_output}")
endfunction()
