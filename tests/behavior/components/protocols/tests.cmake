# Explicit protocols behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/components/protocols/common_canopen.cmake")

include("${nexus_google_root}/components/protocols/cia402.cmake")

include("${nexus_google_root}/components/protocols/at_framing.cmake")

include("${nexus_google_root}/components/protocols/mcuboot_swap.cmake")

include("${nexus_google_root}/components/protocols/common_protocols.cmake")

if(TARGET Nexus::LwIP)
    nexus_google_test(nexus_network_lwip_test
        SOURCES "${nexus_google_root}/components/protocols/network_lwip_test.cpp" LIBRARIES Nexus::LwIPStorage
    )
endif()

if(TARGET Nexus::LwIPIPv4Storage)
    set(f107_reference
        "${NEXUS_SOURCE_DIR}/tests/integration/assemblies/stm32f1_lwip_ipv4"
    )
    nexus_google_test(
        f107_lwip_ipv4_contracts
        SOURCES "${nexus_google_root}/components/protocols/f107_lwip_ipv4_test.cpp"
                "${f107_reference}/raw_frames.c"
        INCLUDES "${f107_reference}"
        LIBRARIES Nexus::LwIPIPv4Storage
    )
endif()

# Actual raw TCP peers own separate lwIP global states on the maintained host.
if(TARGET Nexus::LwIP AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_pair_test
        SOURCES "${nexus_google_root}/components/protocols/network_tcp_pair_test.cpp"
        LIBRARIES Nexus::LwIPStorage
    )
    set_tests_properties(nexus_network_tcp_pair_test PROPERTIES TIMEOUT 30)
endif()

if(TARGET Nexus::LwIPTCPStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_stream_test
        SOURCES "${nexus_google_root}/components/protocols/network_tcp_stream_test.cpp"
        LIBRARIES Nexus::LwIPTCPStorage
    )
    target_link_options(
        nexus_network_tcp_stream_test
        PRIVATE "LINKER:--wrap=tcp_output" "LINKER:--wrap=tcp_close"
    )
    set_tests_properties(nexus_network_tcp_stream_test PROPERTIES TIMEOUT 30)
endif()

# A selected RX payload-loan profile is a separate static stack domain.
if(TARGET Nexus::LwIPLoanedRXStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_lwip_loaned_test
        SOURCES "${nexus_google_root}/components/protocols/network_lwip_loaned_test.cpp" LIBRARIES Nexus::LwIPLoanedRXStorage
    )
endif()

if(TARGET Nexus::LwIPTCPLoanedRXStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_loaned_stream_test
        SOURCES "${nexus_google_root}/components/protocols/network_tcp_stream_test.cpp"
        LIBRARIES Nexus::LwIPTCPLoanedRXStorage
    )
    target_link_options(
        nexus_network_tcp_loaned_stream_test
        PRIVATE "LINKER:--wrap=tcp_output" "LINKER:--wrap=tcp_close"
    )
    set_tests_properties(
        nexus_network_tcp_loaned_stream_test
        PROPERTIES TIMEOUT 30
    )
endif()

include("${nexus_google_root}/components/protocols/tls_client.cmake")

nexus_add_coremqtt()

nexus_google_test(nexus_mqtt_client_test
    SOURCES "${nexus_google_root}/components/protocols/mqtt_client_test.cpp" LIBRARIES Nexus::CoreMQTTStorage
)

# Compile the opt-in broker client without running Docker in ordinary hooks.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(
        nexus_mqtt_tls_interop_client
        EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/tests/integration/protocols/mqtt_tls_interop/client.c"
    )
    nexus_apply_target_policy(nexus_mqtt_tls_interop_client)
    target_link_libraries(
        nexus_mqtt_tls_interop_client
        PRIVATE
            Nexus::TLSStorage
            Nexus::CoreMQTTStorage
            nexus_build_options
            nexus_owned_warnings
    )
    add_dependencies(nexus_tls_client_test nexus_mqtt_tls_interop_client)
endif()

# Compile the real client stack chain as a nested Google dependency. Broker
# execution stays explicit; ordinary hooks never start Docker or use a PHY.
if(
    CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND TARGET nexus_network_tcp_stream_test
    AND TARGET Nexus::TLSStorage
    AND TARGET Nexus::CoreMQTTStorage
)
    add_executable(
        nexus_lwip_mqtt_interop_client
        EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/tests/integration/protocols/lwip_mqtt_interop/client.c"
        "${NEXUS_SOURCE_DIR}/tests/integration/protocols/lwip_mqtt_interop/transport.cpp"
    )
    nexus_apply_target_policy(nexus_lwip_mqtt_interop_client)
    target_include_directories(
        nexus_lwip_mqtt_interop_client
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(
        nexus_lwip_mqtt_interop_client
        PRIVATE
            Nexus::LwIPTCPStorage
            Nexus::TLSStorage
            Nexus::CoreMQTTStorage
            GTest::gtest
            nexus_build_options
            nexus_owned_warnings
    )
    add_dependencies(
        nexus_network_tcp_stream_test
        nexus_lwip_mqtt_interop_client
    )
endif()

# The reviewed full boot model uses the supplier's ordinary Linux test port.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${nexus_google_root}/components/protocols/mcuboot.cmake")
endif()
