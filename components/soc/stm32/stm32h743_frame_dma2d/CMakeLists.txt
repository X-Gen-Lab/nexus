# Explicit pinless H743 memory accelerator; unlinked unless a consumer selects it.
# Author: Nexus Team
include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/platform/CMSISCore.cmake")
if(NEXUS_SOC_FAMILY STREQUAL "stm32h743")
    add_library(nexus_h743_frame_dma2d STATIC EXCLUDE_FROM_ALL
        src/h743_frame_dma2d.c)
    target_include_directories(nexus_h743_frame_dma2d PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_include_directories(nexus_h743_frame_dma2d SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include")
    nexus_use_cmsis_core(nexus_h743_frame_dma2d)
    target_compile_definitions(nexus_h743_frame_dma2d PRIVATE STM32H743xx)
    target_link_libraries(nexus_h743_frame_dma2d PUBLIC Nexus::IO Nexus::Core
        PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
    add_library(Nexus::H743FrameDMA2D ALIAS nexus_h743_frame_dma2d)
endif()
