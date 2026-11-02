# Maintained Native contracts explicitly instantiate the reviewed TLS profile.
# The product client never includes the separate fixture server's configuration.
nexus_add_tls()
get_property(tls_test_source GLOBAL PROPERTY NEXUS_TLS_SOURCE)
include(ExternalProject)
ExternalProject_Add(nexus_tls_host_peer
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/tls_peer"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/tls-peer"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_ARGS
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_BUILD_TYPE=MinSizeRel"
        "-DNEXUS_MBEDTLS_SOURCE=${tls_test_source}"
        "-DNEXUS_TLS_PROFILE_ROOT=${NEXUS_SOURCE_DIR}/components/tls/profile"
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --parallel 1
    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
    TEST_COMMAND "")
nexus_google_test(nexus_tls_client_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/tls_client_test.cpp"
    LIBRARIES Nexus::TLSStorage)
target_compile_definitions(nexus_tls_client_test PRIVATE
    NX_TLS_FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/tls_peer/fixtures"
    NX_TLS_TEST_SERVER="${CMAKE_CURRENT_BINARY_DIR}/tls-peer/tls_host_peer")
add_dependencies(nexus_tls_client_test nexus_tls_host_peer)
set_tests_properties(nexus_tls_client_test PROPERTIES TIMEOUT 30)

# A separate process observes backend defaults before any arena initialization.
nexus_google_test(nexus_tls_defaults_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/tls_defaults_test.cpp"
    LIBRARIES Nexus::TLSStorage)
