# Shared official transport/MMIO model and two distinguishing provider contexts.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(qspi_contract "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f4_qspi")
    set(qspi_generated "${CMAKE_CURRENT_BINARY_DIR}/qspi-vendor-model")
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${qspi_contract}/vendor_model.py" "${NEXUS_SOURCE_DIR}"
        "${qspi_generated}" RESULT_VARIABLE qspi_result)
    if(NOT qspi_result EQUAL 0)
        message(FATAL_ERROR "Pinned QSPI SDK MMIO model generation failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${qspi_contract}/vendor_model.py"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_qspi.c")
    # Provider contexts retain their distinguishing contracts rather than
    # repeating the complete ordinary PIO/DMA suite in the quad-read variant.
    foreach(qspi_variant ordinary quad mapped)
        set(qspi_test nexus_stm32_qspi_test)
        set(qspi_quad 0)
        set(qspi_map 0)
        if(qspi_variant STREQUAL "quad")
            set(qspi_test nexus_stm32_qspi_quad_test)
            set(qspi_quad 1)
        elseif(qspi_variant STREQUAL "mapped")
            set(qspi_test nexus_stm32_qspi_mapped_test)
            set(qspi_quad 1)
            set(qspi_map 1)
        endif()
        nexus_google_test(${qspi_test} STM32F4_LL
            SOURCES stm32_qspi_test.cpp
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/qspi.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/qspi_dma.c"
            LIBRARIES Nexus::IO
            INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/qspi_hal"
                "${NEXUS_SOURCE_DIR}/arch/include")
        if(qspi_map)
            target_sources(${qspi_test} PRIVATE
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/qspi_mapped.c")
            target_link_libraries(${qspi_test} PRIVATE Nexus::MappedStorage)
        endif()
        target_include_directories(${qspi_test} BEFORE PRIVATE
            "${qspi_contract}/model-config")
        target_compile_definitions(${qspi_test} PRIVATE
            STM32F479xx NEXUS_STM32_QSPI_MODEL=1 NEXUS_QSPI_PROGRAM_DMA=1
            NEXUS_QSPI_QUAD_READ=${qspi_quad} NEXUS_QSPI_CPU_MAP=${qspi_map}
            nx_arch_is_privileged=nx_qspi_privileged
            nx_arch_in_isr=nx_qspi_in_isr
            nx_arch_irq_is_masked=nx_qspi_masked
            nx_arch_dsb=nx_qspi_dsb)
        target_compile_options(${qspi_test} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_qspi_model.h"
            -ffunction-sections -fdata-sections)
        target_include_directories(${qspi_test} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        nexus_use_cmsis_core(${qspi_test})
        target_link_options(${qspi_test} PRIVATE -Wl,--gc-sections)
    endforeach()
    # QUAD_READ changes Nexus command selection, not this official Handle ABI.
    add_library(nexus_stm32_qspi_test_hal OBJECT
        "${qspi_generated}/stm32f4xx_hal_qspi.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_dma.c")
    nexus_stm32f4_official(nexus_stm32_qspi_test_hal)
    target_compile_features(nexus_stm32_qspi_test_hal PRIVATE c_std_11)
    set_target_properties(nexus_stm32_qspi_test_hal PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(nexus_stm32_qspi_test_hal PRIVATE STM32F479xx)
    target_include_directories(nexus_stm32_qspi_test_hal BEFORE PRIVATE
        "${qspi_contract}/model-config"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/qspi_hal"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(nexus_stm32_qspi_test_hal SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(nexus_stm32_qspi_test_hal)
    nexus_use_cmsis_host_model(nexus_stm32_qspi_test_hal)
    target_link_libraries(nexus_stm32_qspi_test_hal PRIVATE nexus_build_options)
    target_compile_options(nexus_stm32_qspi_test_hal PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_qspi_model.h"
        -ffunction-sections -fdata-sections
        -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)
    foreach(qspi_test nexus_stm32_qspi_test nexus_stm32_qspi_quad_test
            nexus_stm32_qspi_mapped_test)
        target_sources(${qspi_test} PRIVATE
            "$<TARGET_OBJECTS:nexus_stm32_qspi_test_hal>")
    endforeach()
endif()
