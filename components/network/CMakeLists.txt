# Explicit optional target over the organization's immutable lwIP gitlink.
# No dependency download or source-tree generation occurs during configure.
include_guard(GLOBAL)

# Admission occurs only when the component is explicitly selected.
function(nexus_add_lwip)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_lwip accepts no profile overrides")
    endif()
    if(TARGET Nexus::LwIP)
        return()
    endif()
    if(NOT TARGET Nexus::Config)
        message(FATAL_ERROR "lwIP requires a configured Nexus assembly")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    get_target_property(NEXUS_SOURCE_DIR Nexus::Config NEXUS_SOURCE_DIR)
    if(NOT NEXUS_SOURCE_DIR OR NEXUS_SOURCE_DIR MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "lwIP requires the assembly source identity")
    endif()
    if(DEFINED NEXUS_LWIP_SOURCE AND NOT NEXUS_LWIP_SOURCE STREQUAL "")
        get_filename_component(nexus_lwip_source "${NEXUS_LWIP_SOURCE}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        set(nexus_lwip_source "${NEXUS_SOURCE_DIR}/ext/lwip")
    endif()
    set(nexus_lwip_scope)
    if(NEXUS_LWIP_DEVELOPMENT)
        list(APPEND nexus_lwip_scope --development)
    else()
        get_property(nexus_lwip_sdk GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
        if(nexus_lwip_sdk AND nexus_lwip_sdk STREQUAL NEXUS_SOURCE_DIR)
            list(APPEND nexus_lwip_scope --source-sdk)
        endif()
    endif()
    # Verify the selected spelling before resolving it; REALPATH would erase an
    # inadmissible symbolic selector before the verifier can inspect it.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/maintenance/lwip_lock.py"
            --source "${nexus_lwip_source}"
            --lock "${NEXUS_SOURCE_DIR}/dependencies/lwip.lock.json"
            ${nexus_lwip_scope}
        RESULT_VARIABLE nexus_lwip_verified
        OUTPUT_VARIABLE nexus_lwip_output
        ERROR_VARIABLE nexus_lwip_error)
    if(NOT nexus_lwip_verified EQUAL 0)
        message(FATAL_ERROR "lwIP source rejected: ${nexus_lwip_output}"
            "${nexus_lwip_error}")
    endif()
    get_filename_component(nexus_lwip_source "${nexus_lwip_source}" REALPATH)
    string(JSON nexus_lwip_input_count LENGTH "${nexus_lwip_output}"
        configure_inputs)
    math(EXPR nexus_lwip_input_last "${nexus_lwip_input_count} - 1")
    foreach(nexus_lwip_input_index RANGE 0 ${nexus_lwip_input_last})
        string(JSON nexus_lwip_input GET "${nexus_lwip_output}"
            configure_inputs ${nexus_lwip_input_index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${nexus_lwip_input}")
    endforeach()

    # Source admission and profile target definitions have one authority each.
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Profiles.cmake")
    nexus_lwip_profile("" LwIP LwIPTCP FALSE)
    nexus_lwip_profile("_loaned_rx" LwIPLoanedRX LwIPTCPLoanedRX TRUE)
endfunction()
