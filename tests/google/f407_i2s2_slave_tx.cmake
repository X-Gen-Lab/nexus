# Real C11 owner, System TIM2 reader and frame/stream loans; MMIO leaves only.
# Author: Nexus Team
nexus_google_test(nexus_f407_i2s2_slave_tx_test
    SOURCES f407_i2s2_slave_tx_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/i2s2_slave_tx.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/system.c"
        "${NEXUS_SOURCE_DIR}/core/src/request.c"
        "${NEXUS_SOURCE_DIR}/core/src/time.c"
        "${NEXUS_SOURCE_DIR}/core/src/fault.c"
        "${NEXUS_SOURCE_DIR}/io/src/dma.c"
        "${NEXUS_SOURCE_DIR}/io/src/stream.c"
        "${NEXUS_SOURCE_DIR}/io/src/frame_stream.c"
    INCLUDES
        "${NEXUS_SOURCE_DIR}/components/f407_i2s2_slave_tx/include"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_f407_i2s2_slave_tx_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_f407_i2s2_slave_tx_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL=1 NX_F407_I2S_FLASH_BYTES=524288)
target_compile_options(nexus_f407_i2s2_slave_tx_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/f407_i2s2_slave_tx_model.h")
