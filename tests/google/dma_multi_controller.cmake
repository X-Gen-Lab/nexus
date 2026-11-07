# Real F4 finite-DMA providers share only an external CPU/time fixture.
foreach(dma_multi_kind uart spi)
    set(dma_multi_target "nexus_stm32f4_${dma_multi_kind}_multi_controller_test")
    nexus_google_test(${dma_multi_target} STM32F4_LL
        SOURCES "${dma_multi_kind}_multi_controller_test.cpp"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${dma_multi_kind}.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${dma_multi_kind}_dma.c"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu/model.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/dma_multi_controller"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    if(dma_multi_kind STREQUAL "spi")
        target_sources(${dma_multi_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c")
        target_include_directories(${dma_multi_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/io/private")
    endif()
    target_compile_options(${dma_multi_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/${dma_multi_kind}_multi_controller_model.h")
    target_include_directories(${dma_multi_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${dma_multi_target} PRIVATE
        STM32F407xx NX_DMA_MULTI_STM32)
endforeach()
