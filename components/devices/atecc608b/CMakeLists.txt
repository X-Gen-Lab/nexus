# Only the selected Microchip protocol builders/executor enter this profile.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_atecc608b)
    if(TARGET Nexus::ATECC608B)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(cryptoauthlib source)
    add_library(
        nexus_atecc608b_vendor
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/lib/calib/calib_command.c"
        "${source}/lib/calib/calib_execution.c"
        "${source}/lib/atca_debug.c"
    )
    nexus_apply_target_policy(nexus_atecc608b_vendor)
    target_include_directories(
        nexus_atecc608b_vendor
        PRIVATE "${component}/profile" "${source}/lib"
    )
    target_link_libraries(nexus_atecc608b_vendor PRIVATE nexus_build_options)
    add_library(
        nexus_atecc608b
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/atecc608b.c"
    )
    nexus_apply_target_policy(nexus_atecc608b)
    target_include_directories(nexus_atecc608b PUBLIC "${component}/include")
    target_include_directories(nexus_atecc608b PRIVATE "${component}/profile")
    target_include_directories(nexus_atecc608b SYSTEM PRIVATE "${source}/lib")
    target_link_libraries(
        nexus_atecc608b
        PUBLIC Nexus::IOI2C Nexus::IOP256Signer
        PRIVATE nexus_atecc608b_vendor nexus_owned_warnings nexus_build_options
    )
    add_library(Nexus::ATECC608B ALIAS nexus_atecc608b)
endfunction()
