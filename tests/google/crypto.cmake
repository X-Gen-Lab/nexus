# Actual SDK control flow; only FIFO, digest and write-only key MMIO are ports.
nexus_google_test(nexus_crypto_api_test SOURCES crypto_api_test.cpp
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(crypto_contract "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f4_crypto")
    set(crypto_generated "${CMAKE_CURRENT_BINARY_DIR}/crypto-vendor-model")
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${crypto_contract}/vendor_model.py" "${NEXUS_SOURCE_DIR}"
        "${crypto_generated}" RESULT_VARIABLE crypto_result)
    if(NOT crypto_result EQUAL 0)
        message(FATAL_ERROR "Pinned crypto SDK MMIO model generation failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${crypto_contract}/vendor_model.py"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_cryp.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_hash.c")
    add_library(nexus_crypto_dma_hal OBJECT
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_dma.c")
    nexus_stm32f4_official(nexus_crypto_dma_hal)
    target_compile_features(nexus_crypto_dma_hal PRIVATE c_std_11)
    set_target_properties(nexus_crypto_dma_hal PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions(nexus_crypto_dma_hal PRIVATE
        STM32F415xx NEXUS_STM32_MODEL=1 NEXUS_STM32_CRYPTO_DMA=1)
    target_include_directories(nexus_crypto_dma_hal PRIVATE "${crypto_contract}")
    target_include_directories(nexus_crypto_dma_hal SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(nexus_crypto_dma_hal)
    nexus_use_cmsis_host_model(nexus_crypto_dma_hal)
    target_link_libraries(nexus_crypto_dma_hal PRIVATE nexus_build_options)
    target_compile_options(nexus_crypto_dma_hal PRIVATE
        "-include${crypto_contract}/model.h"
        -ffunction-sections -fdata-sections
        -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)
    foreach(kind cipher hash irq)
        if(kind STREQUAL "irq")
            set(driver crypto_irq)
        else()
            set(driver "${kind}")
        endif()
        set(target nexus_stm32_crypto_${kind}_test)
        nexus_google_test(${target} STM32F4_LL
            SOURCES "${crypto_contract}/${kind}_test.cpp"
                "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f407_uart_hal/cpu_model.cpp"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${driver}.c"
            LIBRARIES Nexus::IO
            INCLUDES "${crypto_contract}"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                "${NEXUS_SOURCE_DIR}/io/private"
                "${NEXUS_SOURCE_DIR}/arch/include")
        target_compile_definitions(${target} PRIVATE STM32F415xx NEXUS_STM32_MODEL=1)
        target_compile_options(${target} PRIVATE "-include${crypto_contract}/model.h")
        target_include_directories(${target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        nexus_use_cmsis_core(${target})
        target_link_options(${target} PRIVATE -Wl,--gc-sections)
        if(NOT kind STREQUAL "irq")
            if(kind STREQUAL "cipher")
                set(module cryp)
            else()
                set(module hash)
            endif()
            set(vendor ${target}_hal)
            add_library(${vendor} OBJECT
                "${crypto_generated}/stm32f4xx_hal_${module}.c")
            nexus_stm32f4_official(${vendor})
            target_compile_features(${vendor} PRIVATE c_std_11)
            set_target_properties(${vendor} PROPERTIES C_EXTENSIONS OFF)
            target_compile_definitions(${vendor} PRIVATE STM32F415xx NEXUS_STM32_MODEL=1)
            target_include_directories(${vendor} PRIVATE "${crypto_contract}")
            target_include_directories(${vendor} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            nexus_use_cmsis_core(${vendor})
            nexus_use_cmsis_host_model(${vendor})
            target_link_libraries(${vendor} PRIVATE nexus_build_options)
            target_compile_options(${vendor} PRIVATE
                "-include${crypto_contract}/model.h"
                -ffunction-sections -fdata-sections
                -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)
            target_sources(${target} PRIVATE "$<TARGET_OBJECTS:${vendor}>")
            # Shared fixture, distinct transport assertions; IRQ behavior is
            # executed only by its original target above.
            set(dma_target "nexus_stm32_crypto_${kind}_dma_test")
            string(TOUPPER "${kind}" crypto_kind)
            nexus_google_test(${dma_target} STM32F4_LL
                SOURCES "${crypto_contract}/${kind}_test.cpp"
                    "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f407_uart_hal/cpu_model.cpp"
                    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${kind}.c"
                    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${kind}_dma.c"
                    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/crypto_dma.c"
                LIBRARIES Nexus::IO
                INCLUDES "${crypto_contract}"
                    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                    "${NEXUS_SOURCE_DIR}/io/private"
                    "${NEXUS_SOURCE_DIR}/arch/include")
            target_compile_definitions(${dma_target} PRIVATE
                STM32F415xx NEXUS_STM32_MODEL=1 NEXUS_STM32_CRYPTO_DMA=1
                "NEXUS_STM32_${crypto_kind}_DMA=1")
            target_compile_options(${dma_target} PRIVATE
                "-include${crypto_contract}/model.h")
            target_include_directories(${dma_target} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            nexus_use_cmsis_core(${dma_target})
            target_link_options(${dma_target} PRIVATE -Wl,--gc-sections
                -Wl,--wrap=nx_request_settle)
            target_sources(${dma_target} PRIVATE "$<TARGET_OBJECTS:${vendor}>"
                "$<TARGET_OBJECTS:nexus_crypto_dma_hal>")
        endif()
    endforeach()
endif()
