# Selected settled LSM6DSOX profile; SDK remains private.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_lsm6dsox)
    if(TARGET Nexus::LSM6DSOX)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(lsm6dsox source)
    add_library(
        nexus_lsm6dsox_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/lsm6dsox_reg.c"
    )
    nexus_apply_target_policy(nexus_lsm6dsox_upstream)
    target_include_directories(nexus_lsm6dsox_upstream PRIVATE "${source}")
    target_link_libraries(nexus_lsm6dsox_upstream PRIVATE nexus_build_options)
    add_library(
        nexus_lsm6dsox
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/lsm6dsox.c"
    )
    nexus_apply_target_policy(nexus_lsm6dsox)
    target_include_directories(nexus_lsm6dsox PUBLIC "${component}/include")
    target_include_directories(nexus_lsm6dsox SYSTEM PRIVATE "${source}")
    target_link_libraries(
        nexus_lsm6dsox
        PUBLIC Nexus::IO
        PRIVATE nexus_lsm6dsox_upstream nexus_build_options nexus_owned_warnings
    )
    add_library(Nexus::LSM6DSOX ALIAS nexus_lsm6dsox)
    foreach(bus IN ITEMS i2c spi)
        add_library(
            nexus_lsm6dsox_${bus}
            STATIC
            EXCLUDE_FROM_ALL
            "${component}/src/${bus}_transport.c"
        )
        nexus_apply_target_policy(nexus_lsm6dsox_${bus})
        target_link_libraries(
            nexus_lsm6dsox_${bus}
            PUBLIC Nexus::LSM6DSOX
            PRIVATE nexus_build_options nexus_owned_warnings
        )
    endforeach()
    target_link_libraries(nexus_lsm6dsox_i2c PUBLIC Nexus::IOI2C)
    target_link_libraries(nexus_lsm6dsox_spi PUBLIC Nexus::IOSPI)
    add_library(Nexus::LSM6DSOXI2C ALIAS nexus_lsm6dsox_i2c)
    add_library(Nexus::LSM6DSOXSPI ALIAS nexus_lsm6dsox_spi)
endfunction()
