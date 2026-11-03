# Explicit fixed client/PSA profile, invoked by selected components or tests.
# Defining the helper does not configure a vendor or create runtime storage.
include_guard(GLOBAL)
function(nexus_add_tls)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_tls accepts no profile overrides")
    endif()
    if(TARGET Nexus::TLS)
        return()
    endif()
    if(NOT TARGET Nexus::Config OR NOT TARGET Nexus::IO)
        message(FATAL_ERROR "TLS requires a configured Nexus assembly")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    get_target_property(nexus_tls_root Nexus::Config NEXUS_SOURCE_DIR)
    if(NOT nexus_tls_root OR "${nexus_tls_root}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "TLS requires the assembly source identity")
    endif()
    set(nexus_tls_component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    if(TARGET mbedtls OR TARGET tfpsacrypto)
        message(FATAL_ERROR "Nexus TLS requires exclusive ownership of its PSA profile")
    endif()
    if(DEFINED NEXUS_MBEDTLS_SOURCE AND NOT NEXUS_MBEDTLS_SOURCE STREQUAL "")
        get_filename_component(nexus_tls_source "${NEXUS_MBEDTLS_SOURCE}" ABSOLUTE
            BASE_DIR "${nexus_tls_component}")
    else()
        set(nexus_tls_source "${nexus_tls_root}/ext/mbedtls")
    endif()
    set(nexus_tls_scope)
    if(NEXUS_MBEDTLS_DEVELOPMENT)
        list(APPEND nexus_tls_scope --development)
    else()
        get_property(nexus_tls_sdk GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
        if(nexus_tls_sdk AND nexus_tls_sdk STREQUAL nexus_tls_root)
            list(APPEND nexus_tls_scope --source-sdk)
        endif()
    endif()
    # Verify the original selector before canonicalization can erase a symlink.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -B
            "${nexus_tls_root}/tools/maintenance/mbedtls_lock.py"
            --source "${nexus_tls_source}"
            --lock "${nexus_tls_root}/dependencies/mbedtls.lock.json"
            ${nexus_tls_scope}
        RESULT_VARIABLE nexus_tls_verified
        OUTPUT_VARIABLE nexus_tls_output
        ERROR_VARIABLE nexus_tls_error)
    if(NOT nexus_tls_verified EQUAL 0)
        message(FATAL_ERROR "TLS source rejected: ${nexus_tls_output}${nexus_tls_error}")
    endif()
    get_filename_component(nexus_tls_source "${nexus_tls_source}" REALPATH)
    set_property(GLOBAL PROPERTY NEXUS_TLS_SOURCE "${nexus_tls_source}")
    string(JSON nexus_tls_input_count LENGTH "${nexus_tls_output}" configure_inputs)
    math(EXPR nexus_tls_input_last "${nexus_tls_input_count} - 1")
    foreach(index RANGE 0 ${nexus_tls_input_last})
        string(JSON input GET "${nexus_tls_output}" configure_inputs ${index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()

    # Normal scoped variables deliberately select one profile; upstream options
    # honor CMP0077. A second independently configured PSA domain is unsupported.
    # Honor selected IPO when upstream resets an older policy scope.
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
    set(ENABLE_PROGRAMS OFF)
    set(ENABLE_TESTING OFF)
    set(GEN_FILES OFF)
    set(USE_SHARED_MBEDTLS_LIBRARY OFF)
    set(USE_STATIC_MBEDTLS_LIBRARY ON)
    set(MBEDTLS_CONFIG_FILE "${nexus_tls_source}/configs/config-suite-b.h")
    set(TF_PSA_CRYPTO_CONFIG_FILE
        "${nexus_tls_source}/configs/crypto-config-suite-b.h")
    set(MBEDTLS_USER_CONFIG_FILE "${nexus_tls_component}/profile/tls-user.h")
    set(TF_PSA_CRYPTO_USER_CONFIG_FILE
        "${nexus_tls_component}/profile/crypto-user.h")
    add_subdirectory("${nexus_tls_source}"
        "${CMAKE_CURRENT_BINARY_DIR}/nexus-tls-vendor" EXCLUDE_FROM_ALL)
    foreach(target builtin everest extras mbedtls mbedx509 p256-m platform pqcp
                   tfpsacrypto utilities)
        if(NOT TARGET "${target}")
            message(FATAL_ERROR "Reviewed TLS target is absent: ${target}")
        endif()
        # Upstream explicitly defaults to C99. Set the actual target property and
        # inherit the selected MCU ABI on every supplier translation unit.
        set_target_properties("${target}" PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        nexus_apply_target_policy("${target}")
        target_link_libraries("${target}" PRIVATE nexus_build_options)
    endforeach()
    add_library(nexus_tls STATIC EXCLUDE_FROM_ALL
        "${nexus_tls_component}/src/tls.c")
    nexus_apply_target_policy(nexus_tls)
    target_include_directories(nexus_tls PUBLIC
        "${nexus_tls_component}/include"
        "${nexus_tls_component}/../network/include")
    target_link_libraries(nexus_tls PUBLIC Nexus::IO
        PRIVATE MbedTLS::mbedtls nexus_build_options nexus_owned_warnings)
    add_library(Nexus::TLS ALIAS nexus_tls)
    set_property(TARGET nexus_tls PROPERTY
        INTERFACE_NEXUS_CRYPTO_ABI mbedtls4-psa)
    set_property(TARGET nexus_tls APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING NEXUS_CRYPTO_ABI)

    # Only static composition opts into the exact supplier storage layout.
    # Ordinary users of tls.h inherit neither vendor headers nor profile macros.
    add_library(nexus_tls_storage INTERFACE)
    target_link_libraries(nexus_tls_storage INTERFACE
        Nexus::TLS MbedTLS::mbedtls)
    add_library(Nexus::TLSStorage ALIAS nexus_tls_storage)
endfunction()
