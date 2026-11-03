# Do not include upstream Filelists.cmake: it configures files in the source
# checkout and creates unrelated targets. CMake remains the source graph.
set(nexus_lwip_sources
    core/init.c core/def.c core/inet_chksum.c core/ip.c core/mem.c
    core/memp.c core/netif.c core/pbuf.c core/raw.c core/stats.c core/sys.c
    core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c core/udp.c
    core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/ip4_frag.c core/ipv4/ip4.c
    core/ipv4/ip4_addr.c netif/ethernet.c)
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
    add_library("${vendor}" STATIC EXCLUDE_FROM_ALL ${nexus_lwip_sources})
    nexus_apply_target_policy("${vendor}")
    target_include_directories("${vendor}" SYSTEM PUBLIC
        "${nexus_lwip_source}/src/include")
    target_include_directories("${vendor}" PUBLIC
        "${network_dir}/port")
    target_link_libraries("${vendor}" PUBLIC nexus_abi_options
        PRIVATE nexus_build_options)
    if(loaned)
        target_compile_definitions("${vendor}" PUBLIC
            LWIP_SUPPORT_CUSTOM_PBUF=1 NEXUS_LWIP_LOANED_RX=1)
        set(profile loaned_rx)
    else()
        set(profile copied_rx)
    endif()
    set_property(TARGET "${vendor}" PROPERTY
        INTERFACE_NEXUS_LWIP_RX_PROFILE "${profile}")
    set_property(TARGET "${vendor}" APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING NEXUS_LWIP_RX_PROFILE)

    add_library("${adapter}" STATIC EXCLUDE_FROM_ALL
        "${network_dir}/src/lwip_adapter.c")
    nexus_apply_target_policy("${adapter}")
    target_include_directories("${adapter}" PUBLIC
        "${network_dir}/include")
    target_link_libraries("${adapter}" PUBLIC Nexus::IO
        PRIVATE "${vendor}" nexus_build_options nexus_owned_warnings)
    add_library("Nexus::${adapter_alias}" ALIAS "${adapter}")

    # Only static composition imports the exact supplier/profile layout.
    add_library("${storage}" INTERFACE)
    target_link_libraries("${storage}" INTERFACE "${adapter}" "${vendor}")
    add_library("Nexus::${adapter_alias}Storage" ALIAS "${storage}")

    # TCP uses the same source and the selected adapter's exact layout.
    add_library("${tcp}" STATIC EXCLUDE_FROM_ALL
        "${network_dir}/src/lwip_tcp.c")
    nexus_apply_target_policy("${tcp}")
    target_include_directories("${tcp}" PUBLIC
        "${network_dir}/include")
    target_link_libraries("${tcp}" PUBLIC Nexus::IO
        PRIVATE "${adapter}" "${vendor}"
            nexus_build_options nexus_owned_warnings)
    add_library("Nexus::${tcp_alias}" ALIAS "${tcp}")
    add_library("${tcp_storage}" INTERFACE)
    target_link_libraries("${tcp_storage}" INTERFACE "${tcp}" "${vendor}")
    add_library("Nexus::${tcp_alias}Storage" ALIAS "${tcp_storage}")
    # PRIVATE vendor links do not propagate a consumer's compatibility label.
    # Carry the selected ABI fact through ordinary adapter/TCP public targets.
    foreach(profile_target IN ITEMS "${adapter}" "${tcp}")
        set_property(TARGET "${profile_target}" PROPERTY
            INTERFACE_NEXUS_LWIP_RX_PROFILE "${profile}")
        set_property(TARGET "${profile_target}" APPEND PROPERTY
            COMPATIBLE_INTERFACE_STRING NEXUS_LWIP_RX_PROFILE)
    endforeach()
    if(loaned)
        target_compile_definitions("${adapter}" PRIVATE NEXUS_LWIP_LOANED_RX=1)
        target_compile_definitions("${tcp}" PRIVATE NEXUS_LWIP_LOANED_RX=1)
        target_compile_definitions("${storage}" INTERFACE
            NEXUS_LWIP_LOANED_RX=1)
        target_compile_definitions("${tcp_storage}" INTERFACE
            NEXUS_LWIP_LOANED_RX=1)
    endif()
endfunction()
