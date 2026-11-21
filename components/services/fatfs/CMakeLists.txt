# Single selected volume, private upstream state and the existing Block owner.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_fatfs)
    if(TARGET Nexus::FatFS)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(fatfs source)
    add_library(
        nexus_fatfs_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/source/ff.c"
    )
    nexus_apply_target_policy(nexus_fatfs_upstream)
    target_include_directories(
        nexus_fatfs_upstream
        PRIVATE "${component}/profile" "${source}/source"
    )
    target_link_libraries(nexus_fatfs_upstream PRIVATE nexus_build_options)
    add_library(nexus_fatfs STATIC EXCLUDE_FROM_ALL "${component}/src/fatfs.c")
    nexus_apply_target_policy(nexus_fatfs)
    target_include_directories(
        nexus_fatfs
        PUBLIC "${component}/include"
        PRIVATE "${component}/profile"
    )
    target_include_directories(nexus_fatfs SYSTEM PRIVATE "${source}/source")
    target_link_libraries(
        nexus_fatfs
        PUBLIC Nexus::IOBlock
        PRIVATE nexus_fatfs_upstream nexus_build_options nexus_owned_warnings
    )
    add_library(Nexus::FatFS ALIAS nexus_fatfs)
endfunction()
