# Exact shared production C; only external register writes are modeled.
nexus_google_test(nexus_uart_v1_channel_dma_test
    SOURCES uart_v1_channel_dma_test.cpp
        "${NEXUS_SOURCE_DIR}/drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/drivers/uart/uart_v1_channel_dma")
target_compile_options(nexus_uart_v1_channel_dma_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/uart_v1_channel_dma_model.h")

foreach(legacy_family stm32f103 gd32f303)
    set(legacy_target "nexus_${legacy_family}_transport_test")
    nexus_google_test(${legacy_target} SOURCES legacy_transport_test.cpp
        "${NEXUS_SOC_SOURCE_${legacy_family}}/drivers/uart_dma.c"
        "${NEXUS_SOC_SOURCE_${legacy_family}}/drivers/i2c_async.c"
        "${NEXUS_SOURCE_DIR}/drivers/uart/uart_v1_channel_dma/uart_v1_channel_dma.c"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v1_async/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch Nexus::NativeModel
        INCLUDES "${NEXUS_SOC_SOURCE_${legacy_family}}/private")
    target_compile_options(${legacy_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/google/legacy_transport_model.h")
    target_include_directories(${legacy_target} SYSTEM PRIVATE
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${legacy_target})
    if(legacy_family STREQUAL "stm32f103")
        target_include_directories(${legacy_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include")
        target_compile_definitions(${legacy_target} PRIVATE
            NX_LEGACY_F103 STM32F103xB __ARM_ARCH_7M__=1 __ARM_FEATURE_LDREX=7)
    else()
        target_include_directories(${legacy_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include"
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include")
        target_compile_definitions(${legacy_target} PRIVATE GD32F30X_HD)
    endif()
endforeach()
