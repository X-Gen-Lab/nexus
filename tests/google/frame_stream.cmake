# Real framing geometry plus exact optional DMA2D provider contracts.
nexus_google_test(nexus_frame_stream_test SOURCES frame_stream_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_h743_frame_dma2d_test
    SOURCES h743_frame_dma2d_test.cpp
        "${NEXUS_SOURCE_DIR}/components/frame_dma2d/src/h743_frame_dma2d.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/components/frame_dma2d/include")
target_include_directories(nexus_h743_frame_dma2d_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_frame_dma2d_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_frame_dma2d_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_frame_dma2d_model.h")
