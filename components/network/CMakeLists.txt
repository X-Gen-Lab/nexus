# Explicit optional target over the organization's immutable lwIP gitlink.
# No dependency download or source-tree generation occurs during configure.
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

# Do not include upstream Filelists.cmake: it configures files in the source
# checkout and creates unrelated targets. CMake remains the source graph.
set(nexus_lwip_sources
    core/init.c core/def.c core/inet_chksum.c core/ip.c core/mem.c
    core/memp.c core/netif.c core/pbuf.c core/raw.c core/stats.c core/sys.c
    core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c core/udp.c
    core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/ip4_frag.c core/ipv4/ip4.c
    core/ipv4/ip4_addr.c netif/ethernet.c)
list(TRANSFORM nexus_lwip_sources PREPEND "${nexus_lwip_source}/src/")
add_library(nexus_lwip_vendor STATIC EXCLUDE_FROM_ALL ${nexus_lwip_sources})
target_include_directories(nexus_lwip_vendor SYSTEM PUBLIC
    "${nexus_lwip_source}/src/include")
target_include_directories(nexus_lwip_vendor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/port")
target_link_libraries(nexus_lwip_vendor PUBLIC nexus_abi_options
    PRIVATE nexus_build_options)

add_library(nexus_lwip STATIC EXCLUDE_FROM_ALL src/lwip_adapter.c)
target_include_directories(nexus_lwip PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_lwip PUBLIC Nexus::IO
    PRIVATE nexus_lwip_vendor nexus_build_options nexus_owned_warnings)
add_library(Nexus::LwIP ALIAS nexus_lwip)

# Only static composition opts into the exact supplier netif layout/profile.
add_library(nexus_lwip_storage INTERFACE)
target_link_libraries(nexus_lwip_storage INTERFACE
    Nexus::LwIP nexus_lwip_vendor)
add_library(Nexus::LwIPStorage ALIAS nexus_lwip_storage)

# Optional byte client over the same explicitly owned NO_SYS stack.
add_library(nexus_lwip_tcp STATIC EXCLUDE_FROM_ALL src/lwip_tcp.c)
nexus_apply_target_policy(nexus_lwip_tcp)
target_include_directories(nexus_lwip_tcp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_lwip_tcp PUBLIC Nexus::IO
    PRIVATE nexus_lwip nexus_lwip_vendor
        nexus_build_options nexus_owned_warnings)
add_library(Nexus::LwIPTCP ALIAS nexus_lwip_tcp)

# Only static composition opts into the exact PCB/pbuf storage layout.
add_library(nexus_lwip_tcp_storage INTERFACE)
target_link_libraries(nexus_lwip_tcp_storage INTERFACE
    Nexus::LwIPTCP nexus_lwip_vendor)
add_library(Nexus::LwIPTCPStorage ALIAS nexus_lwip_tcp_storage)
