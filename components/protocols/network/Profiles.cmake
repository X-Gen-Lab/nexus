# Do not include upstream Filelists.cmake: it configures files in the source
# checkout and creates unrelated targets. CMake remains the source graph.
set(nexus_lwip_sources
    core/init.c
    core/def.c
    core/inet_chksum.c
    core/ip.c
    core/mem.c
    core/memp.c
    core/netif.c
    core/pbuf.c
    core/raw.c
    core/stats.c
    core/sys.c
    core/tcp.c
    core/tcp_in.c
    core/tcp_out.c
    core/timeouts.c
    core/udp.c
    core/ipv4/etharp.c
    core/ipv4/icmp.c
    core/ipv4/ip4_frag.c
    core/ipv4/ip4.c
    core/ipv4/ip4_addr.c
    netif/ethernet.c
)
list(TRANSFORM nexus_lwip_sources PREPEND "${nexus_lwip_source}/src/")
# Both explicit profiles use this one reviewed vendor recipe. Only a selected
# target builds; incompatible interface properties reject two stack domains.
function(nexus_lwip_profile suffix adapter_alias tcp_alias loaned)
    set(network_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    set(vendor "nexus_lwip${suffix}_vendor")
    set(adapter "nexus_lwip${suffix}")
    set(storage "nexus_lwip${suffix}_storage")
    set(tcp "nexus_lwip${suffix}_tcp")
    set(tcp_storage "nexus_lwip${suffix}_tcp_storage")
    set(managed FALSE)
    if(ARGC EQUAL 5)
        set(managed "${ARGV4}")
    elseif(ARGC GREATER 5)
        message(FATAL_ERROR "Invalid private lwIP profile arguments")
    endif()
    set(profile_sources ${nexus_lwip_sources})
    if(NOT tcp_alias)
        list(
            FILTER profile_sources
            EXCLUDE
            REGEX "/core/(tcp[^/]*|udp|raw)[.]c$"
        )
    elseif(managed)
        list(
            APPEND profile_sources
            "${nexus_lwip_source}/src/core/dns.c"
            "${nexus_lwip_source}/src/core/ipv4/dhcp.c"
            "${nexus_lwip_source}/src/core/ipv4/acd.c"
        )
    endif()
    add_library("${vendor}" STATIC EXCLUDE_FROM_ALL ${profile_sources})
    nexus_apply_target_policy("${vendor}")
    target_include_directories(
        "${vendor}"
        SYSTEM
        PUBLIC "${nexus_lwip_source}/src/include"
    )
    target_include_directories("${vendor}" PUBLIC "${network_dir}/port")
    target_link_libraries(
        "${vendor}"
        PUBLIC nexus_abi_options
        PRIVATE nexus_build_options
    )
    if(NOT tcp_alias)
        target_compile_definitions("${vendor}" PUBLIC NEXUS_LWIP_IPV4_ONLY=1)
        set(profile copied_ipv4)
    elseif(loaned)
        target_compile_definitions(
            "${vendor}"
            PUBLIC LWIP_SUPPORT_CUSTOM_PBUF=1 NEXUS_LWIP_LOANED_RX=1
        )
        set(profile loaned_rx)
    else()
        if(managed)
            set(profile managed_copied_rx)
            target_compile_definitions("${vendor}" PUBLIC NEXUS_LWIP_MANAGED=1)
        else()
            set(profile copied_rx)
        endif()
    endif()
    set_property(
        TARGET "${vendor}"
        PROPERTY INTERFACE_NEXUS_LWIP_RX_PROFILE "${profile}"
    )
    set_property(
        TARGET "${vendor}"
        APPEND
        PROPERTY COMPATIBLE_INTERFACE_STRING NEXUS_LWIP_RX_PROFILE
    )

    add_library(
        "${adapter}"
        STATIC
        EXCLUDE_FROM_ALL
        "${network_dir}/src/lwip_adapter.c"
    )
    nexus_apply_target_policy("${adapter}")
    target_include_directories("${adapter}" PUBLIC "${network_dir}/include")
    target_link_libraries(
        "${adapter}"
        PUBLIC Nexus::IOEthernetFrames
        PRIVATE "${vendor}" nexus_build_options nexus_owned_warnings
    )
    add_library("Nexus::${adapter_alias}" ALIAS "${adapter}")

    # Only static composition imports the exact supplier/profile layout.
    add_library("${storage}" INTERFACE)
    target_link_libraries("${storage}" INTERFACE "${adapter}" "${vendor}")
    add_library("Nexus::${adapter_alias}Storage" ALIAS "${storage}")

    set(profile_targets "${adapter}")
    if(tcp_alias)
        # TCP uses the same source and the selected adapter's exact layout.
        add_library(
            "${tcp}"
            STATIC
            EXCLUDE_FROM_ALL
            "${network_dir}/src/lwip_tcp.c"
        )
        nexus_apply_target_policy("${tcp}")
        target_include_directories("${tcp}" PUBLIC "${network_dir}/include")
        target_link_libraries(
            "${tcp}"
            PUBLIC nexus_io_contract
            PRIVATE
                "${adapter}"
                "${vendor}"
                nexus_build_options
                nexus_owned_warnings
        )
        add_library("Nexus::${tcp_alias}" ALIAS "${tcp}")
        add_library("${tcp_storage}" INTERFACE)
        target_link_libraries("${tcp_storage}" INTERFACE "${tcp}" "${vendor}")
        add_library("Nexus::${tcp_alias}Storage" ALIAS "${tcp_storage}")
        list(APPEND profile_targets "${tcp}")
    endif()
    # PRIVATE vendor links do not propagate a consumer's compatibility label.
    # Carry the selected ABI fact through ordinary adapter/TCP public targets.
    foreach(profile_target IN LISTS profile_targets)
        set_property(
            TARGET "${profile_target}"
            PROPERTY INTERFACE_NEXUS_LWIP_RX_PROFILE "${profile}"
        )
        set_property(
            TARGET "${profile_target}"
            APPEND
            PROPERTY COMPATIBLE_INTERFACE_STRING NEXUS_LWIP_RX_PROFILE
        )
    endforeach()
    if(loaned)
        target_compile_definitions("${adapter}" PRIVATE NEXUS_LWIP_LOANED_RX=1)
        target_compile_definitions("${tcp}" PRIVATE NEXUS_LWIP_LOANED_RX=1)
        target_compile_definitions(
            "${storage}"
            INTERFACE NEXUS_LWIP_LOANED_RX=1
        )
        target_compile_definitions(
            "${tcp_storage}"
            INTERFACE NEXUS_LWIP_LOANED_RX=1
        )
    endif()
endfunction()

# Services bind to this one managed stack, with no additional executor.
function(nexus_lwip_managed_services)
    add_library(
        nexus_lwip_managed_services
        STATIC
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/lwip_services.c"
    )
    target_include_directories(
        nexus_lwip_managed_services
        PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include"
    )
    target_link_libraries(
        nexus_lwip_managed_services
        PUBLIC Nexus::IOEthernetFrames
        PRIVATE
            Nexus::LwIPManagedStorage
            nexus_build_options
            nexus_owned_warnings
    )
    nexus_apply_target_policy(nexus_lwip_managed_services)
    add_library(Nexus::LwIPManagedServices ALIAS nexus_lwip_managed_services)
    add_library(nexus_lwip_managed_services_storage INTERFACE)
    target_link_libraries(
        nexus_lwip_managed_services_storage
        INTERFACE Nexus::LwIPManagedServices Nexus::LwIPManagedStorage
    )
    add_library(
        Nexus::LwIPManagedServicesStorage
        ALIAS nexus_lwip_managed_services_storage
    )
endfunction()
