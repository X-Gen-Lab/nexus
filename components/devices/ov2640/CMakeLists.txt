# Real ST sensor tables, selected with the exact compatible BSP Common header.
# Author: Nexus Team
include_guard(GLOBAL)
function(nexus_add_ov2640)
    if(TARGET Nexus::OV2640)
        return()
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/../../../cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(ov2640 source)
    nexus_component_source(stm32_bsp_common common)
    # Preserve the upstream relative Common header layout using original bytes.
    set(private "${CMAKE_CURRENT_BINARY_DIR}/ov2640-private")
    file(MAKE_DIRECTORY "${private}/Driver" "${private}/Common")
    configure_file("${common}/camera.h" "${private}/Common/camera.h" COPYONLY)
    set(private_callbacks
        CAMERA_IO_Init=nx_ov2640_io_init
        CAMERA_IO_Write=nx_ov2640_io_write
        CAMERA_IO_Read=nx_ov2640_io_read
        CAMERA_Delay=nx_ov2640_delay
    )
    add_library(
        nexus_ov2640_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/ov2640.c"
    )
    nexus_apply_target_policy(nexus_ov2640_upstream)
    target_include_directories(
        nexus_ov2640_upstream
        SYSTEM
        PRIVATE "${source}" "${private}/Driver"
    )
    target_link_libraries(nexus_ov2640_upstream PRIVATE nexus_build_options)
    target_compile_definitions(
        nexus_ov2640_upstream
        PRIVATE ${private_callbacks}
    )
    add_library(
        nexus_ov2640
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/ov2640.c"
    )
    nexus_apply_target_policy(nexus_ov2640)
    target_include_directories(nexus_ov2640 PUBLIC "${component}/include")
    target_include_directories(
        nexus_ov2640
        SYSTEM
        PRIVATE "${source}" "${private}/Driver"
    )
    target_link_libraries(
        nexus_ov2640
        PUBLIC Nexus::Core
        PRIVATE nexus_ov2640_upstream nexus_build_options nexus_owned_warnings
    )
    target_compile_definitions(nexus_ov2640 PRIVATE ${private_callbacks})
    add_library(Nexus::OV2640 ALIAS nexus_ov2640)
endfunction()
