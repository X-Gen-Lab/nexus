# Explicit selected NOR algorithms; no default discovery, reset or vendor work.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_sfud)
    if(TARGET Nexus::SFUD)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(sfud source)
    add_library(
        nexus_sfud_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/sfud/src/sfud.c"
    )
    nexus_apply_target_policy(nexus_sfud_upstream)
    target_include_directories(
        nexus_sfud_upstream
        PRIVATE "${component}/profile" "${source}/sfud/inc"
    )
    target_link_libraries(nexus_sfud_upstream PRIVATE nexus_build_options)
    add_library(nexus_sfud STATIC EXCLUDE_FROM_ALL "${component}/src/sfud.c")
    nexus_apply_target_policy(nexus_sfud)
    target_include_directories(
        nexus_sfud
        PUBLIC "${component}/include"
        PRIVATE "${component}/profile"
    )
    target_include_directories(nexus_sfud SYSTEM PRIVATE "${source}/sfud/inc")
    target_link_libraries(
        nexus_sfud
        PUBLIC Nexus::IOSPI Nexus::IOFlash
        PRIVATE nexus_sfud_upstream nexus_build_options nexus_owned_warnings
    )
    add_library(Nexus::SFUD ALIAS nexus_sfud)
endfunction()
