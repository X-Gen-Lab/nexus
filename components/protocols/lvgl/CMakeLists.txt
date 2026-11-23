# The selected GUI owns one explicit caller arena and no background executor.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_lvgl)
    if(TARGET Nexus::LVGL)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(lvgl source)
    include("${component}/sources.cmake")
    list(TRANSFORM lvgl_sources PREPEND "${source}/")
    add_library(nexus_lvgl_upstream STATIC EXCLUDE_FROM_ALL ${lvgl_sources})
    nexus_apply_target_policy(nexus_lvgl_upstream)
    target_include_directories(
        nexus_lvgl_upstream
        PRIVATE "${component}/profile"
    )
    target_include_directories(nexus_lvgl_upstream SYSTEM PRIVATE "${source}")
    target_compile_definitions(
        nexus_lvgl_upstream
        PRIVATE LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE
    )
    target_link_libraries(nexus_lvgl_upstream PRIVATE nexus_build_options)
    add_library(nexus_lvgl STATIC EXCLUDE_FROM_ALL "${component}/src/lvgl.c")
    nexus_apply_target_policy(nexus_lvgl)
    target_include_directories(
        nexus_lvgl
        PUBLIC "${component}/include"
        PRIVATE "${component}/profile"
    )
    target_include_directories(nexus_lvgl SYSTEM PRIVATE "${source}")
    target_compile_definitions(
        nexus_lvgl
        PRIVATE LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE
    )
    target_link_libraries(
        nexus_lvgl
        PUBLIC Nexus::Core
        PRIVATE nexus_lvgl_upstream nexus_build_options nexus_owned_warnings
    )
    add_library(Nexus::LVGL ALIAS nexus_lvgl)
    add_library(nexus_lvgl_profile INTERFACE)
    target_include_directories(
        nexus_lvgl_profile
        INTERFACE "${component}/profile"
    )
    target_include_directories(nexus_lvgl_profile SYSTEM INTERFACE "${source}")
    target_compile_definitions(
        nexus_lvgl_profile
        INTERFACE LV_CONF_INCLUDE_SIMPLE LV_KCONFIG_IGNORE
    )
    target_link_libraries(nexus_lvgl_profile INTERFACE Nexus::LVGL)
    add_library(Nexus::LVGLProfile ALIAS nexus_lvgl_profile)
endfunction()
