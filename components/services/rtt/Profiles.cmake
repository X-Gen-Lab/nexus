# Exact upstream implementation with a private static configuration.
# Author: Nexus Team
function(nexus_add_rtt)
    if(TARGET nexus_rtt)
        return()
    endif()
    set(rtt_leaf "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(rtt_root "${rtt_leaf}/../../.." ABSOLUTE)
    include("${rtt_root}/cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(segger_rtt rtt_source)
    add_library(
        nexus_segger_rtt
        STATIC
        EXCLUDE_FROM_ALL
        "${rtt_source}/RTT/SEGGER_RTT.c"
    )
    target_include_directories(
        nexus_segger_rtt
        PRIVATE "${rtt_leaf}/profile" "${rtt_source}/RTT"
    )
    target_link_libraries(
        nexus_segger_rtt
        PRIVATE nexus_arch nexus_build_options
    )
    add_library(nexus_rtt STATIC EXCLUDE_FROM_ALL "${rtt_leaf}/src/rtt.c")
    target_include_directories(
        nexus_rtt
        PUBLIC "${rtt_leaf}/include"
        PRIVATE "${rtt_leaf}/profile" "${rtt_source}/RTT"
    )
    target_link_libraries(
        nexus_rtt
        PUBLIC nexus_log
        PRIVATE
            nexus_segger_rtt
            nexus_arch
            nexus_owned_warnings
            nexus_build_options
    )
    add_library(Nexus::RTT ALIAS nexus_rtt)
    nexus_apply_target_policy(nexus_segger_rtt)
    nexus_apply_target_policy(nexus_rtt)
endfunction()
