# Selected upstream filesystem, static caches and no implicit vendor preparation.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_littlefs)
    if(TARGET Nexus::LittleFS)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(littlefs source)
    add_library(
        nexus_littlefs_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/lfs.c"
        "${source}/lfs_util.c"
    )
    nexus_apply_target_policy(nexus_littlefs_upstream)
    target_compile_definitions(
        nexus_littlefs_upstream
        PRIVATE LFS_NO_MALLOC LFS_NO_DEBUG LFS_NO_WARN LFS_NO_ERROR
    )
    target_include_directories(nexus_littlefs_upstream PRIVATE "${source}")
    target_link_libraries(nexus_littlefs_upstream PRIVATE nexus_build_options)
    add_library(
        nexus_littlefs
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/littlefs.c"
    )
    nexus_apply_target_policy(nexus_littlefs)
    target_compile_definitions(
        nexus_littlefs
        PRIVATE LFS_NO_MALLOC LFS_NO_DEBUG LFS_NO_WARN LFS_NO_ERROR
    )
    target_include_directories(nexus_littlefs PUBLIC "${component}/include")
    target_include_directories(nexus_littlefs SYSTEM PRIVATE "${source}")
    target_link_libraries(
        nexus_littlefs
        PUBLIC Nexus::FAL
        PRIVATE nexus_littlefs_upstream nexus_build_options nexus_owned_warnings
    )
    add_library(Nexus::LittleFS ALIAS nexus_littlefs)
endfunction()
