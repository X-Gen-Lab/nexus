# Selected CANopenNode objects use explicit private state and no CO_new allocator.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_canopen)
    if(TARGET Nexus::CANopen)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(canopen source)
    add_library(
        nexus_canopen_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/301/CO_Emergency.c"
        "${source}/301/CO_HBconsumer.c"
        "${source}/301/CO_NMT_Heartbeat.c"
        "${source}/301/CO_ODinterface.c"
        "${source}/301/CO_PDO.c"
        "${source}/301/CO_SDOserver.c"
    )
    nexus_apply_target_policy(nexus_canopen_upstream)
    target_include_directories(
        nexus_canopen_upstream
        PRIVATE "${component}/profile" "${source}"
    )
    target_link_libraries(
        nexus_canopen_upstream
        PRIVATE Nexus::IOCAN nexus_build_options
    )
    add_library(
        nexus_canopen
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/canopen.c"
    )
    nexus_apply_target_policy(nexus_canopen)
    target_include_directories(nexus_canopen PUBLIC "${component}/include")
    target_include_directories(nexus_canopen PRIVATE "${component}/profile")
    target_include_directories(nexus_canopen SYSTEM PRIVATE "${source}")
    target_link_libraries(
        nexus_canopen
        PUBLIC Nexus::IOCAN
        PRIVATE nexus_canopen_upstream nexus_owned_warnings nexus_build_options
    )
    add_library(Nexus::CANopen ALIAS nexus_canopen)
endfunction()

# The optional drive layer retains its own OD and never changes generic nodes.
function(nexus_add_cia402)
    if(TARGET Nexus::CiA402)
        return()
    endif()
    nexus_add_canopen()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_library(
        nexus_cia402
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/cia402.c"
    )
    nexus_apply_target_policy(nexus_cia402)
    target_include_directories(nexus_cia402 PUBLIC "${component}/include")
    target_link_libraries(
        nexus_cia402
        PUBLIC Nexus::CANopen
        PRIVATE nexus_owned_warnings nexus_build_options
    )
    if(NOT MSVC)
        target_link_libraries(nexus_cia402 PRIVATE m)
    endif()
    add_library(Nexus::CiA402 ALIAS nexus_cia402)
endfunction()
