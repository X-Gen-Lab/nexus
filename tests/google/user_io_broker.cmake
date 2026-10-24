# Actual broker and Native typed GPIO; only trusted kernel context is modeled.
add_library(nexus_user_io_broker_test_provider STATIC
    "${NEXUS_SOURCE_DIR}/components/user_io_broker/src/user_io_broker.c")
target_include_directories(nexus_user_io_broker_test_provider PUBLIC
    "${NEXUS_SOURCE_DIR}/components/user_io_broker/include"
    "${NEXUS_SOURCE_DIR}/components/user_io_broker/private")
target_link_libraries(nexus_user_io_broker_test_provider PUBLIC
    Nexus::Core Nexus::IO PRIVATE Nexus::Arch nexus_build_options
    nexus_owned_warnings)
target_compile_definitions(nexus_user_io_broker_test_provider PRIVATE
    nx_arch_is_privileged=nx_user_io_test_privileged
    nx_arch_exception_number=nx_user_io_test_exception)
nexus_google_test(nexus_user_io_broker_test SOURCES user_io_broker_test.cpp
    LIBRARIES nexus_user_io_broker_test_provider Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/soc/native/drivers")
