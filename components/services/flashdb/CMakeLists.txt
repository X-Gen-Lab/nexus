# Fixed-profile upstream algorithms are admitted only by explicit selection.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_flashdb)
    if(TARGET Nexus::FlashDB)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(flashdb source)
    add_library(
        nexus_flashdb_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/src/fdb.c"
        "${source}/src/fdb_utils.c"
        "${source}/src/fdb_kvdb.c"
        "${source}/src/fdb_tsdb.c"
    )
    nexus_apply_target_policy(nexus_flashdb_upstream)
    target_include_directories(
        nexus_flashdb_upstream
        PRIVATE "${component}/profile" "${source}/inc" "${source}/port/fal/inc"
    )
    target_link_libraries(nexus_flashdb_upstream PRIVATE nexus_build_options)
    add_library(
        nexus_flashdb
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/flashdb.c"
        "${component}/src/persistent_record.c"
    )
    nexus_apply_target_policy(nexus_flashdb)
    target_include_directories(
        nexus_flashdb
        PUBLIC "${component}/include"
        PRIVATE "${component}/profile"
    )
    target_include_directories(
        nexus_flashdb
        SYSTEM
        PRIVATE "${source}/inc" "${source}/port/fal/inc"
    )
    target_link_libraries(
        nexus_flashdb
        PUBLIC Nexus::FAL
        PRIVATE
            nexus_flashdb_upstream
            Nexus::Algorithms
            nexus_build_options
            nexus_owned_warnings
    )
    add_library(Nexus::FlashDB ALIAS nexus_flashdb)
endfunction()
