# Secondary finite DMA providers execute their real normal NVIC/MMIO paths.
# The GD host CPU shim is used for both silicon families, without changing SDKs.
set(dma_multi_vendor
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
foreach(dma_multi_family stm32f407 gd32f470)
    set(dma_multi_official)
    if(dma_multi_family STREQUAL "stm32f407")
        set(dma_multi_official STM32F4_LL)
    endif()
    foreach(dma_multi_kind uart spi)
        set(dma_multi_target
            "nexus_${dma_multi_family}_${dma_multi_kind}_multi_controller_test")
        nexus_google_test(${dma_multi_target} ${dma_multi_official}
            SOURCES "${dma_multi_kind}_multi_controller_test.cpp"
                "${NEXUS_SOURCE_DIR}/soc/${dma_multi_family}/drivers/${dma_multi_kind}.c"
                "${NEXUS_SOURCE_DIR}/soc/${dma_multi_family}/drivers/${dma_multi_kind}_dma.c"
                "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
            LIBRARIES Nexus::IO
            INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/dma_multi_controller"
                "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
                "${NEXUS_SOURCE_DIR}/soc/gd32f470"
                "${NEXUS_SOURCE_DIR}/soc/gd32f470/private"
                "${NEXUS_SOURCE_DIR}/soc/${dma_multi_family}/private"
                "${NEXUS_SOURCE_DIR}/arch/include")
        if(dma_multi_kind STREQUAL "spi")
            target_include_directories(${dma_multi_target} PRIVATE
                "${NEXUS_SOURCE_DIR}/io/private")
        endif()
        target_compile_options(${dma_multi_target} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/${dma_multi_kind}_multi_controller_model.h"
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
        target_include_directories(${dma_multi_target} SYSTEM PRIVATE
            "${dma_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${dma_multi_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${dma_multi_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        if(dma_multi_family STREQUAL "stm32f407")
            target_include_directories(${dma_multi_target} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            target_compile_definitions(${dma_multi_target} PRIVATE
                STM32F407xx NX_DMA_MULTI_STM32)
            if(dma_multi_kind STREQUAL "spi")
                target_sources(${dma_multi_target} PRIVATE
                    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/gpio.c")
            endif()
        endif()
    endforeach()
endforeach()
